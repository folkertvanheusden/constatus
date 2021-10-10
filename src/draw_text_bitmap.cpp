// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <algorithm>
#include <assert.h>
#include <fontconfig/fontconfig.h>
#include "draw_text_bitmap.h"
#include "draw_text_font.h"
#include "utils.h"
#include "log.h"
#include "error.h"

void draw_text_bitmap::draw_bitmap(const FT_Face & face, const std::tuple<UChar32 *, int> & utf_string, const std::tuple<int, int, int, int> & dimensions, const bool use_kerning, const int output_height)
{
	int width = std::get<0>(dimensions);
	int max_ascender = std::get<1>(dimensions);
	int max_descender = std::get<2>(dimensions);
	int height = std::get<3>(dimensions);

	result_bitmap = new uint8_t[width * height * 3]();

	FT_GlyphSlot slot = face->glyph;

	const int utf32_len = std::get<1>(utf_string);
	const UChar32 *const utf32_str = std::get<0>(utf_string);

	uint8_t color_r = col.r, color_g = col.g, color_b = col.b;
	bool invert = false, underline = false;

	double x = 0.0;

	int prev_glyph_index = -1;
	for(int n = 0; n < utf32_len;)
	{
		const int c = utf32_str[n++];

		if (c == '$')
		{
			std::string::size_type eo = n;
			while(utf32_str[eo] != '$' && eo != utf32_len)
				eo++;
			if (eo == utf32_len)
				break;

			const char c2 = utf32_str[n++];

			if (c2 == 'i')
				invert = !invert;

			else if (c2 == 'u')
				underline = !underline;

			else if (c2 == 'C') {
				std::string temp = substr(utf32_str, n, 6);
				hex_str_to_rgb(temp, &color_r, &color_g, &color_b);
			}

			else {
				if (!error_shown) {
					error_shown = true;

					log(LL_WARNING, "One or more text-escapes not understood! ($%c$)", c2);
				}
			}

			n = eo + 1;
			continue;
		}

		int glyph_index = FT_Get_Char_Index(face, c);

		if (use_kerning && prev_glyph_index != -1 && glyph_index)
		{
			FT_Vector akern = { 0, 0 };
			FT_Get_Kerning(face, prev_glyph_index, glyph_index, FT_KERNING_DEFAULT, &akern);
			x += akern.x;
		}

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER))
			continue;

		draw_character(&slot->bitmap, output_height, x / 64.0, max_ascender / 64.0 - slot -> bitmap_top, color_r, color_g, color_b, invert, underline, width, result_bitmap, width, height);

		x += face -> glyph -> metrics.horiAdvance;

		prev_glyph_index = glyph_index;
	}
}

draw_text_bitmap::draw_text_bitmap(const std::string & font_filename, const std::string & text, const int output_height, const bool antialias, const std::optional<rgb_t> bg, const rgb_t col, const bool invert) : 
	draw_text(font_filename, text, output_height, antialias, nullptr, -1, -1, -1, -1, -1, bg, col, invert)
{
	// apparently freetype2 is not thread safe
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	FT_Face face = load_font(font_filename);

	FT_Select_Charmap(face, ft_encoding_unicode);

	if (FT_HAS_COLOR(face))
		printf("Font has colors\n");

	FT_Set_Char_Size(face, output_height * 64, output_height * 64, 72, 72); /* set character size */

	auto utf_string = text_to_utf32(text);

	if (!utf_string.has_value())
		return;

	bool use_kerning = FT_HAS_KERNING(face);
#ifdef DEBUG
	printf("Has kerning: %d\n", use_kerning);
#endif

	auto dimensions = find_text_dimensions(face, utf_string.value(), antialias);

	if (bg.has_value()) {
		int width = std::get<0>(dimensions);

		draw_background(text_w == -1 ? width : text_w, output_height, bg.value());
	}

	draw_bitmap(face, utf_string.value(), dimensions, use_kerning, output_height);

	delete [] std::get<0>(utf_string.value());

	result_w = std::get<0>(dimensions);
	result_h = std::get<3>(dimensions);
}

draw_text_bitmap::~draw_text_bitmap()
{
	delete [] result_bitmap;
}
