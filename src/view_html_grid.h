// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "view.h"

class view_html_grid : public view
{
private:
	const int grid_width, grid_height;
	const double switch_interval;

	video_frame * get_frame(const bool handle_failure, const uint64_t after) override { return { }; }

public:
	view_html_grid(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, const int gwidth, const int gheight, const double switch_interval, const int jpeg_quality);
	virtual ~view_html_grid();

	std::string get_html(const std::map<std::string, std::string> & pars) const override;

	void operator()() override;
};
