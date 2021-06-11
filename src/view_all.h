// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "view_ss.h"

class view_all : public view_ss
{
private:
	const int grid_width{ 1 }, grid_height{ 1 };
	uint64_t *ts_list{ nullptr };
	uint8_t *work{ nullptr };
	video_frame *prev_frame { nullptr };

public:
	view_all(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const int gwidth, const int gheight, const std::vector<view_src_t> & sources, const double switch_interval, std::vector<filter *> *const filters, const int jpeg_quality);
	virtual ~view_all();

	std::string get_html(const std::map<std::string, std::string> & pars) const override;

	video_frame * get_frame(const bool handle_failure, const uint64_t after) override;
};
