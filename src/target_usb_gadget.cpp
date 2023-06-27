// (C) 2023 by folkert van heusden, released under the MIT license
// The 'setup()' method is based on libusbgx/examples/gadget-uvc.c from https://github.com/linux-usb-gadgets/libusbgx/ (GPL-v2).
#include "config.h"

#if HAVE_USBGADGET == 1
#include <math.h>
#include <cstring>
#include <unistd.h>

#include <linux/usb/ch9.h>
#include <usbg/usbg.h>
#include <usbg/function/uvc.h>

#include "target_usb_gadget.h"
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


#define VENDOR  0x1d6b
#define PRODUCT 0x0104

target_usbgadget::target_usbgadget(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched) : target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", override_fps, cfg, false, handle_failure, sched), quality(quality)
{
}

target_usbgadget::~target_usbgadget()
{
	stop();
}

void target_usbgadget::setup()
{
	usbg_gadget_attrs g_attrs = {
		.bcdUSB = 0x0200,
		.bDeviceClass =	USB_CLASS_PER_INTERFACE,
		.bDeviceSubClass = 0x00,
		.bDeviceProtocol = 0x00,
		.bMaxPacketSize0 = 64,
		.idVendor = VENDOR,
		.idProduct = PRODUCT,
		.bcdDevice = 0x0001, /* device version */
	};

	usbg_gadget_strs g_strs { 0 };
	g_strs.serial = const_cast<char *>("1");
	g_strs.manufacturer = const_cast<char *>("vanHeusden.com");
	g_strs.product = const_cast<char *>("Constatus");

	usbg_config_strs c_strs = {
		.configuration = "UVC"
	};

	usbg_f_uvc_frame_attrs uvc_frame_attrs { 0 };
	uvc_frame_attrs.bFrameIndex     = 1;
	uvc_frame_attrs.dwFrameInterval = interval * 1000000;
	uvc_frame_attrs.wHeight         = s->get_height();
	uvc_frame_attrs.wWidth          = s->get_width();

	usbg_f_uvc_frame_attrs *uvc_frame_mjpeg_attrs[] { &uvc_frame_attrs, nullptr };

	usbg_f_uvc_frame_attrs *uvc_frame_uncompressed_attrs[] { &uvc_frame_attrs, nullptr };

	usbg_f_uvc_format_attrs uvc_format_attrs_mjpeg { 0 };
	uvc_format_attrs_mjpeg.frames             = uvc_frame_mjpeg_attrs;
	uvc_format_attrs_mjpeg.format             = "mjpeg/m";
	uvc_format_attrs_mjpeg.bDefaultFrameIndex = 1;

	usbg_f_uvc_format_attrs uvc_format_attrs_uncompressed { 0 };
	uvc_format_attrs_uncompressed.frames             = uvc_frame_uncompressed_attrs;
	uvc_format_attrs_uncompressed.format             = "uncompressed/u";
	uvc_format_attrs_uncompressed.bDefaultFrameIndex = 1;

	usbg_f_uvc_format_attrs *uvc_format_attrs[] { &uvc_format_attrs_mjpeg, &uvc_format_attrs_uncompressed, nullptr };

	usbg_f_uvc_attrs uvc_attrs = {
		.formats = uvc_format_attrs,
	};

	usbg_error usbg_ret = usbg_error(usbg_init("/sys/kernel/config", &ug_state));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB gadget init: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

	usbg_ret = usbg_error(usbg_create_gadget(ug_state, "g1", &g_attrs, &g_strs, &g));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB create gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

        usbg_ret = usbg_error(usbg_create_function(g, USBG_F_UVC, "uvc", &uvc_attrs, &f_uvc));
        if(usbg_ret != USBG_SUCCESS)
        {
		log(id, LL_ERR, "Error on USB create uvc function: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

	usbg_ret = usbg_error(usbg_create_config(g, 1, "Constatus", nullptr, &c_strs, &c));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB create configuration: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

        usbg_ret = usbg_error(usbg_add_config_function(c, "uvc.cam", f_uvc));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB adding acm.GS0: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}

	usbg_ret = usbg_error(usbg_enable_gadget(g, DEFAULT_UDC));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB enabling gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return;
	}
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

	bool setup_performed = false;

	video_frame *prev_frame = nullptr;

	for(;!local_stop_flag;) {
		pauseCheck();
		st->track_fps();

		uint64_t before_ts = get_us();

		video_frame *pvf = s->get_frame(handle_failure, prev_ts);

		if (pvf) {
			if (setup_performed == false) {
				setup_performed = true;

				setup();
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

			if (allow_store) {
				// TODO bool rc = send_frame(pvf->get_data(E_RGB), pvf->get_w(), pvf->get_h());
			}

			delete prev_frame;
			prev_frame = pvf;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;

	s -> stop();

	usbg_cleanup(ug_state);

	log(id, LL_INFO, "stopping");
}
#endif
