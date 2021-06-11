// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <stdlib.h>
#include <string>
#include <vector>

#include "cfg.h"
#include "view.h"

class source;
class target;

class view_ss : public view
{
private:
	const bool auto_shrink;
	unsigned long event_nr { 0 };

protected:
	const double switch_interval;
	video_frame *prev_frame { nullptr };

	size_t cur_source_index;
	uint64_t latest_switch;
	source *cur_source;

public:
	view_ss(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const bool auto_shrink, const std::vector<view_src_t> & sources, const double switch_interval, std::vector<filter *> *const filters, const int jpeg_quality);
	virtual ~view_ss();

	std::string get_html(const std::map<std::string, std::string> & pars) const override;

	video_frame * get_frame(const bool handle_failure, const uint64_t after) override;

	virtual source *get_current_source() override;

	void start() override;
	void stop() override;

	virtual void operator()() override;
};
