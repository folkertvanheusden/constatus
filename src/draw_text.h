// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include <unicode/ustring.h>

#include "gen.h"


typedef struct {
	UChar32              c;
	bool                 underline;
	bool                 invert;
	rgb_t                fg_color;
	std::optional<rgb_t> bg_color;
} text_with_attributes_t;

#define DEFAULT_FONT_FILE "/usr/share/fonts/truetype/unifont/unifont.ttf"

extern std::mutex freetype2_lock;
extern std::mutex fontconfig_lock;

class draw_text
{
protected:
	static FT_Library library;

	const int         font_height { 0 };
	FT_Face           face        { 0 };

	std::optional<std::tuple<int, int, int, int> > find_text_dimensions(const UChar32 c);

	void draw_glyph_bitmap(const FT_Bitmap *const bitmap, const int output_height, const FT_Int x, const FT_Int y, const bool invert, const bool underline, uint8_t *const dest, const int dest_width, const int dest_height);

	int draw_glyph(const UChar32 utf_character, const int output_height, const bool invert, const bool underline, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height);

	std::tuple<int, int, int, int> find_text_dimensions(const FT_Face & face, const std::vector<text_with_attributes_t> & utf_string);

	std::optional<std::tuple<UChar32 *, int> > text_to_utf32(const std::string & text);

	std::vector<text_with_attributes_t> preprocess_text(const std::string & input, const rgb_t & fg_color, const std::optional<rgb_t> & bg_color);

public:
	draw_text(const std::string & font_file, const int font_height);
	virtual ~draw_text();

	static void init();
	static void uninit();

	void draw_string(const std::string & input, const int height, uint8_t **const grayscale_pixels, int *const width);
};

void draw_text_on_bitmap(draw_text *const font, const std::string & in, const int dest_bitmap_width, const int dest_bitmap_height, uint8_t *const dest_bitmap, const int text_height, const rgb_t & fg, const std::optional<const rgb_t> & bg, const int put_x, const int put_y);
