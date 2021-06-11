// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <algorithm>
#include <assert.h>
#include <fontconfig/fontconfig.h>
#include "draw_text.h"
#include "draw_text_font.h"
#include "utils.h"
#include "log.h"
#include "error.h"

std::mutex freetype2_lock;
std::mutex fontconfig_lock;

FT_Library draw_text::library;
std::map<std::string, FT_Face> draw_text::font_cache;

void draw_text::draw_character(const FT_Bitmap *const bitmap, const int output_height, const FT_Int x, const FT_Int y, uint8_t r, uint8_t g, uint8_t b, const bool invert, const bool underline, const int right_x, uint8_t *const dest, const int dest_width, const int dest_height)
{
	const int bytes = dest_width * dest_height * 3;

	int startx = 0;
	if (x < 0)
		startx = -x;

	int endx = bitmap->width;
	if (endx + x > right_x)
		endx = right_x - x;

	for(unsigned int yo=0; yo<bitmap->rows; yo++) {
		int yu = yo + y;

		if (yu < 0)
			continue;

		if (yu >= dest_height)
			break;

		for(int xo=startx; xo<endx; xo++) {
			int xu = xo + x;

			int o = yu * dest_width * 3 + xu * 3;

			int pixel_v = bitmap->buffer[yo * bitmap->width + xo];

			if (!invert)
				pixel_v = 255 - pixel_v;

			int sub = 255 - pixel_v;

			dest[o + 0] = (sub * r + pixel_v * dest[o + 0]) >> 8;
			dest[o + 1] = (sub * g + pixel_v * dest[o + 1]) >> 8;
			dest[o + 2] = (sub * b + pixel_v * dest[o + 2]) >> 8;
		}
	}

	if (underline) {
		int pixel_v = invert ? 0 : 255;

		int u_height = std::max(1, result_h / 20);

		for(int yo=0; yo<u_height; yo++) {
			for(unsigned int xo=0; xo<bitmap->width; xo++) {
				int xu = xo + x;

				if (xu >= right_x)
					break;

				int o = (y + result_h - (1 + yo)) * dest_width * 3 + xu * 3;

				if (o + 2 >= bytes)
					continue;

				dest[o + 0] = (pixel_v * r) >> 8;
				dest[o + 1] = (pixel_v * g) >> 8;
				dest[o + 2] = (pixel_v * b) >> 8;
			}
		}
	}
}

void draw_text::draw_background(const int width, const int height, const rgb_t & col)
{
	for(int y=target_y; y<std::min(target_height, target_y + height); y++) {
		int o = y * target_width * 3 + target_x * 3;

		for(int x=target_x; x<std::min(target_width, target_y + width); x++, o += 3) {
			target[o + 0] = col.r;
			target[o + 1] = col.g;
			target[o + 2] = col.b;
		}
	}
}

void draw_text::init_fonts()
{
	FT_Init_FreeType(&draw_text::library);
}

void draw_text::uninit_fonts()
{
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	std::map<std::string, FT_Face>::iterator it = font_cache.begin();

	while(it != font_cache.end()) {
		FT_Done_Face(it -> second);
		it++;
	}

	FT_Done_FreeType(draw_text::library);
}

std::string substr(const UChar32 *const utf32_str, const int idx, const int n)
{
	std::string out;

	for(int i=idx; i<idx+n; i++)
		out += utf32_str[i];

	return out;
}

std::optional<std::tuple<UChar32 *, int> > draw_text::text_to_utf32(const std::string & text)
{
	UChar32 *utf32_str = NULL;
	int utf32_len = 0;

	// FreeType uses Unicode as glyph index; so we have to convert string from UTF8 to Unicode(UTF32)
	int utf16_buf_size = text.size() + 1; // +1 for the last '\0'
	UChar *utf16_str = new UChar[utf16_buf_size];
	UErrorCode err = U_ZERO_ERROR;
	int utf16_length;
	int32_t numSub = 0;
	u_strFromUTF8WithSub(utf16_str, utf16_buf_size, &utf16_length, text.c_str(), text.size(), 0xFFFD, &numSub, &err);
	if (err != U_ZERO_ERROR) {
		log(LL_DEBUG, "u_strFromUTF8() failed: %s", u_errorName(err));
		delete [] utf16_str;
		return { };
	}

	utf32_len = utf16_length;
	int utf32_buf_size = utf16_length + 1; // +1 for the last '\0'
	utf32_str = new UChar32[utf32_buf_size];
	int utf32_length;
	u_strToUTF32(utf32_str, utf32_buf_size, &utf32_length, utf16_str, utf16_length, &err);
	if (err != U_ZERO_ERROR) {
		log(LL_DEBUG, "u_strToUTF32() failed: %s", u_errorName(err));
		delete [] utf16_str;
		delete [] utf32_str;
		return { };
	}

	delete [] utf16_str;

	return std::make_tuple(utf32_str, utf32_len);
}

FT_Face draw_text::load_font(const std::string & font_filename)
{
	FT_Face face = nullptr;

	auto it = font_cache.find(font_filename);

	if (it == font_cache.end()) {
		if (font_filename.empty()) {
			int rc = FT_New_Memory_Face(library, cruft_ttf, cruft_ttf_len, 0, &face);
			if (rc)
				error_exit(false, "cannot load font from memory: %x", rc);
		}
		else {
			int rc = FT_New_Face(library, font_filename.c_str(), 0, &face);
			if (rc)
				error_exit(false, "cannot open font file %s: %x", font_filename.c_str(), rc);
		}

		font_cache.insert(std::pair<std::string, FT_Face>(font_filename, face));
	}
	else
	{
		face = it -> second;
	}

	return face;
}

std::tuple<int, int, int, int> draw_text::find_text_dimensions(const FT_Face & face, const std::tuple<UChar32 *, int> & utf_string, const bool anti_alias)
{
	const int utf32_len = std::get<1>(utf_string);
	const UChar32 *const utf32_str = std::get<0>(utf_string);

	int max_descender = 0, width = 0, max_ascender = 0;

	for(unsigned int n = 0; n < utf32_len;)
	{
		int c = utf32_str[n];

		if (c == '$')
		{
			std::string::size_type eo = n + 1;
			while(utf32_str[eo] != '$' && eo != utf32_len)
				eo++;
			if (eo == utf32_len)
				break;

			n = eo + 1;
			continue;
		}

		int glyph_index = FT_Get_Char_Index(face, c);

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | (anti_alias ? 0 : FT_LOAD_MONOCHROME))) {
			n++;
			continue;
		}

		width += face -> glyph -> metrics.horiAdvance;

		max_ascender = std::max(max_ascender, int(face -> glyph -> metrics.horiBearingY));
		max_descender = std::max(max_descender, int(face -> glyph -> metrics.height - face -> glyph -> metrics.horiBearingY));

		n++;
	}

	int result_h = (max_ascender + max_descender) / 64;

	return std::make_tuple(width / 64, max_ascender, max_descender, result_h);
}

void draw_text::draw_bitmap(const FT_Face & face, const std::tuple<UChar32 *, int> & utf_string, const std::tuple<int, int, int, int> & dimensions, const bool use_kerning, const int output_height, const bool inverse)
{
	int width = std::get<0>(dimensions);
	int max_ascender = std::get<1>(dimensions);
	int max_descender = std::get<2>(dimensions);
	int result_h = std::get<3>(dimensions);

	FT_GlyphSlot slot = face->glyph;

	const int utf32_len = std::get<1>(utf_string);
	const UChar32 *const utf32_str = std::get<0>(utf_string);

	uint8_t color_r = col.r, color_g = col.g, color_b = col.b;
	bool invert = inverse, underline = false;

	int use_target_x = std::max(0, target_x == -1 ? target_width / 2 - width / 2 : target_x);

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

		draw_character(&slot->bitmap, output_height, use_target_x + x / 64.0, target_y + max_ascender / 64.0 - slot -> bitmap_top, color_r, color_g, color_b, invert, underline, std::min(target_width, use_target_x + text_w), target, target_width, target_height);

		x += face -> glyph -> metrics.horiAdvance;

		prev_glyph_index = glyph_index;
	}
}

draw_text::draw_text(const std::string & font_filename, const std::string & text, const int output_height, const bool antialias, uint8_t *const target, const int target_width, const int target_height, const int target_x, const int target_y, const int text_w, const std::optional<rgb_t> bg, const rgb_t col, const bool inverse) : target(target), target_width(target_width), target_height(target_height), target_x(target_x), target_y(target_y), text_w(text_w), col(col)
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

	draw_bitmap(face, utf_string.value(), dimensions, use_kerning, output_height, inverse);

	delete [] std::get<0>(utf_string.value());

	result_w = std::get<0>(dimensions);
	result_h = std::get<3>(dimensions);
}

draw_text::~draw_text()
{
}

// from http://stackoverflow.com/questions/10542832/how-to-use-fontconfig-to-get-font-list-c-c
std::string find_font_by_name(const std::string & font_name, const std::string & default_font_file)
{
	std::string fontFile = default_font_file;

	const std::lock_guard<std::mutex> lock(fontconfig_lock);

	FcConfig* config = FcInitLoadConfigAndFonts();

	// configure the search pattern, 
	// assume "name" is a std::string with the desired font name in it
	FcPattern* pat = FcNameParse((const FcChar8*)(font_name.c_str()));

	if (pat)
	{
		if (FcConfigSubstitute(config, pat, FcMatchPattern))
		{
			FcDefaultSubstitute(pat);

			// find the font
			FcResult result = FcResultNoMatch;
			FcPattern* font = FcFontMatch(config, pat, &result);
			if (font)
			{
				FcChar8* file = NULL;
				if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file != NULL)
				{
					// save the file to another std::string
					fontFile = (const char *)file;
				}

				FcPatternDestroy(font);
			}
		}

		FcPatternDestroy(pat);
	}

	return fontFile;
}
