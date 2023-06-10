// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <shared_mutex>
#include "source.h"

class source_delay : public source
{
private:
	std::vector<video_frame *> frames;
	mutable std::shared_mutex frames_lock;

	source *const s;
	const size_t n_frames;
	const double max_fps;

	uint64_t last_ts { 0 };

	video_frame *prev { nullptr };

public:
	source_delay(const std::string & id, const std::string & descr, const std::string & exec_failure, source *const s, const int jpeg_quality, const int n_frames, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int ll, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const std::map<std::string, feed *> & text_feeds);
	virtual ~source_delay();

	virtual video_frame * get_frame(const bool handle_failure, const uint64_t after) override;
	uint64_t get_current_ts() const override;

	virtual void operator()() override;
};
