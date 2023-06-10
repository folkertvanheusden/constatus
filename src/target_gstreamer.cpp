// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#if HAVE_GSTREAMER == 1
#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "target_gstreamer.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "filter.h"
#include "schedule.h"

target_gstreamer::target_gstreamer(const std::string & id, const std::string & descr, source *const s, const std::string & pipeline, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, schedule *const sched) : target(id, descr, s, "", "", "", -1, interval, filters, "", "", "", -1, cfg, false, false, sched), pipeline(pipeline)
{
}

target_gstreamer::~target_gstreamer()
{
	stop();
}

void target_gstreamer::put_frame(GstAppSrc *const appsrc, const uint8_t *const work, const size_t n)
{
	GstBuffer *buffer = gst_buffer_new_and_alloc(n);
	gst_buffer_fill(buffer, 0, work, n);

	if (gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer) != GST_FLOW_OK)
		log(id, LL_WARNING, "Problem queing frame");
}

void target_gstreamer::operator()()
{
	log(id, LL_INFO, "\"target-gstreamer\" thread started");

	set_thread_name("tgtgst");

	GError *error{ nullptr };
	GstElement *gpipeline = gst_parse_launch(pipeline.c_str(), &error);
	if (error) {
		log(id, LL_ERR, "gstreamer error: %s", error->message);

		if (!gpipeline)
			error_exit(false, "target-gstreamer: %s", error->message);
	}

	GstElement *src = gst_bin_get_by_name(GST_BIN(gpipeline), "constatus");
	if (!src)
		error_exit(false, "target-gstreamer: 'appsrc name=\"constatus\"' missing in pipeline");

	GstAppSrc *appsrc = GST_APP_SRC(src);
	if (!appsrc)
		error_exit(false, "target-gstreamer: GST_APP_SRC failed");

	bool first = true;

	video_frame *prev_frame = nullptr;
	uint64_t ts = 0;

	s -> start();

	int fps = interval > 0 ? 1.0 / interval : 25;

        for(;!local_stop_flag;) {
                pauseCheck();
		st->track_fps();

                uint64_t before_ts = get_us();

		video_frame *pvf = s -> get_frame(false, ts);

		if (pvf) {
			ts = pvf->get_ts();

			if (filters && !filters->empty()) {
				instance *inst = find_instance_by_interface(cfg, s);

				video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;
			}

			pre_record.push_back(pvf);

			if (first) {
				first = false;

				log(id, LL_INFO, "Starting stream with resolution %dx%d @ %d fps", pvf->get_w(), pvf->get_h(), fps);

				g_object_set (G_OBJECT (appsrc),
						"stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
						"format", GST_FORMAT_TIME,
						"is-live", TRUE,
						"block", TRUE,
						"do-timestamp", TRUE,
						NULL);

				GstCaps *video_caps = gst_caps_new_simple("video/x-raw",
					 "format", G_TYPE_STRING, "RGB",
                                         "width", G_TYPE_INT, pvf->get_w(),
                                         "height", G_TYPE_INT, pvf->get_h(),
					 "block", G_TYPE_BOOLEAN, TRUE,
                                         "framerate", GST_TYPE_FRACTION, fps, 1, nullptr);

				gst_app_src_set_caps(GST_APP_SRC(appsrc), video_caps);

				gst_app_src_set_max_bytes((GstAppSrc *)appsrc, 1 * IMS(pvf->get_w(), pvf->get_h(), 3));

				gst_element_set_state(gpipeline, GST_STATE_PLAYING);
			}

                        // get one
                        video_frame *put_f = pre_record.front();
                        pre_record.erase(pre_record.begin());

			delete prev_frame;

			const bool allow_store = sched == nullptr || (sched && sched->is_on());

			if (allow_store)
				put_frame(appsrc, put_f->get_data(E_RGB), IMS(put_f->get_w(), put_f->get_h(), 3));
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	free(prev_frame);

	s -> stop();

	const bool allow_store = sched == nullptr || (sched && sched->is_on());

	for(auto f : pre_record) {
		if (allow_store)
			put_frame(appsrc, f->get_data(E_RGB), IMS(f->get_w(), f->get_h(), 3));

		delete f;
	}

	pre_record.clear();

	gst_app_src_end_of_stream(GST_APP_SRC(appsrc));

	gst_element_set_state(gpipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(gpipeline));

	log(id, LL_INFO, "\"target-gstreamer\" thread terminating");
}
#endif
