// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_static : public source
{
public:
	source_static(const std::string & id, const std::string & descr, const int width, const int height, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio);
	~source_static();

	virtual video_frame * get_frame(const bool handle_failure, const uint64_t after) override;

	void operator()() override;
};
