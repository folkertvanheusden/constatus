// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <map>
#include <string>
#include <vector>

#include "cfg.h"
#include "view.h"

class view_html_all : public view
{
private:
	virtual video_frame * get_frame(const bool handle_failure, const uint64_t after) override { return { }; }

public:
	view_html_all(configuration_t *const cfg, const std::string & id, const std::string & descr, const std::vector<view_src_t> & sources, const int jpeg_quality);
	virtual ~view_html_all();

	std::string get_html(const std::map<std::string, std::string> & pars) const override;

	void operator()() override;
};
