// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#if HAVE_GSTREAMER == 1
#include <cstring>
#include "source_gstreamer.h"
#include "log.h"
#include "utils.h"
#include "picio.h"
#include "controls.h"

source_gstreamer::source_gstreamer(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & pipeline, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, exec_failure, -1.0, r, resize_w, resize_h, loglevel, timeout, filters, failure, c, jpeg_quality, text_feeds, keep_aspectratio), pipeline(pipeline)
{
	tf = myjpeg::allocate_transformer();
}

source_gstreamer::~source_gstreamer()
{
	stop();
	delete c;
	myjpeg::free_transformer(tf);
}

void source_gstreamer::operator()()
{
	log(id, LL_INFO, "source gstreamer thread started (%s)", pipeline.c_str());

	set_thread_name("src_gstreamer");

	GError *error{ nullptr };
	GstElement *gpipeline = gst_parse_launch(pipeline.c_str(), &error);
	if (error) {
		log(id, LL_ERR, "gstreamer error: %s", error->message);

		if (!gpipeline)
			error_exit(false, "gstreamer-source: %s", error->message);
	}

	GstElement *sink = gst_bin_get_by_name(GST_BIN(gpipeline), "constatus");
	if (!sink)
		error_exit(false, "gstreamer-source: 'appsink name=\"constatus\"' missing in pipeline");

	GstAppSink *appsink = GST_APP_SINK(sink);
	if (!appsink)
		error_exit(false, "gstreamer-source: GST_APP_SINK failed");

	gst_element_set_state(gpipeline, GST_STATE_PLAYING);

	bool first = true;

	for(;!local_stop_flag;)
	{
		GstSample *sample = gst_app_sink_pull_sample(appsink);

		if (sample) {
			GstCaps *caps = gst_sample_get_caps(sample);
			GstStructure *capss = gst_caps_get_structure(caps, 0);

			int gwidth = 0, gheight = 0;
			gst_structure_get_int(capss, "width", &gwidth);
			gst_structure_get_int(capss, "height", &gheight);
			const char *format = gst_structure_get_string(capss, "format");

			set_size(gwidth, gheight);

			if (first) {
				first = false;
				log(id, LL_INFO, "Resolution: %dx%d (%s)", width, height, format);
			}

			if (strcmp(format, "I420") != 0 and strcmp(format, "RGB") != 0) {
				log(id, LL_ERR, "source-gstreamer: stream has unexpected format (%s), expecting either RGB org I420", format);
				gst_sample_unref(sample);
				break;
			}

			GstBuffer *buffer = gst_sample_get_buffer(sample);

			GstMapInfo info;
			gst_buffer_map(buffer, &info, GST_MAP_READ);

			const size_t n = IMS(width, height, 3);

			if (strcmp(format, "RGB") == 0) {
				if (need_scale())
					set_scaled_frame(info.data, width, height, keep_aspectratio);
				else
					set_frame(E_RGB, info.data, n);
			}
			else {
				uint8_t *rgb { nullptr };
				my_jpeg.i420_to_rgb(tf, info.data, width, height, &rgb);

				if (need_scale())
					set_scaled_frame(rgb, width, height, keep_aspectratio);
				else
					set_frame(E_RGB, rgb, n);

				free(rgb);
			}

			gst_buffer_unmap(buffer, &info);

			gst_sample_unref(sample);
		}
		else {
			do_exec_failure();
		}

		st->track_cpu_usage();
	}

	gst_element_set_state(gpipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(gpipeline));

	register_thread_end("gstreamer");
}
#endif
