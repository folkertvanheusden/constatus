// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include <mutex>
#include <string>
#include <fontconfig/fontconfig.h>

#include "draw_text.h"
#include "draw_text_font.h"
#include "error.h"
#include "log.h"
#include "utils.h"


FT_Library draw_text::library;

std::mutex freetype2_lock;
std::mutex fontconfig_lock;

draw_text::draw_text(const std::string & font_file, const int font_height) : font_height(font_height)
{
	// freetype2 is not thread safe
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	if (font_file.empty()) {
		int rc = FT_New_Memory_Face(library, cruft_ttf, cruft_ttf_len, 0, &face);
		if (rc) 
			error_exit(false, "cannot load font from memory: %x", rc);
	}
	else {
		int rc = FT_New_Face(library, font_file.c_str(), 0, &face);
		if (rc)
			error_exit(false, "cannot open font file %s: %x", font_file.c_str(), rc);
	}

	FT_Select_Charmap(face, ft_encoding_unicode);

	FT_Set_Char_Size(face, font_height * 64, font_height * 64, 72, 72);
}

draw_text::~draw_text()
{
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	FT_Done_Face(face);
}

void draw_text::init()
{
	FT_Init_FreeType(&draw_text::library);
}

void draw_text::uninit()
{
	FT_Done_FreeType(draw_text::library);
}

void draw_text::draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int height, const FT_Int x, const FT_Int y, const bool invert, const bool underline, uint8_t *const dest, const int dest_width, const int dest_height)
{
	int startx = x < 0 ? -x : 0;

	for(unsigned int yo=0; yo<bitmap->rows; yo++) {
		int yu = yo + y;

		if (yu < 0)
			continue;

		if (yu >= dest_height)
			break;

		for(unsigned int xo=0; xo<bitmap->width; xo++) {
			int xu = xo + x;

			if (xu >= dest_width)
				break;

			int o = yu * dest_width + xu;

			int pixel_v = bitmap->buffer[yo * bitmap->width + xo];

			if (invert)
				dest[o] = 255 - pixel_v;
			else
				dest[o] = pixel_v;
		}
	}

	if (underline) {
		int pixel_v = invert ? 0 : 255;

		for(int yo=0; yo<2; yo++) {
			int y_offset = (y + bitmap->rows - 1 - yo) * dest_width;

			for(unsigned int xo=0; xo<bitmap->width; xo++) {
				int xu = xo + x;

				if (xu >= dest_width)
					break;

				int o = y_offset + xu;

				dest[o] = 255;
			}
		}
	}
}

int draw_text::draw_glyph(const UChar32 utf_character, const int output_height, const bool invert, const bool underline, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height)
{
	FT_Select_Charmap(face, ft_encoding_unicode);

	int glyph_index   = FT_Get_Char_Index(face, utf_character);

	if (glyph_index == 0)
		return 0;

	if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER))
		return 0;

	FT_GlyphSlot slot = face->glyph;

	int      ascender = int(face -> glyph -> metrics.horiBearingY);

	int        draw_x = x;

	int        draw_y = y - slot->bitmap_top;

	draw_glyph_bitmap(&slot->bitmap, output_height, draw_x, draw_y, invert, underline, dest, dest_width, dest_height);

	return slot->metrics.horiAdvance;
}

std::tuple<int, int, int, int> draw_text::find_text_dimensions(const FT_Face & face, const std::vector<text_with_attributes_t> & utf_string)
{
	int    max_descender = 0;
	int    width         = 0;
	int    max_ascender  = 0;

	size_t str_len       = utf_string.size();

	for(int n = 0; n < str_len;) {
		int c = utf_string.at(n).c;

		int glyph_index = FT_Get_Char_Index(face, c);

		if (FT_Load_Glyph(face, glyph_index, 0) == 0) {
			width        += face -> glyph -> metrics.horiAdvance;

			max_ascender  = std::max(max_ascender,  int(face -> glyph -> metrics.horiBearingY));
			max_descender = std::max(max_descender, int(face -> glyph -> metrics.height - face -> glyph -> metrics.horiBearingY));
		}

		n++;
	}

	int height = (max_ascender + max_descender) / 64;

	width /= 64;

	return std::make_tuple(width, max_ascender, max_descender, height);
}

std::optional<std::tuple<UChar32 *, int> > draw_text::text_to_utf32(const std::string & text)
{
	// FreeType uses Unicode as glyph index; so we have to convert string from UTF8 to Unicode(UTF32)
	int        utf16_buf_size = text.size() + 1;  // +1 for the last '\0'
	UChar     *utf16_str      = new UChar[utf16_buf_size]();
	UErrorCode err            = U_ZERO_ERROR;
	int        utf16_length   = 0;
	int32_t    numSub         = 0;

	u_strFromUTF8WithSub(utf16_str, utf16_buf_size, &utf16_length, text.c_str(), text.size(), 0xfffd, &numSub, &err);

	if (err != U_ZERO_ERROR) {
		log(LL_DEBUG, "u_strFromUTF8() failed: %s", u_errorName(err));

		delete [] utf16_str;

		return { };
	}

	int      utf32_len    = utf16_length;
	int utf32_buf_size    = utf16_length + 1;  // +1 for the last '\0'

	UChar32 *utf32_str    = new UChar32[utf32_buf_size]();
	int      utf32_length = 0;

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

std::vector<text_with_attributes_t> draw_text::preprocess_text(const std::string & input, const rgb_t & fg_color, const std::optional<rgb_t> & bg_color)
{
	auto utf_string = text_to_utf32(input);

	if (!utf_string.has_value())
		return { };

	std::vector<text_with_attributes_t> out;

	std::optional<rgb_t> cur_bg_color = bg_color;
	rgb_t  cur_fg_color = fg_color;

	size_t input_len    = std::get<1>(utf_string.value());

	bool   invert       = false;
	bool   underline    = false;

	for(size_t i=0; i<input_len; i++) {
		if (input.at(i) == '$') {
			size_t escape_end = i + 1;

			while(escape_end < input_len && input.at(escape_end) != '$')
				escape_end++;

			if (escape_end == input_len)  // closing '$' missing
				break;

			int escape = input.at(i + 1);

			if (escape == '$') {  // $$ => $
				out.push_back({ '$', invert, underline, cur_fg_color, cur_bg_color });
			}
			else if (escape == 'i') {  // invert
				invert = true;
			}
			else if (escape == 'u') {  // underline
				invert = true;
			}
			else if (escape == 'C') {  // foreground color
                                std::string temp = substr(std::get<0>(utf_string.value()), i + 2, 6);

                                hex_str_to_rgb(temp, &cur_fg_color.r, &cur_fg_color.g, &cur_fg_color.b);
			}
			else if (escape == 'B') {  // background color
                                std::string temp = substr(std::get<0>(utf_string.value()), i + 2, 6);

				rgb_t       temp_color { 0 };

                                hex_str_to_rgb(temp, &temp_color.r, &temp_color.g, &temp_color.b);

				cur_bg_color = temp_color;
			}

			i = escape_end;
		}
		else {
			out.push_back({ input.at(i), invert, underline, cur_fg_color, cur_bg_color });
		}
	}

	return out;
}

void draw_text::draw_string(const std::string & input, const int height, uint8_t **const grayscale_pixels, int *const width)
{
	auto utf_string = preprocess_text(input, { 255, 255, 255 }, { });

	auto dimensions = find_text_dimensions(face, utf_string);

	*width          = std::get<0>(dimensions);

	*grayscale_pixels = reinterpret_cast<uint8_t *>(malloc(*width * height));
	memset(*grayscale_pixels, 0x00, *width * height);

	int  n_chars    = utf_string.size();

	int  x          = 0;

	int ascender    = std::get<1>(dimensions) / 64;

	for(int i=0; i<n_chars; i++) {
		auto utf_char = utf_string.at(i);

		int w = draw_glyph(utf_char.c, height, utf_char.invert, utf_char.underline, x, ascender, *grayscale_pixels, *width, height);

		x += w / 64;
	}
}

void draw_text_on_bitmap(draw_text *const font, const std::string & in, const int dest_bitmap_width, const int dest_bitmap_height, uint8_t *const dest_bitmap, const int text_height, const rgb_t & fg, const std::optional<const rgb_t> & bg, const int put_x_in, const int put_y)
{
	uint8_t *bitmap       { nullptr };
	int      bitmap_width { 0       };

	font->draw_string(in, text_height, &bitmap, &bitmap_width);

	int      put_x = put_x_in >= 0 ? put_x_in : dest_bitmap_width / 2 - bitmap_width / 2;

	if (bg.has_value()) {
		rgb_t cbg = bg.value();

		for(int cy=0; cy<text_height; cy++) {
			int offset = (put_y + cy) * dest_bitmap_width * 3 + put_x * 3;

			if (put_y + cy >= dest_bitmap_height)
				break;

			for(int cx=0; cx<bitmap_width; cx++) {
				if (put_x + cx >= dest_bitmap_width)
					break;

				int     current_offset = offset + cx * 3;

				int     bitmap_offset  = cy * bitmap_width + cx;

				uint8_t pixel_value    = bitmap[bitmap_offset];
				uint8_t inverted_pixel = 255 - pixel_value;

				dest_bitmap[current_offset + 0] = (fg.r * pixel_value + cbg.r * inverted_pixel) / 255;
				dest_bitmap[current_offset + 1] = (fg.g * pixel_value + cbg.g * inverted_pixel) / 255;
				dest_bitmap[current_offset + 2] = (fg.b * pixel_value + cbg.b * inverted_pixel) / 255;
			}
		}
	}
	else {
		for(int cy=0; cy<text_height; cy++) {
			int offset = (put_y + cy) * dest_bitmap_width * 3 + put_x * 3;

			if (put_y + cy >= dest_bitmap_height)
				break;

			for(int cx=0; cx<bitmap_width; cx++) {
				if (put_x + cx >= dest_bitmap_width)
					break;

				int current_offset     = offset + cx * 3;

				int bitmap_offset      = cy * bitmap_width + cx;

				uint8_t pixel_value    = bitmap[bitmap_offset];
				uint8_t inverted_pixel = 255 - pixel_value;

				dest_bitmap[current_offset + 0] = (fg.r * pixel_value + dest_bitmap[current_offset + 0] * inverted_pixel) / 255;
				dest_bitmap[current_offset + 1] = (fg.g * pixel_value + dest_bitmap[current_offset + 1] * inverted_pixel) / 255;
				dest_bitmap[current_offset + 2] = (fg.b * pixel_value + dest_bitmap[current_offset + 2] * inverted_pixel) / 255;
			}
		}
	}

	free(bitmap);
}
