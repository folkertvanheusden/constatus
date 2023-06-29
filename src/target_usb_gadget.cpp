// (C) 2023 by folkert van heusden, released under the MIT license
// The 'setup()' method is based on libusbgx/examples/gadget-uvc.c from https://github.com/linux-usb-gadgets/libusbgx/ (GPL-v2).
// Some of the other code is from https://github.com/peterbay/uvc-gadget
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
#include <linux/usb/video.h>

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

#include "uvc-gadget/uvc-gadget.h"

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
	// USBG_ERROR_BUSY: already enabled
	if (usbg_ret != USBG_SUCCESS && usbg_ret != USBG_ERROR_BUSY) {
		log(id, LL_ERR, "Error on USB enabling gadget: %s / %s", usbg_error_name(usbg_ret), usbg_strerror(usbg_ret));
		return { };
	}

	return udc_find_video_device(usbg_get_udc_name(usbg_get_gadget_udc(g)), (function_name + ".uvc").c_str());
}

static unsigned int get_frame_size(int pixelformat, int width, int height)
{
	switch (pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			return width * height * 2;

		case V4L2_PIX_FMT_MJPEG:
			return width * height;
	}

	return 0;
}

static int uvc_get_frame_format(struct uvc_frame_format ** frame_format, unsigned int iFormat, unsigned int iFrame)
{
	for(int i = 0; i <= last_format_index; i++) {
		if (uvc_frame_format[i].bFormatIndex == iFormat && uvc_frame_format[i].bFrameIndex == iFrame) {
			*frame_format = &uvc_frame_format[i];

			return 0;
		}
	}

	return -1;
}

static int uvc_get_frame_format_index(int format_index, enum uvc_frame_format_getter getter)
{
	int index = -1;
	int value;
	int i;

	for (i = 0; i <= last_format_index; i++) {
		if (format_index == -1 || format_index == (int) uvc_frame_format[i].bFormatIndex) {
			switch (getter) {
				case FORMAT_INDEX_MIN:
				case FORMAT_INDEX_MAX:
					value = uvc_frame_format[i].bFormatIndex;
					break;

				case FRAME_INDEX_MIN:
				case FRAME_INDEX_MAX:
					value = uvc_frame_format[i].bFrameIndex;
					break;
			}

			if (index == -1) {
				index = value;
			}
			else {
				switch (getter) {
					case FORMAT_INDEX_MIN:
					case FRAME_INDEX_MIN:
						if (value < index) {
							index = value;
						}
						break;

					case FORMAT_INDEX_MAX:
					case FRAME_INDEX_MAX:
						if (value > index) {
							index = value;
						}
						break;
				}
			}
		}
	}

	return index;
}

static const char * uvc_request_code_name(unsigned int uvc_control)
{
	switch (uvc_control) {
		case UVC_RC_UNDEFINED:
			return "RC_UNDEFINED";

		case UVC_SET_CUR:
			return "SET_CUR";

		case UVC_GET_CUR:
			return "GET_CUR";

		case UVC_GET_MIN:
			return "GET_MIN";

		case UVC_GET_MAX:
			return "GET_MAX";

		case UVC_GET_RES:
			return "GET_RES";

		case UVC_GET_LEN:
			return "GET_LEN";

		case UVC_GET_INFO:
			return "GET_INFO";

		case UVC_GET_DEF:
			return "GET_DEF";

		default:
			return "UNKNOWN";
	}
}

static void dump_uvc_streaming_control(struct uvc_streaming_control * ctrl)
{
	log(LL_INFO, "DUMP: uvc_streaming_control: format: %d, frame: %d, frame interval: %d",
			ctrl->bFormatIndex,
			ctrl->bFrameIndex,
			ctrl->dwFrameInterval
	      );

	log(LL_INFO, "DUMP: uvc_streaming_control2: min version %d, max version %d", ctrl->bMinVersion, ctrl->bMaxVersion);
}

static void uvc_dump_frame_format(struct uvc_frame_format * frame_format, const char * title)
{
	log(LL_INFO, "%s: format: %d, frame: %d, resolution: %dx%d, frame_interval: %d,  bitrate: [%d, %d]\n",
			title,
			frame_format->bFormatIndex,
			frame_format->bFrameIndex,
			frame_format->wWidth,
			frame_format->wHeight,
			frame_format->dwDefaultFrameInterval,
			frame_format->dwMinBitRate,
			frame_format->dwMaxBitRate
	      );
}

static void uvc_fill_streaming_control(struct uvc_streaming_control * ctrl, enum stream_control_action action, int iformat, int iframe)
{
	int format_first;
	int format_last;
	int frame_first;
	int frame_last;
	int format_frame_first;
	int format_frame_last;
	unsigned int frame_interval;
	unsigned int dwMaxPayloadTransferSize;

	switch (action) {
		case STREAM_CONTROL_INIT:
			printf("UVC: Streaming control: action: INIT\n");
			break;

		case STREAM_CONTROL_MIN:
			printf("UVC: Streaming control: action: GET MIN\n");
			break;

		case STREAM_CONTROL_MAX:
			printf("UVC: Streaming control: action: GET MAX\n");
			break;

		case STREAM_CONTROL_SET:
			printf("UVC: Streaming control: action: SET, format: %d, frame: %d\n", iformat, iframe);
			break;

	}

	format_first = uvc_get_frame_format_index(-1, FORMAT_INDEX_MIN);
	format_last = uvc_get_frame_format_index(-1, FORMAT_INDEX_MAX);

	frame_first = uvc_get_frame_format_index(-1, FRAME_INDEX_MIN);
	frame_last = uvc_get_frame_format_index(-1, FRAME_INDEX_MAX);

	if (action == STREAM_CONTROL_MIN) {
		iformat = format_first;
		iframe = frame_first;

	} else if (action == STREAM_CONTROL_MAX) {
		iformat = format_last;
		iframe = frame_last;

	} else {
		iformat = clamp(iformat, format_first, format_last);

		format_frame_first = uvc_get_frame_format_index(iformat, FRAME_INDEX_MIN);
		format_frame_last = uvc_get_frame_format_index(iformat, FRAME_INDEX_MAX);

		iframe = clamp(iframe, format_frame_first, format_frame_last);
	}

	struct uvc_frame_format * frame_format;
	uvc_get_frame_format(&frame_format, iformat, iframe);

	uvc_dump_frame_format(frame_format, "FRAME");

	if (frame_format->dwDefaultFrameInterval >= 100000) {
		frame_interval = frame_format->dwDefaultFrameInterval;
	} else {
		frame_interval = 400000;
	}

	dwMaxPayloadTransferSize = streaming_maxpacket;
	if (streaming_maxpacket > 1024 && streaming_maxpacket % 1024 != 0) {
		dwMaxPayloadTransferSize -= (streaming_maxpacket / 1024) * 128;
	}

	memset(ctrl, 0, sizeof * ctrl);
	ctrl->bmHint                   = 1;
	ctrl->bFormatIndex             = iformat;
	ctrl->bFrameIndex              = iframe;
	ctrl->dwMaxVideoFrameSize      = get_frame_size(frame_format->video_format, frame_format->wWidth, frame_format->wHeight);
	ctrl->dwMaxPayloadTransferSize = dwMaxPayloadTransferSize;
	ctrl->dwFrameInterval          = frame_interval;
	ctrl->bmFramingInfo            = 3;
	ctrl->bMinVersion              = format_first;
	ctrl->bMaxVersion              = format_last;
	ctrl->bPreferedVersion         = format_last;

	dump_uvc_streaming_control(ctrl);

	if (uvc_dev.control == UVC_VS_COMMIT_CONTROL && action == STREAM_CONTROL_SET) {
		// TODO set 'source' parameters
	}
}

static void uvc_events_process_streaming(uint8_t req, uint8_t cs, struct uvc_request_data * resp)
{
	printf("UVC: Streaming request CS: _s, REQ: %s\n", /*uvc_vs_interface_control_name(cs), */ uvc_request_code_name(req));

	if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
		return;

	struct uvc_streaming_control * ctrl = (struct uvc_streaming_control *) &resp->data;
	struct uvc_streaming_control * target = (cs == UVC_VS_PROBE_CONTROL) ? &(uvc_dev.probe) : &(uvc_dev.commit);

	int ctrl_length = sizeof * ctrl;
	resp->length = ctrl_length;

	switch (req) {
		case UVC_SET_CUR:
			uvc_dev.control = cs;
			resp->length = ctrl_length;
			break;

		case UVC_GET_MAX:
			uvc_fill_streaming_control(ctrl, STREAM_CONTROL_MAX, 0, 0);
			break;

		case UVC_GET_CUR:
			memcpy(ctrl, target, ctrl_length);
			break;

		case UVC_GET_MIN:
		case UVC_GET_DEF:
			uvc_fill_streaming_control(ctrl, STREAM_CONTROL_MIN, 0, 0);
			break;

		case UVC_GET_RES:
			CLEAR(ctrl);
			break;

		case UVC_GET_LEN:
			resp->data[0] = 0x00;
			resp->data[1] = ctrl_length;
			resp->length = 2;
			break;

		case UVC_GET_INFO:
			resp->data[0] = (uint8_t)(UVC_CONTROL_CAP_GET | UVC_CONTROL_CAP_SET);
			resp->length = 1;
			break;
	}
}

static void uvc_interface_control(unsigned int interface, uint8_t req, uint8_t cs, uint8_t len, struct uvc_request_data * resp)
{
	int i;
	bool found = false;
	const char * request_code_name = uvc_request_code_name(req);
	const char * interface_name = (interface == UVC_VC_INPUT_TERMINAL) ? "INPUT_TERMINAL" : "PROCESSING_UNIT";

	for (i = 0; i < control_mapping_size; i++) {
		if (control_mapping[i].type == interface && control_mapping[i].uvc == cs) {
			found = true;
			break;
		}
	}

	if (!found) {
		printf("UVC: %s - %s - %02x - UNSUPPORTED\n", interface_name, request_code_name, cs);
		resp->length = -EL2HLT;
		uvc_dev.request_error_code = REQEC_INVALID_CONTROL;
		return;
	}

	if (!control_mapping[i].enabled) {
		printf("UVC: %s - %s - %s - DISABLED\n", interface_name, request_code_name,
				control_mapping[i].uvc_name);
		resp->length = -EL2HLT;
		uvc_dev.request_error_code = REQEC_INVALID_CONTROL;
		return;
	}

	printf("UVC: %s - %s - %s\n", interface_name, request_code_name, control_mapping[i].uvc_name);

	switch (req) {
		case UVC_SET_CUR:
			resp->data[0] = 0x0;
			resp->length = len;
			uvc_dev.control_interface = interface;
			uvc_dev.control_type = cs;
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		case UVC_GET_MIN:
			resp->length = 4;
			memcpy(&resp->data[0], &control_mapping[i].minimum, resp->length);
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		case UVC_GET_MAX:
			resp->length = 4;
			memcpy(&resp->data[0], &control_mapping[i].maximum, resp->length);
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		case UVC_GET_CUR:
			resp->length = 4;
			memcpy(&resp->data[0], &control_mapping[i].value, resp->length);
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		case UVC_GET_INFO:
			resp->data[0] = (uint8_t)(UVC_CONTROL_CAP_GET | UVC_CONTROL_CAP_SET);
			resp->length = 1;
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		case UVC_GET_DEF:
			resp->length = 4;
			memcpy(&resp->data[0], &control_mapping[i].default_value, resp->length);
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		case UVC_GET_RES:
			resp->length = 4;
			memcpy(&resp->data[0], &control_mapping[i].step, resp->length);
			uvc_dev.request_error_code = REQEC_NO_ERROR;
			break;

		default:
			resp->length = -EL2HLT;
			uvc_dev.request_error_code = REQEC_INVALID_REQUEST;
			break;

	}
	return;
}

static void uvc_events_process_class(struct usb_ctrlrequest * ctrl, struct uvc_request_data * resp)
{
	uint8_t type      = ctrl->wIndex & 0xff;
	uint8_t interface = ctrl->wIndex >> 8;
	uint8_t control   = ctrl->wValue >> 8;
	uint8_t length    = ctrl->wLength;

	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
		return;

	switch (type) {
		case UVC_INTF_CONTROL:
			printf("UVC_INTF_CONTROL %d\n", interface);

			switch (interface) {
				case 0:
					if (control == UVC_VC_REQUEST_ERROR_CODE_CONTROL) {
						resp->data[0] = uvc_dev.request_error_code;
						resp->length = 1;
					}
					break;

				case 1:
					uvc_interface_control(UVC_VC_INPUT_TERMINAL, ctrl->bRequest, control, length, resp);
					break;

				case 2:
					uvc_interface_control(UVC_VC_PROCESSING_UNIT, ctrl->bRequest, control, length, resp);
					break;

				default:
					break;
			}
			break;

		case UVC_INTF_STREAMING:
			printf("UVC_INTF_STREAMING\n");

			uvc_events_process_streaming(ctrl->bRequest, control, resp);
			break;

		default:
			break;
	}
}

static void uvc_events_process_setup(struct usb_ctrlrequest * ctrl, struct uvc_request_data * resp)
{
	uvc_dev.control = 0;

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS)
		uvc_events_process_class(ctrl, resp);

	if (ioctl(uvc_dev.fd, UVCIOC_SEND_RESPONSE, resp) == -1)
		log(LL_ERR, "UVCIOC_SEND_RESPONSE failed: %s (%d)", strerror(errno), errno);
}

static void uvc_events_process_data_control(struct uvc_request_data * data, struct uvc_streaming_control * target)
{
	uvc_streaming_control *ctrl    = (struct uvc_streaming_control *) &data->data;
	unsigned int           iformat = (unsigned int) ctrl->bFormatIndex;
	unsigned int           iframe  = (unsigned int) ctrl->bFrameIndex;

	uvc_fill_streaming_control(target, STREAM_CONTROL_SET, iformat, iframe);
}

static void uvc_events_process_data(struct uvc_request_data * data)
{
	// printf("UVC: Control %s, length: %d\n", uvc_vs_interface_control_name(uvc_dev.control), data->length);

	switch (uvc_dev.control) {
		case UVC_VS_PROBE_CONTROL:
			uvc_events_process_data_control(data, &(uvc_dev.probe));
			break;

		case UVC_VS_COMMIT_CONTROL:
			uvc_events_process_data_control(data, &(uvc_dev.commit));
			break;

		case UVC_VS_CONTROL_UNDEFINED:
			if (data->length > 0 && data->length <= 4) {
				for(int i = 0; i < control_mapping_size; i++) {
					if (control_mapping[i].type == uvc_dev.control_interface && control_mapping[i].uvc == uvc_dev.control_type && control_mapping[i].enabled) {
						control_mapping[i].value = 0x00000000;
						control_mapping[i].length = data->length;
						memcpy(&control_mapping[i].value, data->data, data->length);
						// TODO v4l2_set_ctrl(control_mapping[i]);
					}
				}
			}
			break;

		default:
			printf("UVC: Setting unknown control, length = %d\n", data->length);
			break;
	}
}

static void uvc_handle_streamon_event(const int fd)
{
	v4l2_format v { 0 };
	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (ioctl(fd, VIDIOC_G_FMT, &v))
		log(LL_ERR, "VIDIOC_G_FMT failed: %s", strerror(errno));

	printf("dim: %dx%d\n", v.fmt.pix.width, v.fmt.pix.height);
	printf("pixelformat: %c%c%c%c\n", v.fmt.pix.pixelformat >> 24, v.fmt.pix.pixelformat >> 16, v.fmt.pix.pixelformat >> 8, v.fmt.pix.pixelformat);
	// ok
}

static void uvc_handle_streamoff_event()
{
	// ok
}

void target_usbgadget::process_event(const int fd)
{
	v4l2_event       v4l2_event { 0 };
	uvc_event       *uvc_event  { reinterpret_cast<struct uvc_event *>(&v4l2_event.u.data) };

	uvc_request_data resp       { 0 };
	resp.length = -EL2HLT;

	if (ioctl(fd, VIDIOC_DQEVENT, &v4l2_event) == -1) {
		log(id, LL_ERR, "VIDIOC_DQEVENT failed: %s", strerror(errno));
		return;
	}

	resp.length = -EL2HLT;

	switch(v4l2_event.type) {
		case UVC_EVENT_CONNECT:
			log(id, LL_INFO, "%s: UVC_EVENT_CONNECT\n", uvc_dev.device_type_name);
			break;

		case UVC_EVENT_DISCONNECT:
			log(id, LL_INFO, "%s: UVC_EVENT_DISCONNECT\n", uvc_dev.device_type_name);
			local_stop_flag = true;
			break;

		case UVC_EVENT_SETUP:
			uvc_events_process_setup(&uvc_event->req, &resp);
			break;

		case UVC_EVENT_DATA:
			uvc_events_process_data(&uvc_event->data);
			break;

		case UVC_EVENT_STREAMON:
			uvc_handle_streamon_event(fd);
			break;

		case UVC_EVENT_STREAMOFF:
			uvc_handle_streamoff_event();
			break;

		default:
			log(id, LL_FATAL, "Unhandled case in target_usbgadget::process_event %u", v4l2_event.type);
			break;
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

	int fd = -1;
	std::vector<uint8_t *> buffers;
	int buffer_nr = 0;
	int nbufs = 4;

	bool setup_performed = false;

	video_frame *prev_frame = nullptr;

	pollfd fds[] = { { -1, POLLIN | POLLPRI | POLLERR, 0 } };

	for(;!local_stop_flag;) {
		pauseCheck();

		st->track_fps();

		uint64_t before_ts = get_us();

		if (fd != -1 && poll(fds, 1, 0) > 0)
			process_event(fd);

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

				fds[0].fd = fd;

				// TODO
				uvc_dev.fd = fd;

				uvc_frame_format[0].defined = true;
				uvc_frame_format[0].usb_speed = USB_SPEED_HIGH;
				uvc_frame_format[0].video_format = V4L2_PIX_FMT_YUYV;
				uvc_frame_format[0].format_name = "YUYV";
				uvc_frame_format[0].bFormatIndex = 1;
				uvc_frame_format[0].bFrameIndex = 1;
				uvc_frame_format[0].dwDefaultFrameInterval = interval * 1000000;
				uvc_frame_format[0].dwMaxVideoFrameBufferSize = fixed_width * 2 * fixed_height;
				uvc_frame_format[0].dwMaxBitRate = fixed_width * 2 * fixed_height * 8;
				uvc_frame_format[0].dwMinBitRate = 1;
				uvc_frame_format[0].wHeight = fixed_height;
				uvc_frame_format[0].wWidth = fixed_width;
				uvc_frame_format[0].bmCapabilities = 0;
				uvc_frame_format[0].dwFrameInterval = interval * 1000000;
				last_format_index = 1;

				v4l2_capability cap { 0 };
				if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
					log(id, LL_ERR, "VIDIOC_QUERYCAP for %s failed: %s", dev_name.value().c_str(), strerror(errno));
					break;
				}

				printf("driver: %s\n",   cap.driver);
				printf("card: %s\n",     cap.card);
				printf("bus_info: %s\n", cap.bus_info);

				if ((cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) == 0) {
					log(id, LL_ERR, "%s is not a video output device: %s", dev_name.value().c_str(), strerror(errno));
					break;
				}

				for(int i=UVC_EVENT_FIRST; i<=UVC_EVENT_LAST; i++) {
					v4l2_event_subscription sub { 0 };
					sub.type = i;

					if (ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub) == -1) {
						log(id, LL_ERR, "Cannot VIDIOC_SUBSCRIBE_EVENT::%x", sub.type);
						break;
					}
				}

				log(id, LL_DEBUG, "Device %s is on card %s on bus %s", dev_name.value().c_str(), cap.card, cap.bus_info);

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

				int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
				if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
					log(id, LL_ERR, "VIDIOC_STREAMON failed: %s", strerror(errno));
					break;
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
