// (C) 2023 by folkert van heusden, released under the MIT license
#include "config.h"

#include <fcntl.h>
#include <glob.h>
#include <math.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "uvc-gadget/include/uvcgadget/configfs.h"
#include "uvc-gadget/include/uvcgadget/events.h"
#include "uvc-gadget/include/uvcgadget/stream.h"
#include "uvc-gadget/include/uvcgadget/video-source.h"

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
	uvc_stream_set_event_handler(stream, &events);

	video_source_ops source_ops = {
		.destroy = nullptr,
		.set_format = nullptr,
		.set_frame_rate = nullptr,
		.alloc_buffers = nullptr,
		.export_buffers = nullptr,
		.free_buffers = nullptr,
		.stream_on = nullptr,
		.stream_off = nullptr,
		.queue_buffer = nullptr,
		.fill_buffer = nullptr,
	};

	video_source source_settings = {
		.ops = &source_ops,
		.events = &events,
		.handler = nullptr,
		.handler_data = nullptr,
		.type = VIDEO_SOURCE_STATIC,
	};

	uvc_stream_set_video_source(stream, &source_settings);

	uvc_stream_init_uvc(stream, fc);

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

        uvc_stream_delete(stream);
        video_source_destroy(&source_settings);
        events_cleanup(&events);
        configfs_free_uvc_function(fc);

	log(id, LL_INFO, "stopping");
}
