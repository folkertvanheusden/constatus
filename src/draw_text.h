// (C) 2017-2023 by folkert van heusden, released under the MIT license
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

typedef struct {
	FT_Bitmap bitmap;
	int       horiBearingX;
	int       bitmap_top;
} glyph_cache_entry_t;

class draw_text
{
public:
	enum intensity_t { I_NORMAL, I_BOLD, I_DIM };

protected:
	static FT_Library    library;

	int                  font_height  { 0 };
	int                  font_width   { 0 };
	int                  max_ascender { 0 };
	std::vector<FT_Face> faces;
	std::vector<std::map<int, glyph_cache_entry_t> > glyph_cache;
	std::vector<std::map<int, glyph_cache_entry_t> > glyph_cache_italic;
	bool                 render_mode_error { false };

	int get_intensity_multiplier(const intensity_t i);

	std::optional<std::tuple<int, int, int, int> > find_text_dimensions(const UChar32 c);

	void draw_glyph_bitmap_low(const FT_Bitmap *const bitmap, const rgb_t & fg, const rgb_t & bg, const bool has_color, const intensity_t intensity, const bool invert, const bool underline, const bool strikethrough, uint8_t **const result, int *const result_width, int *const result_height);
	void draw_glyph_bitmap(const glyph_cache_entry_t *const glyph, const FT_Int x, const FT_Int y, const rgb_t & fg, const rgb_t & bg, const bool has_color, const intensity_t i, const bool invert, const bool underline, const bool strikethrough, uint8_t *const dest, const int dest_width, const int dest_height);

	int draw_glyph(const UChar32 utf_character, const intensity_t i, const bool invert, const bool underline, const bool strikethrough, const bool italic, const rgb_t & fg, const rgb_t & bg, const int x, const int y, uint8_t *const dest, const int dest_width, const int dest_height);

	std::tuple<int, int, int, int> find_text_dimensions(const FT_Face & face, const std::vector<text_with_attributes_t> & utf_string);

	std::optional<std::tuple<UChar32 *, int> > text_to_utf32(const std::string & text);

	std::vector<text_with_attributes_t> preprocess_text(const std::string & input, const rgb_t & fg_color, const std::optional<rgb_t> & bg_color);

	void init(const std::vector<std::string> & font_files);

public:
	draw_text(const std::string & font_file,               const int font_height);
	draw_text(const std::vector<std::string> & font_files, const int font_height);
	virtual ~draw_text();

	static void init();
	static void uninit();

	void draw_string(const std::string & input, const int height, uint8_t **const grayscale_pixels, int *const width);
};

void draw_text_on_bitmap(draw_text *const font, const std::string & in, const int dest_bitmap_width, const int dest_bitmap_height, uint8_t *const dest_bitmap, const int text_height, const rgb_t & fg, const std::optional<const rgb_t> & bg, const int put_x, const int put_y);
