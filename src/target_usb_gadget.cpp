// (C) 2023 by folkert van heusden, released under the MIT license
// The 'setup()' method is based on libusbgx/examples/gadget-uvc.c from https://github.com/linux-usb-gadgets/libusbgx/ (GPL-v2).
#include "config.h"

#if HAVE_USBGADGET == 1
#include <fcntl.h>
#include <glob.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <linux/videodev2.h>
#include <linux/usb/ch9.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

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

// the function "udc_find_video_device" was adapted from
// https://gitlab.freedesktop.org/camera/uvc-gadget/-/blob/master/lib/configfs.c#L199
// its license is "LGPL-2.1-or-later"
static std::optional<std::string> udc_find_video_device(const char *udc, const char *function)
{
	glob_t globbuf { 0 };
	unsigned int i = 0;

	char *vpath = nullptr;
	int ret = asprintf(&vpath,
		       "/sys/class/udc/%s/device/gadget*/video4linux/video*",
		       udc ? udc : "*");
	if (!ret)
		return { };

	glob(vpath, 0, nullptr, &globbuf);
	free(vpath);

	for (i = 0; i < globbuf.gl_pathc; ++i) {
		/* Match on the first if no search string. */
		if (!function)
			break;

		char *node_name = nullptr;
		asprintf(&node_name, "%s/function_name", globbuf.gl_pathv[i]);

		char buffer[256] { 0 };

		FILE *fh = fopen(node_name, "r");
		if (fh) {
			fgets(buffer, sizeof buffer, fh);
			fclose(fh);

			char *lf = strchr(buffer, '\n');
			if (lf)
				*lf = 0x00;
		}

		bool match = strcmp(function, buffer) == 0;

		free(node_name);

		if (match)
			break;
	}

	std::string dev;

	if (i < globbuf.gl_pathc) {
		const char *v = basename(globbuf.gl_pathv[i]);

		dev = "/dev/" + std::string(v);
	}

	globfree(&globbuf);

	if (dev.empty())
		return { };

	return dev;
}

target_usbgadget::target_usbgadget(const std::string & id, const std::string & descr, source *const s, const int width, const int height, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched) : target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", override_fps, cfg, false, handle_failure, sched), fixed_width(width), fixed_height(height), quality(quality)
{
}

target_usbgadget::~target_usbgadget()
{
	stop();
}

std::optional<std::string> target_usbgadget::setup()
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
	uvc_frame_attrs.wHeight         = fixed_height;
	uvc_frame_attrs.wWidth          = fixed_width;

//	usbg_f_uvc_frame_attrs *uvc_frame_mjpeg_attrs[] { &uvc_frame_attrs, nullptr };

	usbg_f_uvc_frame_attrs *uvc_frame_uncompressed_attrs[] { &uvc_frame_attrs, nullptr };

//	usbg_f_uvc_format_attrs uvc_format_attrs_mjpeg { 0 };
//	uvc_format_attrs_mjpeg.frames             = uvc_frame_mjpeg_attrs;
//	uvc_format_attrs_mjpeg.format             = "mjpeg/m";
//	uvc_format_attrs_mjpeg.bDefaultFrameIndex = 1;

	usbg_f_uvc_format_attrs uvc_format_attrs_uncompressed { 0 };
	uvc_format_attrs_uncompressed.frames             = uvc_frame_uncompressed_attrs;
	uvc_format_attrs_uncompressed.format             = "uncompressed/u";
	uvc_format_attrs_uncompressed.bDefaultFrameIndex = 1;

//	usbg_f_uvc_format_attrs *uvc_format_attrs[] { &uvc_format_attrs_mjpeg, &uvc_format_attrs_uncompressed, nullptr };
	usbg_f_uvc_format_attrs *uvc_format_attrs[] { &uvc_format_attrs_uncompressed, nullptr };

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
		log(id, LL_ERR, "Error on USB adding acm.GS0: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	usbg_ret = usbg_error(usbg_enable_gadget(g, DEFAULT_UDC));
	if (usbg_ret != USBG_SUCCESS) {
		log(id, LL_ERR, "Error on USB enabling gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	return udc_find_video_device(usbg_get_udc_name(usbg_get_gadget_udc(g)), (function_name + ".uvc").c_str());
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
	int nbufs = 4;

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

				auto dev_name = setup();

				fd = -1;

				if (dev_name.has_value())
					fd = open(dev_name.value().c_str(), O_RDWR);

				if (fd == -1) {
					log(id, LL_ERR, "Cannot open UVC (usb gadget) device (%s): %s", dev_name.has_value() ? dev_name.value().c_str() : "?", strerror(errno));
					break;
				}

				v4l2_requestbuffers reqbuf { 0 };
				reqbuf.count        = nbufs;
				reqbuf.type         = V4L2_BUF_TYPE_VIDEO_OUTPUT;
				reqbuf.memory       = V4L2_MEMORY_USERPTR;

				if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
					log(id, LL_ERR, "VIDIOC_REQBUFS %d failed: %s", nbufs, strerror(errno));
					break;
				}

				log(id, LL_DEBUG, "Using %u buffers", reqbuf.count);

				for(unsigned i=0; i<reqbuf.count; i++) {
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

			if (allow_store && pvf->get_w() != -1) {
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

				v4l2_buffer buf { 0 };

				buf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;
				buf.memory    = V4L2_MEMORY_USERPTR;
				buf.m.userptr = reinterpret_cast<unsigned long int>(buffers[buffer_nr]);
				buf.length    = n_bytes;
				buf.index     = buffer_nr;

				buffer_nr = (buffer_nr + 1) % nbufs;

				if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
					log(id, LL_ERR, "VIDIOC_QBUF failed");
				}
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

	for(auto & b : buffers)
		free(b);

	if (fd != -1)
		close(fd);

	log(id, LL_INFO, "stopping");
}
#endif