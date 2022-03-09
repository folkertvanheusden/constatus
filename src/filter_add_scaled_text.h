// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <stdint.h>
#include <string>

#include "filter.h"

class interface;

class filter_add_scaled_text : public filter
{
private:
	std::string what;
	std::string font_file;
	const int   x;
	const int   y;
	const int   font_size;
	const int   width;
	const std::optional<rgb_t> bg;
	const rgb_t col;
	const bool  invert;
	const std::map<std::string, feed *> text_feeds;

public:
	filter_add_scaled_text(const std::string & what, const std::string & font_file, const int x, const int y, const int font_size, const int width, std::optional<rgb_t> bg, const rgb_t col, const bool invert, const std::map<std::string, feed *> & text_feeds);
	~filter_add_scaled_text();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
