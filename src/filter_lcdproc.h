// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#include "filter.h"

class interface;

typedef struct {
	std::thread *th;
	std::atomic_bool is_terminated;
} thread_info_t;

typedef enum { LWT_STRING, LWT_HBAR, LWT_VBAR, LWT_ICON, LWT_TITLE, LWT_SCROLLER, LWT_FRAME, LWT_NUM, LWT_NONE } lcdproc_widget_type_t;

struct lcdproc_widget
{
	lcdproc_widget_type_t type;
	std::string id;
	std::string text; // only string and title are supported currently
	int x, y;
	std::string in;
};

struct lcdproc_frame : lcdproc_widget
{
	int left, top, right, bottom, width, height, speed;
	char direction;
};

struct lcdproc_screen
{
	int fd;

	int prio;
	std::string id;
	std::string name;
	int duration;

	std::vector<lcdproc_widget *> widgets;
};

class filter_lcdproc : public filter
{
private:
	const std::string adapter;
	std::string font_file;
	const int x, y, w, h;
	const int n_col, n_row;
	const std::optional<rgb_t> bg;
	const rgb_t col;
	const int switch_interval;
	const bool invert;

	int font_size { 0 };

	std::atomic_bool local_stop_flag { false };
	std::thread *th { nullptr };

	std::vector<thread_info_t *> threads;

	std::mutex lpl; // lock for screens
	std::vector<lcdproc_screen> screens;
	uint64_t last_next { 0 };
	size_t cur_scr_nr { 0 };

	void delete_widget(std::string sid, std::string wid);
	void delete_screen(std::string id);
	void delete_lcdproc_user(int fd);
	std::vector<lcdproc_screen>::iterator find_screen(std::string id);
	lcdproc_widget * find_widget(std::string screen_id, std::string widget_id);
	std::pair<bool, std::string> lcdproc_command(const int fd, const char *const cmd);

	void protocol_processor(std::atomic_bool *const stopped_indicator, std::atomic_bool *const local_stop_flag, const int fd);
	void network_listener();

public:
	filter_lcdproc(const std::string & adapter, const std::string & font_file, const int x, const int y, const int w, const int h, const std::optional<rgb_t> bg, const int n_col, const int n_row, const rgb_t col, const int switch_interval, const bool invert);
	~filter_lcdproc();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
