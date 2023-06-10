// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stdint.h>
#include <string>
#include <cstring>
#include <time.h>

#include "gen.h"
#include "filter_add_text.h"
#include "filter_add_scaled_text.h"
#include "error.h"
#include "utils.h"
#include "draw_text.h"

filter_add_scaled_text::filter_add_scaled_text(const std::string & what, const std::string & font_file, const int x, const int y, const int font_size, const int width, const std::optional<rgb_t> bg, const rgb_t col, const bool invert, const std::map<std::string, feed *> & text_feeds) :
	what(what),
	x(x), y(y),
	font_size(font_size), width(width),
	bg(bg), col(col),
	invert(invert),
	text_feeds(text_feeds)
{
	font = new draw_text(font_file, font_size);
}

filter_add_scaled_text::~filter_add_scaled_text()
{
	delete font;
}

void filter_add_scaled_text::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	std::string text_out = unescape(what, ts, i, specific_int, text_feeds);

	std::vector<std::string> *parts;
	if (text_out.find("\n") != std::string::npos)
		parts = split(text_out.c_str(), "\n");
	else
		parts = split(text_out.c_str(), "\\n");

	int work_x = x < 0 ? x + w : x;
	int work_y = y < 0 ? y + h : y;

	for(std::string cl : *parts) {
		draw_text_on_bitmap(font, cl, w, h, in_out, font_size, col, bg, work_x, work_y);

		work_y += font_size + 1;
	}

	delete parts;
}
