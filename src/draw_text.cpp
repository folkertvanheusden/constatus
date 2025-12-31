// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include <mutex>
#include <string>
#include <fontconfig/fontconfig.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftglyph.h>

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
	init({ font_file });
}

draw_text::draw_text(const std::vector<std::string> & font_files, const int font_height) : font_height(font_height)
{
	init(font_files);
}

void draw_text::init(const std::vector<std::string> & font_files)
{
	FT_Init_FreeType(&draw_text::library);

	// freetype2 is not thread safe
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	if (font_files.empty() == false && font_files.at(0).empty() == false) {
		for(auto & cur_font_file : font_files) {
			log(LL_INFO, "Loading font file %s", cur_font_file.c_str());

			FT_Face face { 0 };
			int rc = FT_New_Face(library, cur_font_file.c_str(), 0, &face);
			if (rc)
				error_exit(false, "cannot open font file %s: %x", cur_font_file.c_str(), rc);

			FT_Select_Charmap(face, ft_encoding_unicode);

			if (FT_Set_Char_Size(face, 0, font_height * 64, 72, 72))
				FT_Select_Size(face, 0);

			faces.push_back(face);
		}
	}
	else {
		FT_Face face { 0 };
		FT_New_Memory_Face(library, cruft_ttf, cruft_ttf_len, 0, &face);
		faces.push_back(face);
	}

	glyph_cache.resize(faces.size());
	glyph_cache_italic.resize(faces.size());

	// font '0' (first font) must contain all basic characters
	// determine dimensions of character set
	int temp_width  = 0;
	int glyph_index = FT_Get_Char_Index(faces.at(0), 'm');
	for(UChar32 c = 33; c < 127; c++) {
		int glyph_index = FT_Get_Char_Index(faces.at(0), c);

		if (FT_Load_Glyph(faces.at(0), glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_COLOR) == 0) {
			temp_width   = std::max(temp_width  , int(faces.at(0)->glyph->metrics.horiAdvance) / 64);  // width should be all the same!
			max_ascender = std::max(max_ascender, int(faces.at(0)->glyph->metrics.horiBearingY));
		}
	}

	if (this->font_height == 0)
		this->font_height = max_ascender / 64;

	font_width = temp_width;
}

draw_text::~draw_text()
{
	const std::lock_guard<std::mutex> lock(freetype2_lock);

	for(auto & face: glyph_cache) {
		for(auto & element: face)
			FT_Bitmap_Done(library, &element.second.bitmap);
	}

	for(auto & face: glyph_cache_italic) {
		for(auto & element: face)
			FT_Bitmap_Done(library, &element.second.bitmap);
	}

	for(auto f : faces)
		FT_Done_Face(f);

	FT_Done_FreeType(draw_text::library);
}

void draw_text::init()
{
	FT_Init_FreeType(&draw_text::library);
}

void draw_text::uninit()
{
	FT_Done_FreeType(draw_text::library);
}

int draw_text::get_intensity_multiplier(const intensity_t i)
{
	if (i == intensity_t::I_DIM)
		return 145;

	if (i == intensity_t::I_BOLD)
		return 255;

	return 200;
}

void draw_text::draw_glyph_bitmap_low(const FT_Bitmap *const bitmap, const rgb_t & fg, const rgb_t & bg, const bool has_color, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t **const result, int *const result_width, int *const result_height)
{
	const uint8_t max = get_intensity_multiplier(intensity);

	*result_height = bitmap->rows;

	if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
		*result_width = bitmap->width;
		*result       = new uint8_t[*result_height * *result_width * 3]();

		for(unsigned int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;

			// assuming width is always multiple of 8
			for(unsigned glyph_x=0; glyph_x<bitmap->width / 8; glyph_x++) {
				int     io = glyph_y * bitmap->width / 8 + glyph_x;
				uint8_t b  = bitmap->buffer[io];

				int screen_buffer_offset = screen_y * *result_width * 3 + glyph_x * 3 + glyph_x * 8;

				for(int xbit=0; xbit < 8; xbit++) {
					int pixel_v = b & 128 ? max : 0;

					b <<= 1;

					if (invert)
						pixel_v = max - pixel_v;

					int sub = max - pixel_v;

					if (screen_buffer_offset >= 0) {
						(*result)[screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
						(*result)[screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
						(*result)[screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
					}

					screen_buffer_offset += 3;
				}
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_GRAY) {
		*result_width = bitmap->width;
		*result       = new uint8_t[*result_height * *result_width * 3]();

		for(int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;
			int screen_buffer_offset = screen_y * *result_width * 3;
			int io_base = glyph_y * bitmap->width;

			for(int glyph_x=0; glyph_x<bitmap->width; glyph_x++) {
				int screen_x = glyph_x;
				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;
				int io = io_base + glyph_x;

				int pixel_v = bitmap->buffer[io] * max / 255;

				if (invert)
					pixel_v = max - pixel_v;

				int sub = max - pixel_v;

				(*result)[local_screen_buffer_offset + 0] = (pixel_v * fg.r + sub * bg.r) >> 8;
				(*result)[local_screen_buffer_offset + 1] = (pixel_v * fg.g + sub * bg.g) >> 8;
				(*result)[local_screen_buffer_offset + 2] = (pixel_v * fg.b + sub * bg.b) >> 8;
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_LCD) {
		*result_width = bitmap->width / 3;
		*result       = new uint8_t[*result_width * *result_height * 3]();

		for(int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;
			int screen_buffer_offset = screen_y * *result_width * 3;
			int io_base = glyph_y * bitmap->pitch;

			for(int glyph_x=0; glyph_x<*result_width; glyph_x++) {
				int screen_x = glyph_x;
				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;

				int io = io_base + glyph_x * 3;

				int pixel_vr = bitmap->buffer[io + 0] * max / 255;
				int pixel_vg = bitmap->buffer[io + 1] * max / 255;
				int pixel_vb = bitmap->buffer[io + 2] * max / 255;

				if (invert) {
					pixel_vr = max - pixel_vr;
					pixel_vg = max - pixel_vg;
					pixel_vb = max - pixel_vb;
				}

				if (has_color) {
					(*result)[local_screen_buffer_offset + 0] = pixel_vr;
					(*result)[local_screen_buffer_offset + 1] = pixel_vg;
					(*result)[local_screen_buffer_offset + 2] = pixel_vb;
				}
				else {
					int sub = max - pixel_vr;
					(*result)[local_screen_buffer_offset + 0] = (pixel_vr * fg.r + sub * bg.r) >> 8;
					(*result)[local_screen_buffer_offset + 1] = (pixel_vr * fg.g + sub * bg.g) >> 8;
					(*result)[local_screen_buffer_offset + 2] = (pixel_vr * fg.b + sub * bg.b) >> 8;
				}
			}
		}
	}
	else if (bitmap->pixel_mode == FT_PIXEL_MODE_BGRA) {
		*result_width = bitmap->width;
		*result       = new uint8_t[*result_width * *result_height * 3]();

		for(int glyph_y=0; glyph_y<bitmap->rows; glyph_y++) {
			int screen_y = glyph_y;
			int screen_buffer_offset = screen_y * *result_width * 3;
			int io_base = glyph_y * bitmap->pitch;

			for(int glyph_x=0; glyph_x<*result_width; glyph_x++) {
				int screen_x = glyph_x;
				int local_screen_buffer_offset = screen_buffer_offset + screen_x * 3;

				int io = io_base + glyph_x * 4;

				int pixel_vr = bitmap->buffer[io + 2] * max / 255;
				int pixel_vg = bitmap->buffer[io + 1] * max / 255;
				int pixel_vb = bitmap->buffer[io + 0] * max / 255;

				if (invert) {
					pixel_vr = max - pixel_vr;
					pixel_vg = max - pixel_vg;
					pixel_vb = max - pixel_vb;
				}

				if (has_color) {
					(*result)[local_screen_buffer_offset + 0] = pixel_vr;
					(*result)[local_screen_buffer_offset + 1] = pixel_vg;
					(*result)[local_screen_buffer_offset + 2] = pixel_vb;
				}
				else {
					int sub = max - pixel_vr;
					(*result)[local_screen_buffer_offset + 0] = (pixel_vr * fg.r + sub * bg.r) >> 8;
					(*result)[local_screen_buffer_offset + 1] = (pixel_vr * fg.g + sub * bg.g) >> 8;
					(*result)[local_screen_buffer_offset + 2] = (pixel_vr * fg.b + sub * bg.b) >> 8;
				}
			}
		}
	}
	else {
		if (render_mode_error == false) {
			render_mode_error = true;
			log(LL_WARNING, "PIXEL MODE %d NOT IMPLEMENTED", bitmap->pixel_mode);
		}
	}

	if (strikethrough) {
		const int middle_line = *result_height / 2;
		const int offset      = middle_line * *result_width * 3;

		for(unsigned glyph_x=0; glyph_x<*result_width; glyph_x++) {
			int screen_buffer_offset = offset + glyph_x * 3;

			if (screen_buffer_offset >= 0) {
				(*result)[screen_buffer_offset + 0] = (max * fg.r) >> 8;
				(*result)[screen_buffer_offset + 1] = (max * fg.g) >> 8;
				(*result)[screen_buffer_offset + 2] = (max * fg.b) >> 8;
			}
		}
	}

	if (underline) {
		int pixel_v = invert ? 0 : max;

		for(unsigned int glyph_x=0; glyph_x<*result_width; glyph_x++) {
			int screen_x = glyph_x;

			if (screen_x >= *result_width)
				break;

			int screen_buffer_offset = (*result_height - 2) * *result_width * 3 + screen_x * 3;

			(*result)[screen_buffer_offset + 0] = (pixel_v * fg.r) >> 8;
			(*result)[screen_buffer_offset + 1] = (pixel_v * fg.g) >> 8;
			(*result)[screen_buffer_offset + 2] = (pixel_v * fg.b) >> 8;
		}
	}
}

typedef struct {
        int n;
        double r, g, b;
} pixel_t;

void draw_text::draw_glyph_bitmap(const glyph_cache_entry_t *const glyph, const FT_Int dest_x, const FT_Int dest_y, const rgb_t & fg, const rgb_t & bg, const bool has_color, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t *const dest, const int dest_width, const int dest_height)
{
	uint8_t *result        = nullptr;
	int      result_width  = 0;
	int      result_height = 0;
	draw_glyph_bitmap_low(&glyph->bitmap, fg, bg, has_color, intensity, invert, underline, strikethrough, &result, &result_width, &result_height);

	// resize & copy to x, y
	if (result_width + glyph->horiBearingX / 64 > font_width || result_height > font_height) {
		const double x_scale_temp      =        font_width   / (result_width + glyph->horiBearingX / 64.);
		const double y_scale_temp      = double(font_height) / result_height;
		const double smallest_scale    = std::min(x_scale_temp, y_scale_temp);
		const double scaled_bearing    = glyph->horiBearingX / 64 * smallest_scale;
		const double scaled_bitmap_top = glyph->bitmap_top        * smallest_scale;

		pixel_t *work = new pixel_t[font_width * font_height]();

		for(int y=0; y<result_height; y++) {
			int target_y     = y * smallest_scale;
			int put_offset_y = target_y * font_width;
			int get_offset_y = y * result_width * 3;

			for(int x=0; x<result_width; x++) {
				int target_x   = x * smallest_scale;
				int put_offset = put_offset_y + target_x;
				int get_offset = get_offset_y + x * 3;

				work[put_offset].n++;
				work[put_offset].r += result[get_offset + 0];
				work[put_offset].g += result[get_offset + 1];
				work[put_offset].b += result[get_offset + 2];
			}
		}

		// TODO: check for out of bounds writes (x)
		int work_dest_y = dest_y + max_ascender / 64.0 - scaled_bitmap_top;
		int use_height  = std::min(dest_height - work_dest_y, font_height);

		for(int y=0; y<use_height; y++) {
			int yo  = y * font_width;
			int temp = y + work_dest_y;
			if (temp < 0)
				continue;
			int o   = temp * dest_width * 3 + (dest_x + scaled_bearing) * 3;

			for(int x=0, i = yo; x<font_width; x++, i++, o += 3) {
				if (work[i].n) {
					dest[o + 0] = work[i].r / work[i].n;
					dest[o + 1] = work[i].g / work[i].n;
					dest[o + 2] = work[i].b / work[i].n;
				}
				else {
					dest[o + 0] =
					dest[o + 1] =
					dest[o + 2] = 0;
				}
			}
		}

		delete [] work;
	}
	else {

		int work_dest_x = dest_x + glyph->horiBearingX / 64;
		int use_width   = std::min(dest_width  - work_dest_x, result_width);
		int work_dest_y = dest_y + max_ascender / 64.0 - glyph->bitmap_top;
		int use_height  = std::min(dest_height - work_dest_y, result_height);

		for(int y=0; y<use_height; y++) {
			int temp = work_dest_y + y;

			if (temp >= 0)
				memcpy(&dest[temp * dest_width * 3 + work_dest_x * 3], &result[result_width * y * 3], use_width * 3);
		}
	}

	delete [] result;
}

int draw_text::draw_glyph(const UChar32 utf_character, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, const bool italic, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height)
{
	std::vector<FT_Encoding> encodings { ft_encoding_symbol, ft_encoding_unicode };

	for(int color = 0; color<2; color++) {
		for(int bitmap = 0; bitmap<2; bitmap++) {
			for(auto & encoding : encodings) {
				for(size_t face = 0; face<faces.size(); face++) {
					FT_Select_Charmap(faces.at(face), encoding);

					int glyph_index = FT_Get_Char_Index(faces.at(face), utf_character);
					if (glyph_index == 0 && face < faces.size() - 1)
						continue;

					auto it = italic ? glyph_cache_italic.at(face).find(glyph_index) : glyph_cache.at(face).find(glyph_index);

					if (it == glyph_cache.at(face).end()) {
						int color_choice  = face == 0 ? (color  == 0 ? 0 : FT_LOAD_COLOR | FT_LOAD_TARGET_LCD)     : (color == 0  ? FT_LOAD_COLOR | FT_LOAD_TARGET_LCD    : 0);
						int bitmap_choice = face == 0 ? (bitmap == 0 ? FT_LOAD_NO_BITMAP : 0) : (bitmap == 0 ? 0 : FT_LOAD_NO_BITMAP);
						if (FT_Load_Glyph(faces.at(face), glyph_index, bitmap_choice | color_choice))
							continue;

						FT_GlyphSlot slot = faces.at(face)->glyph;
						if (!slot)
							continue;
						FT_Glyph glyph { };
						FT_Get_Glyph(slot, &glyph);

						if (italic) {
							FT_Matrix matrix { };
							matrix.xx = 0x10000;
							matrix.xy = 0x5000;
							matrix.yx = 0;
							matrix.yy = 0x10000;
							if (FT_Glyph_Transform(glyph, &matrix, nullptr))
								printf("transform error\n");
						}

						if (glyph->format != FT_GLYPH_FORMAT_BITMAP) {
							if (FT_Glyph_To_Bitmap(&glyph, color_choice ? FT_RENDER_MODE_LCD : FT_RENDER_MODE_NORMAL, nullptr, true)) {
								FT_Done_Glyph(glyph);
								continue;
							}
						}

						glyph_cache_entry_t entry { };
						FT_Bitmap_Init(&entry.bitmap);
						FT_Bitmap_Copy(library, &reinterpret_cast<FT_BitmapGlyph>(glyph)->bitmap, &entry.bitmap);
						entry.horiBearingX = slot->metrics.horiBearingX;
						entry.bitmap_top   = slot->bitmap_top;

						FT_Done_Glyph(glyph);

						if (italic) {
							glyph_cache_italic.at(face).insert({ glyph_index, entry });
							it = glyph_cache_italic.at(face).find(glyph_index);
						}
						else {
							glyph_cache.at(face).insert({ glyph_index, entry });
							it = glyph_cache.at(face).find(glyph_index);
						}
					}

					// draw background
					uint8_t max = get_intensity_multiplier(intensity);
					uint8_t bg_r = invert ? fg.r * max / 255 : bg.r * max / 255;
					uint8_t bg_g = invert ? fg.g * max / 255 : bg.g * max / 255;
					uint8_t bg_b = invert ? fg.b * max / 255 : bg.b * max / 255;
					for(int cy=0; cy<font_height; cy++) {
						int offset_y = (y + cy) * dest_width * 3;

						for(int cx=0; cx<font_width; cx++) {
							int offset = offset_y + (x + cx) * 3;

							dest[offset + 0] = bg_r;
							dest[offset + 1] = bg_g;
							dest[offset + 2] = bg_b;
						}
					}
					draw_glyph_bitmap(&it->second, x, y, fg, bg, FT_HAS_COLOR(faces.at(face)), intensity, invert, underline, strikethrough, dest, dest_width, dest_height);

					return font_width;
				}
			}
		}
	}

	return 0;
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
			printf("escape '%c'\n", escape);

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
			else {
				printf("escape '%c' not known\n", escape);
			}

			i = escape_end;
		}
		else {
			out.push_back({ input.at(i), invert, underline, cur_fg_color, cur_bg_color });
		}
	}

	return out;
}

void draw_text::draw_string(const std::string & input, const int height, uint8_t **const rgb_pixels, int *const width)
{
	auto utf_string = preprocess_text(input, { 255, 255, 255 }, { });
	//auto dimensions = find_text_dimensions(face, utf_string);  // TODO
	auto dimensions = find_text_dimensions(faces.at(0), utf_string);  // TODO 

	int  n_chars    = utf_string.size();
	int  x          = 0;
	int  ascender   = std::get<1>(dimensions) / 64;

	*width          = std::get<0>(dimensions);
	*rgb_pixels     = new uint8_t[3 * *width * height]();

	for(int i=0; i<n_chars; i++) {
		auto & utf_char = utf_string.at(i);
		rgb_t  bg { };
		int advance = draw_glyph(utf_char.c, intensity_t::I_BOLD, utf_char.invert, utf_char.underline, false, false, utf_char.fg_color, utf_char.bg_color.has_value() ? utf_char.bg_color.value() : bg, x, 0, *rgb_pixels, std::get<0>(dimensions), height);
		x += advance;
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
				int     bitmap_offset  = cy * bitmap_width * 3 + cx * 3;
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

				int     current_offset = offset + cx * 3;
				int     bitmap_offset  = cy * bitmap_width * 3 + cx * 3;
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
