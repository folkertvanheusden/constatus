// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once
#include "config.h"
#if HAVE_GSTREAMER == 1
#include "target.h"
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

class target_avi : public target
{
private:
	const int quality;

	std::string name;
	unsigned int f_nr { 0 };
	bool is_start { true };

	void put_frame(GstAppSrc *const appsrc, video_frame * const f);
	void open_file(const std::string & base_pipeline, GstElement **const gpipeline, GstAppSrc **const appsrc);
	void close_file(GstElement **const gpipeline, GstAppSrc **const appsrc);

public:
	target_avi(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_avi();

	void operator()() override;
};
#endif
