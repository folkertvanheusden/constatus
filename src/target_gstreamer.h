// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "target.h"

#if HAVE_GSTREAMER == 1
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

class target_gstreamer : public target
{
private:
	const std::string pipeline;

	void put_frame(GstAppSrc *const appsrc, const uint8_t *const work, const size_t n);

public:
	target_gstreamer(const std::string & id, const std::string & descr, source *const s, const std::string & pipeline, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, schedule *const sched);
	virtual ~target_gstreamer();

	void operator()() override;
};
#endif
