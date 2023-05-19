// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "source.h"
#include "picio.h"

class source_gstreamer : public source
{
private:
	const std::string pipeline;

	transformer_t tf;

public:
	source_gstreamer(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & pipeline, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio);
	virtual ~source_gstreamer();

	void operator()() override;
};
