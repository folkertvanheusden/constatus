// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <shared_mutex>
#include <string>
#include <vector>

#include "cfg.h"
#include "view_ss.h"

class view_pip : public view_ss
{
private:
	std::mutex prev_frame_lock;
	video_frame *prev_frame { nullptr };

	uint64_t new_ts { 0 };

	std::mutex frames_lock;
	std::condition_variable frames_cv;
	std::vector<video_frame *> frames;

public:
	view_pip(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, std::vector<filter *> *const filters, const int jpeg_quality);
	virtual ~view_pip();

	std::string get_html(const std::map<std::string, std::string> & pars) const override;

	video_frame * get_frame(const bool handle_failure, const uint64_t after) override;

	virtual void operator()() override;
};
