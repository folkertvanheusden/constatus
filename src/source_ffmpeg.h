// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_ffmpeg : public source
{
private:
	const std::string url;
	const bool tcp;

public:
	source_ffmpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & url, const bool tcp, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds);
	~source_ffmpeg();

	void operator()() override;
};
