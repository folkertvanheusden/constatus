// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
//
// This code is based on:
// https://github.com/PipeWire/pipewire/blob/master/src/examples/video-src.c
#include "config.h"
#if HAVE_PIPEWIRE == 1
#include <unistd.h>

#include "target_pipewire.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "schedule.h"

#define BPP		3
#define MAX_BUFFERS	64

void put_frame(struct pmit_data *data, video_frame *in)
{
	struct pw_buffer *b { nullptr };
	struct spa_meta *m { nullptr };
	struct spa_meta_header *h { nullptr };
	struct spa_meta_region *mc { nullptr };

	if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
		pw_log_warn("out of buffers: %m");
		return;
	}

	struct spa_buffer *buf = (struct spa_buffer *)b->buffer;
	uint8_t *p = (uint8_t *)buf->datas[0].data;
	if (p == NULL) {
		pw_log_warn("p is NULL");
		return;
	}

	if ((h = (struct spa_meta_header *)spa_buffer_find_meta_data(buf, SPA_META_Header, sizeof(*h)))) {
#if 0
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		h->pts = SPA_TIMESPEC_TO_NSEC(&now); // FIXME timestamp van frame
#else
		h->pts = -1;
#endif
		h->flags = 0;
		h->seq = data->seq++;
		h->dts_offset = 0;
	}

	buf->datas[0].chunk->offset = 0;
	buf->datas[0].chunk->size = data->format.size.height * data->stride;
	buf->datas[0].chunk->stride = data->stride;

	const uint8_t *rgb = in->get_data(E_RGB); // FIXME E_JPEG directly?
	int w = in->get_w();
	for (int y = 0; y < data->format.size.height; y++)
		memcpy(&p[y * data->stride], &rgb[y * w * 3], w * 3);

	pw_stream_queue_buffer(data->stream, b);
}

void on_stream_state_changed(void *data_in, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
	struct pmit_data *data = (struct pmit_data *)data_in;

	log(data->id, LL_INFO, "stream state: \"%s\"", pw_stream_state_as_string(state));

	switch (state) {
	case PW_STREAM_STATE_ERROR:
	case PW_STREAM_STATE_UNCONNECTED:
		data->playing = false; // false: decrease source usercount? FIXME
		break;

	case PW_STREAM_STATE_PAUSED:
		data->playing = false; // ^^^ FIXME
		break;

	case PW_STREAM_STATE_STREAMING:
		data->playing = true;
		break;

	default:
		break;
	}
}

void on_stream_param_changed(void *data_in, uint32_t id, const struct spa_pod *param)
{
	struct pmit_data *data = (struct pmit_data *)data_in;
	struct pw_stream *stream = data->stream;
	uint8_t params_buffer[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
	const struct spa_pod *params[5];

	if (param == NULL || id != SPA_PARAM_Format)
		return;

	spa_format_video_raw_parse(param, &data->format);

	data->stride = SPA_ROUND_UP_N(data->format.size.width * BPP, 4);

	params[0] = (const struct spa_pod *)spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
		SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 2, MAX_BUFFERS),
		SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
		SPA_PARAM_BUFFERS_size,    SPA_POD_Int(data->stride * data->format.size.height),
		SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(data->stride),
		SPA_PARAM_BUFFERS_align,   SPA_POD_Int(16));

	params[1] = (const struct spa_pod *)spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
		SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
		SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header)));

	params[2] = (const struct spa_pod *)spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
		SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoCrop),
		SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_region)));

	pw_stream_update_params(stream, params, 3);
}

const struct pw_stream_events stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_stream_state_changed,
	.param_changed = on_stream_param_changed,
};

target_pipewire::target_pipewire(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : 
	target(id, descr, s, "", "", "", -1, interval, filters, "", "", "", -1, cfg, is_view_proxy, handle_failure, sched), quality(100)
{
	const struct spa_pod *params[1] = { nullptr };
	uint8_t buffer[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);

	data.id = id;

	data.loop = pw_main_loop_new(nullptr);

	data.context = pw_context_new(pw_main_loop_get_loop(data.loop), nullptr, 0);

	data.core = pw_context_connect(data.context, nullptr, 0);
	if (data.core == nullptr)
		error_exit(true, "Cannot connect to PipeWire system");

	std::string name = "Constatus " + id;
	data.stream = pw_stream_new(data.core, name.c_str(), pw_properties_new(PW_KEY_MEDIA_CLASS, "Video/Source", nullptr));

	// need to retrieve 1 frame so that we know the resolution
	s -> start();
	(void)s -> get_frame(handle_failure, 0);
	s -> stop();

	struct spa_rectangle min_dim = SPA_RECTANGLE(1, 1);
	struct spa_rectangle default_dim = SPA_RECTANGLE(uint32_t(s->get_width()), uint32_t(s->get_height()));
	struct spa_rectangle max_dim = default_dim;

	struct spa_fraction spa_fps = SPA_FRACTION(uint32_t(interval), 1);

	params[0] = (const struct spa_pod *)spa_pod_builder_add_object(&b,
			SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
			SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
			SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
			SPA_FORMAT_VIDEO_format,    SPA_POD_Id(SPA_VIDEO_FORMAT_RGB),
			SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
				&default_dim,
				&min_dim,
				&max_dim),
			SPA_FORMAT_VIDEO_framerate, &spa_fps);

	pw_stream_add_listener(data.stream,
			&data.stream_listener,
			&stream_events,
			&data);

	pw_stream_connect(data.stream,
			PW_DIRECTION_OUTPUT,
			PW_ID_ANY,
			pw_stream_flags(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_MAP_BUFFERS),
			params, 1);

}

target_pipewire::~target_pipewire()
{
	stop();

	pw_context_destroy(data.context);
	pw_main_loop_destroy(data.loop);
}

void target_pipewire::operator()()
{
	set_thread_name("w-pipew" + prefix);

	std::thread t([&]() {
			pw_main_loop_run(data.loop);
		});

	uint64_t prev_ts = 0;
	std::string name;

	const double fps = 1.0 / interval;

	s -> start();

	video_frame *prev_frame = nullptr;

	for(;!local_stop_flag;) {
		pauseCheck();
		st->track_fps();

		uint64_t before_ts = get_us();

		if (data.playing) {
			video_frame *pvf = s -> get_frame(handle_failure, prev_ts);

			if (pvf) {
				prev_ts = pvf->get_ts();

				if (!filters || filters -> empty()) {
					pre_record.push_back(pvf);
				}
				else {
					source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
					instance *inst = find_instance_by_interface(cfg, cur_s);

					video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
					pre_record.push_back(temp);

					delete prev_frame;
					prev_frame = temp->duplicate({ });
				}

				video_frame * put_f = pre_record.front();
				pre_record.erase(pre_record.begin());

				const bool allow_store = sched == nullptr || (sched && sched->is_on());

				if (allow_store)
					put_frame(&data, put_f);
			}
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, name);

		join_thread(&exec_end_th, id, "exec-end");
	}

	for(auto f : pre_record)
		delete f;

	pre_record.clear();

	delete prev_frame;

	s -> stop();

	log(id, LL_INFO, "terminating pipewire thread");

	pw_main_loop_quit(data.loop);

	log(id, LL_INFO, "waiting for pipewire thread wrapper to terminate");

	t.join();

	s -> stop();
}
#endif
