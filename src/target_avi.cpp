// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#if HAVE_GSTREAMER == 1
#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "target_avi.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "filter.h"
#include "schedule.h"

target_avi::target_avi(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : target(id, descr, s, store_path, prefix, fmt, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps, cfg, is_view_proxy, handle_failure, sched), quality(quality)
{
}

target_avi::~target_avi()
{
	stop();
}

void target_avi::put_frame(GstAppSrc *const appsrc, video_frame *const f)
{
	GstBuffer *buffer = nullptr;

	auto img = f->get_data_and_len(E_JPEG);

	buffer = gst_buffer_new_and_alloc(std::get<1>(img));
	gst_buffer_fill(buffer, 0, std::get<0>(img), std::get<1>(img));

	if (gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer) != GST_FLOW_OK)
		log(id, LL_WARNING, "Problem queing frame");
}

void target_avi::open_file(const std::string & base_pipeline, GstElement **const gpipeline, GstAppSrc **const appsrc)
{
	// gen filename & register & exec
	name = gen_filename(s, fmt, store_path, prefix, "avi", get_us(), f_nr++);
	create_path(name);
	register_file(name);

	if (!exec_start.empty() && is_start) {
		if (check_thread(&exec_start_th))
			exec_start_th = exec(exec_start, name);

		is_start = false;
	}
	else if (!exec_cycle.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_cycle, name);
	}

	// start pipeline
	GError *error{ nullptr };
	*gpipeline = gst_parse_launch((base_pipeline + name).c_str(), &error);
	if (error) {
		log(id, LL_ERR, "avi error: %s", error->message);

		if (!*gpipeline)
			error_exit(false, "target-avi: %s", error->message);
	}

	GstElement *src = gst_bin_get_by_name(GST_BIN(*gpipeline), "constatus");
	if (!src)
		error_exit(false, "target-avi: failed configuring gstreamer pipeline");

	*appsrc = GST_APP_SRC(src);
	if (!*appsrc)
		error_exit(false, "target-avi: GST_APP_SRC failed");

	int fps = interval > 0 ? 1.0 / interval : 25;

	log(id, LL_INFO, "Starting stream with resolution %dx%d @ %d fps", s->get_width(), s->get_height(), fps);

	g_object_set(G_OBJECT(*appsrc),
		"stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
		"format", GST_FORMAT_TIME,
		"is-live", TRUE,
		"block", TRUE,
		"do-timestamp", TRUE,
		NULL);

	GstCaps *video_caps = gst_caps_new_simple("image/jpeg",
			"width", G_TYPE_INT, s->get_width(),
			"height", G_TYPE_INT, s->get_height(),
			"framerate", GST_TYPE_FRACTION, fps, 1, nullptr);

	gst_app_src_set_caps(GST_APP_SRC(*appsrc), video_caps);

	gst_element_set_state(*gpipeline, GST_STATE_PLAYING);
}

void target_avi::close_file(GstElement **const gpipeline, GstAppSrc **const appsrc)
{
	if (!*gpipeline)
		return;

	// push EOS to stream
	gst_app_src_end_of_stream(*appsrc);

	GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(*gpipeline));

	// wait for an EOS ack
	GstClockTime timeout = 10 * GST_SECOND;
	GstMessage *msg = gst_bus_timed_pop_filtered(bus, timeout, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
	if (msg)
		gst_message_unref(msg);

	gst_object_unref(bus);

	// set pipeline to idle
	gst_element_set_state(*gpipeline, GST_STATE_NULL);

	// delete it
	gst_object_unref(GST_OBJECT(*gpipeline));

	*appsrc = nullptr;
	*gpipeline = nullptr;

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, name);

		join_thread(&exec_end_th, id, "exec-end");
	}
}

void target_avi::operator()()
{
	log(id, LL_INFO, "\"target-avi\" thread started");

	set_thread_name("tgt-avi");

	const std::string base_pipeline = "appsrc name=\"constatus\" ! mux.video_%u avimux name=mux ! filesink location=";

	video_frame *prev_frame = nullptr;
	uint64_t ts = 0;

	GstElement *gpipeline { nullptr };
	GstAppSrc *appsrc = { nullptr };

	s -> start();

	int fps = interval > 0 ? 1.0 / interval : 25;

	bool use_filters = filters && !filters -> empty();

	time_t next_file = max_time > 0 ? time(nullptr) + max_time : 0;

        for(;!local_stop_flag;) {
                pauseCheck();
		st->track_fps();

                uint64_t before_ts = get_us();

		const bool allow_store = sched == nullptr || (sched && sched->is_on());

		video_frame *pvf = s -> get_frame(false, ts);

		if (pvf) {
			ts = pvf->get_ts();

			if (filters && !filters->empty()) {
				instance *inst = find_instance_by_interface(cfg, s);

				video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;

				delete prev_frame;
				prev_frame = pvf->duplicate(E_RGB);
			}

			pre_record.push_back(pvf);

                        // get one
                        video_frame *put_f = pre_record.front();
                        pre_record.erase(pre_record.begin());

			time_t now = time(nullptr);
			if ((now >= next_file && next_file != 0 && gpipeline) || allow_store == false) {
				close_file(&gpipeline, &appsrc);

				next_file = now + max_time;
			}

			if (gpipeline == nullptr)
				open_file(base_pipeline, &gpipeline, &appsrc);

			if (allow_store)
				put_frame(appsrc, put_f);

			delete put_f;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;

	s -> stop();

	if (pre_record.empty()) {
		if (gpipeline)
			close_file(&gpipeline, &appsrc);
	}
	else {
		log(id, LL_INFO, "Pushing buffers to avi file...");

		if (!gpipeline)
			open_file(base_pipeline, &gpipeline, &appsrc);

		const bool allow_store = sched == nullptr || (sched && sched->is_on());

		for(auto f : pre_record) {
			if (allow_store)
				put_frame(appsrc, f);

			delete f;
		}

		pre_record.clear();

		close_file(&gpipeline, &appsrc);
	}

	log(id, LL_INFO, "\"target-avi\" thread terminating");
}
#endif
