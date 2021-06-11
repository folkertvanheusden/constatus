// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <map>
#include <mutex>
#include <stdint.h>
#include <string>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include <unicode/ustring.h>

#include "gen.h"

#define DEFAULT_FONT_FILE "/usr/share/fonts/truetype/unifont/unifont.ttf"
#define DEFAULT_PROP_FONT_FILE "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf"

extern std::mutex freetype2_lock;
extern std::mutex fontconfig_lock;

class draw_text {
protected:
	static FT_Library library;
	static std::map<std::string, FT_Face> font_cache;

	uint8_t *const target;
	const int target_width, target_height, target_x, target_y, text_w;
	const rgb_t col;

	int result_w { 0 }, result_h { 0 };
	bool error_shown { false };

	void draw_character(const FT_Bitmap *const bitmap, const int output_height, const FT_Int x, const FT_Int y, uint8_t r, uint8_t g, uint8_t b, const bool invert, const bool underline, const int right_x, uint8_t *const dest, const int dest_width, const int dest_height);
	void draw_background(const int width, const int result_h, const rgb_t & bg);
	std::optional<std::tuple<UChar32 *, int> > text_to_utf32(const std::string & text);
	FT_Face load_font(const std::string & font_filename);
	std::tuple<int, int, int, int> find_text_dimensions(const FT_Face & face, const std::tuple<UChar32 *, int> & utf_string, const bool anti_alias);
	void draw_bitmap(const FT_Face & face, const std::tuple<UChar32 *, int> & utf_string, const std::tuple<int, int, int, int> & dimensions, const bool use_kerning, const int output_height, const bool inverse);

public:
	draw_text(const std::string & font_filename, const std::string & text, const int output_height, const bool antialias, uint8_t *const target, const int target_width, const int target_height, const int target_x, const int target_y, const int text_w, const std::optional<rgb_t> bg, const rgb_t col, const bool inverse);
	virtual ~draw_text();

	std::tuple<int, int> text_final_dimensions() const { return std::make_tuple(result_w, result_h); }

	static void init_fonts();
	static void uninit_fonts();
};

std::string substr(const UChar32 *const utf32_str, const int idx, const int n);
std::string find_font_by_name(const std::string & font_name, const std::string & default_font_file);
