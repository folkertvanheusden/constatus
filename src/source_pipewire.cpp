// (C) 2017-2023 by folkert van heusden, released under the MIT license
//
// Based on example code from https://docs.pipewire.org/page_tutorial5.html

#include "config.h"
#if HAVE_PIPEWIRE == 1
#include <assert.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_pipewire.h"
#include "filter_add_text.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"


void on_process(void *userdata)
{
        struct pmis_data *data = (struct pmis_data *)userdata;
        struct pw_buffer *b = nullptr;
        struct spa_buffer *buf = nullptr;

        if ((b = pw_stream_dequeue_buffer(data->stream)) == nullptr) {
                pw_log_warn("out of buffers: %m");
                log(LL_WARNING, "out of buffers: %m");
                return;
        }

        buf = b->buffer;
	uint8_t *src = (uint8_t *)buf->datas[0].data;
        if (src == nullptr) {
		log(LL_DEBUG, "frame has no data");
                return;
	}

	const int stride = buf->datas[0].chunk->stride;
	const int size   = buf->datas[0].chunk->size;

	const int width = data->format.info.raw.size.width;
	int line_len = -1;

	if (data->format.info.raw.format == SPA_VIDEO_FORMAT_RGB)
		line_len = width * 3;
	else if (data->format.info.raw.format == SPA_VIDEO_FORMAT_YUY2)
		line_len = width * 2;
	else if (data->format.info.raw.format == SPA_VIDEO_FORMAT_I420)
		line_len = width * 2;
	else
		log(LL_WARNING, "Unexpected video format %x", data->format.info.raw.format);

	if (line_len != -1) {
		int padding = stride - line_len;
		assert(padding >= 0);

		uint8_t *unstrided = padding ? (uint8_t *)duplicate(src, size, line_len, padding) : src;
		int height = data->format.info.raw.size.height;

		int rgb_len = height * width * 3;

		if (data->format.info.raw.format == SPA_VIDEO_FORMAT_RGB)
			data->s->set_frame(E_RGB, unstrided, rgb_len);
		else if (data->format.info.raw.format == SPA_VIDEO_FORMAT_I420) {
			uint8_t *rgb = nullptr;
			myjpeg::i420_to_rgb(data->tf, src, width, height, &rgb);

			data->s->set_frame(E_RGB, rgb, rgb_len, false);
		}
		else if (data->format.info.raw.format == SPA_VIDEO_FORMAT_YUY2) {
			uint8_t *rgb = nullptr;
			yuy2_to_rgb(src, width, height, &rgb);

			data->s->set_frame(E_RGB, rgb, rgb_len, false);
		}
		else {
			log(LL_WARNING, "Unknown frame encoding (%x, size: %d)", data->format.info.raw.format, size);
		}

		pw_stream_queue_buffer(data->stream, b);

		if (padding)
			free(unstrided);
	}
}

void on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
        struct pmis_data *data = (struct pmis_data *)userdata;

        if (param == nullptr || id != SPA_PARAM_Format)
                return;

        if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0)
                return;

        if (data->format.media_type != SPA_MEDIA_TYPE_video || data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
                return;

        if (spa_format_video_raw_parse(param, &data->format.info.raw) < 0)
                return;

        log(LL_INFO, "PipeWire format changed event returned: format: %d (%s), size: %dx%d, framerate: %dx%d",
		data->format.info.raw.format, spa_debug_type_find_name(spa_type_video_format, data->format.info.raw.format), 
		data->format.info.raw.size.width, data->format.info.raw.size.height,
       		data->format.info.raw.framerate.num, data->format.info.raw.framerate.denom);

	data->s->set_size(data->format.info.raw.size.width, data->format.info.raw.size.height);

	// 2021-6-24, 0.3.24, " MAP_BUFFERS was disabled for dmabuf because it requires
	// special handling, not setting a dataType would allow v4l2 to choose, and then
	// it chooses DMABuf so things end up not mapped"
	uint8_t buffer[1024] { 0 };
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);

	const struct spa_pod *params[1] = { nullptr };

	params[0] = (const struct spa_pod *)spa_pod_builder_add_object(&b,
		SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers, 
		SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1<<SPA_DATA_MemPtr)));

	pw_stream_update_params(data->stream, params, 1);
	/////
}

static const struct pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = on_param_changed,
        .process = on_process,
};

source_pipewire::source_pipewire(const std::string & id, const std::string & descr, const int source_id, const int width, const int height, const int quality, controls *const c, const double max_fps, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, "", width, height, nullptr, c, quality, text_feeds, keep_aspectratio), source_id(source_id), interval(1.0 / max_fps)
{
	uint8_t buffer[1024] { 0 };
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);

	min_dim = SPA_RECTANGLE(1, 1);
	default_dim = SPA_RECTANGLE(uint32_t(width), uint32_t(height));
	max_dim = SPA_RECTANGLE(4096, 4096);

	min_frac = SPA_FRACTION(0, 1);
	default_frac = SPA_FRACTION(uint32_t(interval), 1);
	max_frac = SPA_FRACTION(1000, 1);

	data.s = this;
	data.tf = myjpeg::allocate_transformer();

	data.loop = pw_main_loop_new(nullptr);

	std::string name = "Constatus " + id;
	data.stream = pw_stream_new_simple(
			pw_main_loop_get_loop(data.loop),
			name.c_str(),
			pw_properties_new(
				PW_KEY_MEDIA_TYPE, "Video",
				PW_KEY_MEDIA_CATEGORY, "Capture",
				PW_KEY_MEDIA_ROLE, "Camera",
				nullptr),
			&stream_events,
			&data);

	params[0] = (const struct spa_pod *)spa_pod_builder_add_object(&b,
			SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
			SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
			SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
			SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(4,
				SPA_VIDEO_FORMAT_RGB,
				SPA_VIDEO_FORMAT_RGB,
				SPA_VIDEO_FORMAT_YUY2,
				SPA_VIDEO_FORMAT_I420),
			SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
				&default_dim,
				&min_dim,
				&max_dim),
			SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
				&default_frac,
				&min_frac,
				&max_frac),
			SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1<<SPA_DATA_MemPtr)));

	pw_stream_connect(data.stream,
			PW_DIRECTION_INPUT,
			source_id,
			pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
			params, 1);
}

source_pipewire::~source_pipewire()
{
	pw_stream_destroy(data.stream);
	pw_main_loop_destroy(data.loop);

	delete c;

	myjpeg::free_transformer(data.tf);
}

void source_pipewire::operator()()
{
	log(id, LL_INFO, "source pipewire thread started (%x)", source_id);

	std::thread t([&]() {
			pw_main_loop_run(data.loop);
		});

	for(;!local_stop_flag;)
		myusleep(100000);

	log(id, LL_INFO, "terminating pipewire thread");

	pw_main_loop_quit(data.loop);

	log(id, LL_INFO, "waiting for pipewire thread wrapper to terminate");

	t.join();

	register_thread_end("source pipewire");
}
#endif
