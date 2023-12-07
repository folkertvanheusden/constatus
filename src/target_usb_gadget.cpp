// (C) 2023 by folkert van heusden, released under the MIT license
#include "config.h"

#include <fcntl.h>
#include <glob.h>
#include <math.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <linux/videodev2.h>

extern "C" {
#include "uvcgadget/configfs.h"
#include "uvcgadget/events.h"
#include "uvcgadget/stream.h"
#include "uvcgadget/../../lib/uvc.h"
#include "uvcgadget/../../lib/video-buffers.h"
#include "uvcgadget/video-source.h"
}

#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "resize.h"
#include "schedule.h"
#include "target_usb_gadget.h"

struct my_video_source : public video_source {
	my_video_source() {
	}
	source *s { nullptr };
	int width  { 320 };
	int height { 240 };
	unsigned pixelformat { 0 };
};

static void my_fill_buffer(video_source *s, video_buffer *buf)
{
        my_video_source *src = reinterpret_cast<my_video_source *>(s);
        unsigned int bpl;
        unsigned int i, j;
        void *mem = buf->mem;

        bpl = src->width * 2;
        for (i = 0; i < src->height; ++i) {
                for (j = 0; j < bpl; j += 4) {
                        if (j < bpl * 1 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xff;
                        else if (j < bpl * 2 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xff00;
                        else if (j < bpl * 3 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xff0000;
                        else if (j < bpl * 4 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xff00ff;
                        else if (j < bpl * 5 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xffff00;
                        else if (j < bpl * 6 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xffffff;
                        else if (j < bpl * 7 / 8)
                                *(unsigned int *)(mem + i*bpl + j) = 0xf0000f;
                        else
                                *(unsigned int *)(mem + i*bpl + j) = 0;
                }
        }

        buf->bytesused = bpl * src->height;
}

static int my_source_set_format(video_source *s, v4l2_pix_format *fmt)
{
        my_video_source *src = reinterpret_cast<my_video_source *>(s);

        src->width  = fmt->width;
        src->height = fmt->height;
        src->pixelformat = fmt->pixelformat;

        if (src->pixelformat != v4l2_fourcc('Y', 'U', 'Y', 'V'))
                return -EINVAL;

        return 0;
}

static int my_source_set_frame_rate(struct video_source *s __attribute__((unused)), unsigned int fps __attribute__((unused)))
{
        return 0;
}

static int my_source_free_buffers(struct video_source *s __attribute__((unused)))
{
        return 0;
}

static int my_source_stream_on(struct video_source *s __attribute__((unused)))
{
        return 0;
}

static int my_source_stream_off(struct video_source *s __attribute__((unused)))
{
        return 0;
}


target_usbgadget::target_usbgadget(const std::string & id, const std::string & descr, source *const s, const int width, const int height, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched, const std::string & uvc_id) : target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", override_fps, cfg, false, handle_failure, sched), fixed_width(width), fixed_height(height), quality(quality), uvc_id(uvc_id)
{
}

target_usbgadget::~target_usbgadget()
{
	stop();
}

void target_usbgadget::operator()()
{
	set_thread_name("usbgadget_" + prefix);

	s->start();

	const double fps = 1.0 / interval;

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;
	unsigned f_nr = 0;

	int fd = -1;
	std::vector<uint8_t *> buffers;
	int buffer_nr = 0;
	int nbufs = 1;

	video_frame *prev_frame = nullptr;

	pollfd fds[] = { { -1, POLLIN | POLLPRI | POLLERR, 0 } };

	uvc_function_config *fc = configfs_parse_uvc_function(uvc_id.c_str());
	if (!fc) {
		log(id, LL_ERR, "Failed to parse %s", uvc_id.c_str());
		return;
	}

	uvc_stream *stream = uvc_stream_new(fc->video);
	if (!stream) {
		log(id, LL_ERR, "Failed open stream");
		return;
	}

	struct events events { 0 };
	events_init(&events);

	uvc_stream_set_event_handler(stream, &events);

	video_source_ops source_ops = {
		.destroy = nullptr,
		.set_format = my_source_set_format,
		.set_frame_rate = my_source_set_frame_rate,
		.alloc_buffers = nullptr,
		.export_buffers = nullptr,
		.free_buffers = my_source_free_buffers,
		.stream_on = my_source_stream_on,
		.stream_off = my_source_stream_off,
		.queue_buffer = nullptr,
		.fill_buffer = my_fill_buffer,
	};

	struct my_video_source source_settings;
	source_settings.ops          = &source_ops;
	source_settings.events       = &events;
	source_settings.handler      = nullptr;
	source_settings.handler_data = nullptr;
	source_settings.type         = VIDEO_SOURCE_STATIC;
	source_settings.s            = s;

	uvc_stream_set_video_source(stream, &source_settings);

	source_settings.events       = &events;
	uvc_stream_init_uvc(stream, fc);

	events_loop(&events);
#if 0
	bool setup_performed = false;
	bool stream_is_on    = false;

	for(;!local_stop_flag;) {
		pauseCheck();

		st->track_fps();

		uint64_t before_ts = get_us();

		video_frame *pvf = s->get_frame(handle_failure, prev_ts);

		if (pvf) {
			if (setup_performed == false) {
				setup_performed = true;

				// TODO

				for(unsigned i=0; i<nbufs/*reqbuf.count*/; i++) {
					uint8_t *buffer = reinterpret_cast<uint8_t *>(calloc(fixed_width * 3, fixed_height));

					buffers.push_back(buffer);
				}
			}

			prev_ts = pvf->get_ts();

			if (filters && !filters->empty()) {
				source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
				instance *inst = find_instance_by_interface(cfg, cur_s);

				video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;
			}

			const bool allow_store = sched == nullptr || (sched && sched->is_on());

			if (allow_store && pvf->get_w() != -1 && stream_is_on) {
				size_t n_bytes = 0;

				if (pvf->get_w() != fixed_width || pvf->get_h() != fixed_height) {
					auto temp_pvf = pvf->do_resize(cfg->r, fixed_width, fixed_height);

					auto frame = temp_pvf->get_data_and_len(E_YUYV);

					n_bytes = std::get<1>(frame);

					memcpy(buffers[buffer_nr], std::get<0>(frame), std::get<1>(frame));

					delete temp_pvf;
				}
				else {
					auto frame = pvf->get_data_and_len(E_YUYV);

					n_bytes = std::get<1>(frame);

					memcpy(buffers[buffer_nr], std::get<0>(frame), std::get<1>(frame));
				}

				// TODO
			}

			delete prev_frame;
			prev_frame = pvf;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;
	s -> stop();
#endif

        uvc_stream_delete(stream);
        video_source_destroy(&source_settings);
        events_cleanup(&events);
        configfs_free_uvc_function(fc);

	log(id, LL_INFO, "stopping");
}
