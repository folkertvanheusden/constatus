// (C) 2023 by folkert van heusden, released under the MIT license
#include "config.h"

#if HAVE_USBGADGET == 1
#include <fcntl.h>
#include <glob.h>
#include <math.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <linux/usb/ch9.h>
#include <usbg/usbg.h>
#include <usbg/function/uvc.h>

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
#include "view.h"
#include "filter.h"
#include "resize.h"
#include "schedule.h"
#include "target_usb_gadget.h"


#define VENDOR  0x1d6b
#define PRODUCT 0x0104

constexpr unsigned pf_yuyv = v4l2_fourcc('Y', 'U', 'Y', 'V');
constexpr unsigned pf_mjpg = v4l2_fourcc('M', 'J', 'P', 'G');

struct my_video_source : public video_source {
	my_video_source() {
	}

	target_usbgadget *t      { nullptr };
	resize           *r      { nullptr };
	int               width  { 1280 };
	int               height { 720  };
	unsigned          pixelformat { pf_yuyv  };
};

static void my_fill_buffer(video_source *s, video_buffer *buf)
{
        my_video_source *src = reinterpret_cast<my_video_source *>(s);
        void *mem = buf->mem;

	video_frame *pvf = src->t->get_prepared_frame();
	if (!pvf) {
		buf->bytesused = 0;
		return;
	}

	size_t n_bytes = 0;

	if (pvf->get_w() != src->width || pvf->get_h() != src->height) {
		auto temp_pvf = pvf->do_resize(src->r, src->width, src->height);
		auto frame    = temp_pvf->get_data_and_len(src->pixelformat == pf_yuyv ? E_YUYV : E_JPEG);
		n_bytes = std::min(std::get<1>(frame), buf->size);
		memcpy(mem, std::get<0>(frame), n_bytes);

		delete temp_pvf;
	}
	else {
		auto frame = pvf->get_data_and_len(src->pixelformat == pf_yuyv ? E_YUYV : E_JPEG);
		n_bytes = std::min(std::get<1>(frame), buf->size);

		memcpy(mem, std::get<0>(frame), n_bytes);
	}

        buf->bytesused = n_bytes;
}

static int my_source_set_format(video_source *s, v4l2_pix_format *fmt)
{
        my_video_source *src = reinterpret_cast<my_video_source *>(s);

        src->width  = fmt->width;
        src->height = fmt->height;
        src->pixelformat = fmt->pixelformat;

        if (src->pixelformat != pf_yuyv && src->pixelformat != pf_mjpg) {
		const char *p = reinterpret_cast<const char *>(&src->pixelformat);
		printf("UNSUPPORTED PIXEL FORMAT %c%c%c%c\n", p[0], p[1], p[2], p[3]);
                return -EINVAL;
	}

        return 0;
}

static int my_source_set_frame_rate(struct video_source *s, unsigned int fps)
{
        my_video_source *src = reinterpret_cast<my_video_source *>(s);

	if (fps)
		src->t->override_fps(fps);

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

static void my_source_destroy(struct video_source *s __attribute__((unused)))
{
}

target_usbgadget::target_usbgadget(const std::string & id, const std::string & descr, source *const s, const int width, const int height, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched) : target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", -1, cfg, false, handle_failure, sched), fixed_width(width), fixed_height(height), quality(quality)
{
}

target_usbgadget::~target_usbgadget()
{
	stop();

	delete prev_frame;
}

void target_usbgadget::override_fps(const unsigned new_fps)
{
	interval = 1. / new_fps;
}

video_frame * target_usbgadget::get_prepared_frame()
{
	uint64_t now_ts        = get_us();
	uint64_t next_frame_ts = t_prev_ts + interval * 1000000;
	if (next_frame_ts > now_ts)
		myusleep(next_frame_ts - now_ts);

	t_prev_ts = next_frame_ts;

	video_frame *pvf = s->get_frame(handle_failure, s_prev_ts);
	if (!pvf)
		return nullptr;

	s_prev_ts = pvf->get_ts();

	if (filters && !filters->empty()) {
		source *cur_s = is_view_proxy ? reinterpret_cast<view *>(s)->get_current_source() : s;
		instance *inst = find_instance_by_interface(cfg, cur_s);

		video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
		delete pvf;
		pvf = temp;
	}

	delete prev_frame;
	prev_frame = pvf;

	return pvf;

}

void target_usbgadget::stop()
{
	events_stop(&events);

	interface::stop();
}

void target_usbgadget::unsetup()
{
	usbg_error usbg_ret = usbg_error(usbg_disable_gadget(g));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on gadget disable: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

        usbg_ret = usbg_error(usbg_rm_config(c, USBG_RM_RECURSE));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on removing config: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

        usbg_ret = usbg_error(usbg_rm_gadget(g, USBG_RM_RECURSE));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on removing gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

	usbg_cleanup(ug_state);
}

std::optional<std::string> target_usbgadget::setup()
{
	usbg_gadget_attrs g_attrs = {
		.bcdUSB          = 0x0200,
		.bDeviceClass    = USB_CLASS_PER_INTERFACE,
		.bDeviceSubClass = 0x00,
		.bDeviceProtocol = 0x00,
		.bMaxPacketSize0 = 64,
		.idVendor        = VENDOR,
		.idProduct       = PRODUCT,
		.bcdDevice       = 0x0002, /* device version */
	};

	usbg_gadget_strs g_strs { 0 };
	g_strs.serial       = const_cast<char *>("1");
	g_strs.manufacturer = const_cast<char *>("vanHeusden.com");
	g_strs.product      = const_cast<char *>("Constatus");

	usbg_config_strs c_strs = {
		.configuration = const_cast<char *>("UVC")
	};

	usbg_f_uvc_frame_attrs uvc_frame_attrs { 0 };
	uvc_frame_attrs.bFrameIndex     = 1;
	uvc_frame_attrs.dwFrameInterval = interval * 1000000;
	uvc_frame_attrs.wHeight         = fixed_height;
	uvc_frame_attrs.wWidth          = fixed_width;

	usbg_f_uvc_frame_attrs *uvc_frame_mjpeg_attrs[] { &uvc_frame_attrs, nullptr };

	usbg_f_uvc_frame_attrs *uvc_frame_uncompressed_attrs[] { &uvc_frame_attrs, nullptr };

	usbg_f_uvc_format_attrs uvc_format_attrs_mjpeg { 0 };
	uvc_format_attrs_mjpeg.frames             = uvc_frame_mjpeg_attrs;
	uvc_format_attrs_mjpeg.format             = "mjpeg/m";
	uvc_format_attrs_mjpeg.bDefaultFrameIndex = 3;

	usbg_f_uvc_format_attrs uvc_format_attrs_uncompressed { 0 };
	uvc_format_attrs_uncompressed.frames             = uvc_frame_uncompressed_attrs;
	uvc_format_attrs_uncompressed.format             = "uncompressed/u";
	uvc_format_attrs_uncompressed.bDefaultFrameIndex = 2;

	usbg_f_uvc_format_attrs *uvc_format_attrs[] { &uvc_format_attrs_mjpeg, &uvc_format_attrs_uncompressed, nullptr };

	usbg_f_uvc_attrs uvc_attrs = {
		.formats = uvc_format_attrs,
	};

	usbg_error usbg_ret = usbg_error(usbg_init("/sys/kernel/config", &ug_state));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB gadget init: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	usbg_ret = usbg_error(usbg_create_gadget(ug_state, "g1", &g_attrs, &g_strs, &g));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB create gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	std::string function_name = "uvc";

        usbg_ret = usbg_error(usbg_create_function(g, USBG_F_UVC, function_name.c_str(), &uvc_attrs, &f_uvc));
        if(usbg_ret != USBG_SUCCESS)
        {
		log(id, LL_ERR, "Error on USB create uvc function: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	usbg_ret = usbg_error(usbg_create_config(g, 1, "Constatus", nullptr, &c_strs, &c));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB create configuration: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

        usbg_ret = usbg_error(usbg_add_config_function(c, "uvc.cam", f_uvc));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB adding uvc.cam: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	usbg_ret = usbg_error(usbg_enable_gadget(g, DEFAULT_UDC));
	// USBG_ERROR_BUSY: already enabled
	if (usbg_ret != USBG_SUCCESS && usbg_ret != USBG_ERROR_BUSY) {
		log(id, LL_ERR, "Error on USB enabling gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	return function_name + ".uvc";
}

void target_usbgadget::operator()()
{
	set_thread_name("usbgadget_" + prefix);

	s->start();

	auto dev_name = setup();
	if (dev_name.has_value() == false)
		return;

	uvc_function_config *fc = configfs_parse_uvc_function(dev_name.value().c_str());
	if (!fc) {
		log(id, LL_ERR, "Failed to process %s", dev_name.value().c_str());
		return;
	}

	uvc_stream *stream = uvc_stream_new(fc->video);
	if (!stream) {
		log(id, LL_ERR, "Failed open stream");
		return;
	}

	events_init(&events);

	uvc_stream_set_event_handler(stream, &events);

	video_source_ops source_ops = {
		.destroy        = my_source_destroy,
		.set_format     = my_source_set_format,
		.set_frame_rate = my_source_set_frame_rate,
		.alloc_buffers  = nullptr,
		.export_buffers = nullptr,
		.free_buffers   = my_source_free_buffers,
		.stream_on      = my_source_stream_on,
		.stream_off     = my_source_stream_off,
		.queue_buffer   = nullptr,
		.fill_buffer    = my_fill_buffer,
	};

	struct my_video_source source_settings;
	source_settings.ops          = &source_ops;
	source_settings.handler      = nullptr;
	source_settings.handler_data = nullptr;
	source_settings.type         = VIDEO_SOURCE_STATIC;
	source_settings.t            = this;
	source_settings.r            = cfg->r;

	uvc_stream_set_video_source(stream, &source_settings);
	uvc_stream_init_uvc(stream, fc);

	events_loop(&events);

        uvc_stream_delete(stream);
        video_source_destroy(&source_settings);
        events_cleanup(&events);
        configfs_free_uvc_function(fc);

	unsetup();

	s->stop();

	log(id, LL_INFO, "stopping");
}
#endif
