// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <map>
#include <stdint.h>
#include <string>

#include "draw_text.h"

class draw_text_bitmap : public draw_text
{
private:
	uint8_t *result_bitmap { nullptr };

protected:
	void draw_bitmap(const FT_Face & face, const std::tuple<UChar32 *, int> & utf_string, const std::tuple<int, int, int, int> & dimensions, const bool use_kerning, const int output_height);

public:
	draw_text_bitmap(const std::string & filename, const std::string & text, const int output_height, const bool antialias, const std::optional<rgb_t> bg, const rgb_t col, const bool invert);
	virtual ~draw_text_bitmap();

	const uint8_t *get_bitmap() const { return result_bitmap; }
};
