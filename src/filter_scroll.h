// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <atomic>
#include <stdint.h>
#include <string>

#include "filter.h"
#include "cfg.h"

typedef struct
{
	std::string text;
	const uint8_t *bitmap;
	int w, h;
} scroll_entry_t;

class filter_scroll : public filter
{
protected:
	std::string what, font_file;
	const int x, y, text_w, n_lines, font_size;
	const bool horizontal_scroll;
	const std::optional<rgb_t> bg;
	const rgb_t col;
	const bool invert;

	std::mutex buffer_lock;
	std::vector<scroll_entry_t> buffer;

	std::atomic_bool local_stop_flag{ false };

	size_t char_nr { 0 };

private:
	const std::string exec_what;
	const int scroll_speed;
	int fd { -1 }, cur_x_pos { 0 };
	std::string in_buffer;

	void restart_process();

public:
	filter_scroll(const std::string & font_file, const int x, const int y, const int text_w, const int n_lines, const int font_size, const std::string & exec_what, const bool horizontal_scroll, const std::optional<rgb_t> bg, const int scroll_speed, const rgb_t col, const bool invert);
	~filter_scroll();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
	
	virtual std::tuple<bool, std::string> poll_for_data();

	void operator()() override;
};
