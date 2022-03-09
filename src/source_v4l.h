// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_LIBV4L2 == 1
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_v4l : public source
{
protected:
	const std::string dev;
	const int w_override, h_override;
	const int use_controls;
	int fd, vw, vh;
	unsigned int pixelformat;

	struct buffer {
		void   *start;
		size_t length;
	};

	struct buffer *buffers = nullptr;
	int n_buffers;

	const bool prefer_jpeg { false };

public:
	source_v4l(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & dev, const int jpeg_quality, const double max_fps, const int w_override, const int h_override, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, const bool prefer_jpeg, const bool use_controls, const std::map<std::string, feed *> & text_feeds);
	virtual ~source_v4l();

	virtual void pan_tilt(const double abs_pan, const double abs_tilt) override;
	virtual void get_pan_tilt(double *const pan, double *const tilt) const override;

	virtual void operator()() override;
};
#endif
