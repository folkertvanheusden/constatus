/*
 *	uvc_gadget.h  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/usb/ch9.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define clamp(val, min, max)                        \
    ({                                              \
        typeof(val) __val = (val);                  \
        typeof(min) __min = (min);                  \
        typeof(max) __max = (max);                  \
        (void)(&__val == &__min);                   \
        (void)(&__val == &__max);                   \
        __val = __val < __min ? __min : __val;      \
        __val > __max ? __max : __val;              \
    })

#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(a[0])))
#define pixfmtstr(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

enum gpio {
    GPIO_EXPORT = 0,
    GPIO_DIRECTION,
    GPIO_VALUE,
};

#define GPIO_DIRECTION_OUT "out"
#define GPIO_DIRECTION_IN "in"
#define GPIO_DIRECTION_LOW "low"
#define GPIO_DIRECTION_HIGH "high"

#define GPIO_VALUE_OFF "0"
#define GPIO_VALUE_ON "1"

enum leds {
    LED_TRIGGER = 1,
    LED_BRIGHTNESS,
};

#define LED_TRIGGER_NONE "none"
#define LED_BRIGHTNESS_LOW "0"
#define LED_BRIGHTNESS_HIGH "1"

#define UVC_EVENT_FIRST        (V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_CONNECT      (V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_DISCONNECT   (V4L2_EVENT_PRIVATE_START + 1)
#define UVC_EVENT_STREAMON     (V4L2_EVENT_PRIVATE_START + 2)
#define UVC_EVENT_STREAMOFF    (V4L2_EVENT_PRIVATE_START + 3)
#define UVC_EVENT_SETUP	       (V4L2_EVENT_PRIVATE_START + 4)
#define UVC_EVENT_DATA         (V4L2_EVENT_PRIVATE_START + 5)
#define UVC_EVENT_LAST         (V4L2_EVENT_PRIVATE_START + 5)

struct uvc_request_data
{
	__s32 length;
	__u8 data[60];
};

struct uvc_event
{
	union {
		enum usb_device_speed speed;
		struct usb_ctrlrequest req;
		struct uvc_request_data data;
	};
};

#define UVCIOC_SEND_RESPONSE		_IOW('U', 1, struct uvc_request_data)

#define UVC_INTF_CONTROL		0
#define UVC_INTF_STREAMING		1

// UVC - Request Error Code Control
#define REQEC_NO_ERROR 0x00
#define REQEC_NOT_READY 0x01
#define REQEC_WRONG_STATE 0x02
#define REQEC_POWER 0x03
#define REQEC_OUT_OF_RANGE 0x04
#define REQEC_INVALID_UNIT 0x05
#define REQEC_INVALID_CONTROL 0x06
#define REQEC_INVALID_REQUEST 0x07
#define REQEC_INVALID_VALUE 0x08

enum video_stream_action {
    STREAM_OFF,
    STREAM_ON,
};

enum stream_control_action {
    STREAM_CONTROL_INIT,
    STREAM_CONTROL_MIN,
    STREAM_CONTROL_MAX,
    STREAM_CONTROL_SET,
};

/* Buffer representing one video frame */
struct buffer {
    struct v4l2_buffer buf;
    void * start;
    size_t length;
};

/* ---------------------------------------------------------------------------
 * UVC specific stuff
 */

struct uvc_frame_format {
    bool defined;

    enum usb_device_speed usb_speed;
    int video_format;
    const char * format_name;

    unsigned int bFormatIndex;
    unsigned int bFrameIndex;

    unsigned int dwDefaultFrameInterval;
    unsigned int dwMaxVideoFrameBufferSize;
    unsigned int dwMaxBitRate;
    unsigned int dwMinBitRate;
    unsigned int wHeight;
    unsigned int wWidth;
    unsigned int bmCapabilities;
    
    unsigned int dwFrameInterval;
};

int last_format_index = 0;

struct uvc_frame_format uvc_frame_format[30];

enum uvc_frame_format_getter {
    FORMAT_INDEX_MIN,
    FORMAT_INDEX_MAX,
    FRAME_INDEX_MIN,
    FRAME_INDEX_MAX,
};

unsigned int streaming_maxburst = 0;
unsigned int streaming_maxpacket = 1023;
unsigned int streaming_interval = 1;

/* ---------------------------------------------------------------------------
 * V4L2 and UVC device instances
 */

bool uvc_shutdown_requested = false;

/* device type */
enum device_type {
    DEVICE_TYPE_UVC,
    DEVICE_TYPE_V4L2,
    DEVICE_TYPE_FRAMEBUFFER,
};

/* Represents a V4L2 based video capture device */
struct v4l2_device {
    enum device_type device_type;
    const char * device_type_name;

    /* v4l2 device specific */
    int fd;
    int is_streaming;

    /* v4l2 buffer specific */
    struct buffer * mem;
    unsigned int nbufs;
    unsigned int buffer_type;
    unsigned int memory_type;

    /* v4l2 buffer queue and dequeue counters */
    unsigned long long int qbuf_count;
    unsigned long long int dqbuf_count;

    /* uvc specific */
    int run_standalone;
    int control;
    struct uvc_streaming_control probe;
    struct uvc_streaming_control commit;
    unsigned char request_error_code;
    unsigned int control_interface;
    unsigned int control_type;

    /* uvc specific flags */
    int uvc_shutdown_requested;

    struct buffer * dummy_buf;

    /* fb specific */
    unsigned int fb_screen_size;
    unsigned int fb_mem_size;
    unsigned int fb_width;
    unsigned int fb_height;
    unsigned int fb_bpp;
    unsigned int fb_line_length;
    void * fb_memory;

    double last_time_video_process;
    int buffers_processed;
};

static struct v4l2_device v4l2_dev;
static struct v4l2_device uvc_dev;
static struct v4l2_device fb_dev;

struct uvc_settings {
    char * uvc_devname { nullptr };
    char * v4l2_devname { nullptr };
    char * fb_devname { nullptr };
    enum device_type source_device { DEVICE_TYPE_V4L2 };
    unsigned int nbufs { 4 };
    bool show_fps { false };
    bool fb_grayscale { false };
    unsigned int fb_framerate { 25 };
    bool streaming_status_onboard { false };
    bool streaming_status_onboard_enabled { false };
    char * streaming_status_pin { nullptr };
    bool streaming_status_enabled { false };
    unsigned int blink_on_startup { 0 };
};

#if 0
struct uvc_settings settings = {
    .uvc_devname = "/dev/video1",
    .v4l2_devname = "/dev/video0",
    .source_device = DEVICE_TYPE_V4L2,
    .nbufs = 2,
    .fb_framerate = 25,
    .fb_grayscale = false,
    .show_fps = false,
    .streaming_status_onboard = false,
    .streaming_status_onboard_enabled = false,
    .streaming_status_enabled = false,
    .blink_on_startup = 0
};
#endif

struct control_mapping_pair {
    unsigned int type;
    unsigned int uvc;
    const char * uvc_name;
    unsigned int v4l2;
    const char * v4l2_name;
    bool enabled;
    unsigned int control_type;
    unsigned int value;
    unsigned int length;
    unsigned int minimum;
    unsigned int maximum;
    unsigned int step;
    unsigned int default_value;
    int v4l2_minimum;
    int v4l2_maximum;
};

struct control_mapping_pair control_mapping[] = {
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_BACKLIGHT_COMPENSATION_CONTROL,
		.uvc_name = "UVC_PU_BACKLIGHT_COMPENSATION_CONTROL",
        .v4l2 = V4L2_CID_BACKLIGHT_COMPENSATION,
        .v4l2_name = "V4L2_CID_BACKLIGHT_COMPENSATION",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_BRIGHTNESS_CONTROL,
		.uvc_name = "UVC_PU_BRIGHTNESS_CONTROL",
        .v4l2 = V4L2_CID_BRIGHTNESS,
        .v4l2_name = "V4L2_CID_BRIGHTNESS"
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_CONTRAST_CONTROL,
		.uvc_name = "UVC_PU_CONTRAST_CONTROL",
        .v4l2 = V4L2_CID_CONTRAST,
        .v4l2_name = "V4L2_CID_CONTRAST",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_GAIN_CONTROL,
		.uvc_name = "UVC_PU_GAIN_CONTROL",
        .v4l2 = V4L2_CID_GAIN,
        .v4l2_name = "V4L2_CID_GAIN",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_POWER_LINE_FREQUENCY_CONTROL,
		.uvc_name = "UVC_PU_POWER_LINE_FREQUENCY_CONTROL",
        .v4l2 = V4L2_CID_POWER_LINE_FREQUENCY,
        .v4l2_name = "V4L2_CID_POWER_LINE_FREQUENCY",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_HUE_CONTROL,
		.uvc_name = "UVC_PU_HUE_CONTROL",
        .v4l2 = V4L2_CID_HUE,
        .v4l2_name = "V4L2_CID_HUE",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_SATURATION_CONTROL,
		.uvc_name = "UVC_PU_SATURATION_CONTROL",
        .v4l2 = V4L2_CID_SATURATION,
        .v4l2_name = "V4L2_CID_SATURATION",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_SHARPNESS_CONTROL,
		.uvc_name = "UVC_PU_SHARPNESS_CONTROL",
        .v4l2 = V4L2_CID_SHARPNESS,
        .v4l2_name = "V4L2_CID_SHARPNESS",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_GAMMA_CONTROL,
		.uvc_name = "UVC_PU_GAMMA_CONTROL",
        .v4l2 = V4L2_CID_GAMMA,
        .v4l2_name = "V4L2_CID_GAMMA",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL",
        .v4l2 = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
        .v4l2_name = "V4L2_CID_WHITE_BALANCE_TEMPERATURE",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL",
        .v4l2 = V4L2_CID_RED_BALANCE,
        .v4l2_name = "V4L2_CID_RED_BALANCE + V4L2_CID_BLUE_BALANCE"
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL",
        .v4l2 = V4L2_CID_AUTO_WHITE_BALANCE,
        .v4l2_name = "V4L2_CID_AUTO_WHITE_BALANCE",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_DIGITAL_MULTIPLIER_CONTROL,
		.uvc_name = "UVC_PU_DIGITAL_MULTIPLIER_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL,
		.uvc_name = "UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_HUE_AUTO_CONTROL,
		.uvc_name = "UVC_PU_HUE_AUTO_CONTROL",
        .v4l2 = V4L2_CID_HUE_AUTO,
        .v4l2_name = "V4L2_CID_HUE_AUTO",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL,
		.uvc_name = "UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_ANALOG_LOCK_STATUS_CONTROL,
		.uvc_name = "UVC_PU_ANALOG_LOCK_STATUS_CONTROL",
	},
	// {
    //     .type = UVC_VC_PROCESSING_UNIT,
	// 	.uvc = UVC_PU_CONTRAST_AUTO_CONTROL,
	// 	.uvc_name = "UVC_PU_CONTRAST_AUTO_CONTROL",
	// },
    {
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_CONTROL_UNDEFINED,
		.uvc_name = "UVC_CT_CONTROL_UNDEFINED",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_SCANNING_MODE_CONTROL,
		.uvc_name = "UVC_CT_SCANNING_MODE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_AE_MODE_CONTROL,
		.uvc_name = "UVC_CT_AE_MODE_CONTROL",
        .v4l2 = V4L2_CID_EXPOSURE_AUTO,
        .v4l2_name = "V4L2_CID_EXPOSURE_AUTO"
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_AE_PRIORITY_CONTROL,
		.uvc_name = "UVC_CT_AE_PRIORITY_CONTROL",
        .v4l2 = V4L2_CID_EXPOSURE_AUTO_PRIORITY,
        .v4l2_name = "V4L2_CID_EXPOSURE_AUTO_PRIORITY"
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL",
        .v4l2 = V4L2_CID_EXPOSURE_ABSOLUTE,
        .v4l2_name = "V4L2_CID_EXPOSURE_ABSOLUTE"
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_FOCUS_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_FOCUS_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_FOCUS_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_FOCUS_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_FOCUS_AUTO_CONTROL,
		.uvc_name = "UVC_CT_FOCUS_AUTO_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_IRIS_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_IRIS_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_IRIS_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_IRIS_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ZOOM_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_ZOOM_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ZOOM_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_ZOOM_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_PANTILT_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_PANTILT_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_PANTILT_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_PANTILT_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ROLL_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_ROLL_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ROLL_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_ROLL_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_PRIVACY_CONTROL,
		.uvc_name = "UVC_CT_PRIVACY_CONTROL",
	}
};

int control_mapping_size = sizeof(control_mapping) / sizeof(* control_mapping);

/*
 * RGB to YUYV conversion 
 */

unsigned int mult_38[256] = {0, 38, 76, 114, 152, 190, 228, 266, 304, 342, 380, 418, 456, 494, 532,
    570, 608, 646, 684, 722, 760, 798, 836, 874, 912, 950, 988, 1026, 1064, 1102, 1140, 1178, 1216,
    1254, 1292, 1330, 1368, 1406, 1444, 1482, 1520, 1558, 1596, 1634, 1672, 1710, 1748, 1786, 1824,
    1862, 1900, 1938, 1976, 2014, 2052, 2090, 2128, 2166, 2204, 2242, 2280, 2318, 2356, 2394, 2432,
    2470, 2508, 2546, 2584, 2622, 2660, 2698, 2736, 2774, 2812, 2850, 2888, 2926, 2964, 3002, 3040,
    3078, 3116, 3154, 3192, 3230, 3268, 3306, 3344, 3382, 3420, 3458, 3496, 3534, 3572, 3610, 3648,
    3686, 3724, 3762, 3800, 3838, 3876, 3914, 3952, 3990, 4028, 4066, 4104, 4142, 4180, 4218, 4256,
    4294, 4332, 4370, 4408, 4446, 4484, 4522, 4560, 4598, 4636, 4674, 4712, 4750, 4788, 4826, 4864,
    4902, 4940, 4978, 5016, 5054, 5092, 5130, 5168, 5206, 5244, 5282, 5320, 5358, 5396, 5434, 5472,
    5510, 5548, 5586, 5624, 5662, 5700, 5738, 5776, 5814, 5852, 5890, 5928, 5966, 6004, 6042, 6080,
    6118, 6156, 6194, 6232, 6270, 6308, 6346, 6384, 6422, 6460, 6498, 6536, 6574, 6612, 6650, 6688,
    6726, 6764, 6802, 6840, 6878, 6916, 6954, 6992, 7030, 7068, 7106, 7144, 7182, 7220, 7258, 7296,
    7334, 7372, 7410, 7448, 7486, 7524, 7562, 7600, 7638, 7676, 7714, 7752, 7790, 7828, 7866, 7904,
    7942, 7980, 8018, 8056, 8094, 8132, 8170, 8208, 8246, 8284, 8322, 8360, 8398, 8436, 8474, 8512,
    8550, 8588, 8626, 8664, 8702, 8740, 8778, 8816, 8854, 8892, 8930, 8968, 9006, 9044, 9082, 9120,
    9158, 9196, 9234, 9272, 9310, 9348, 9386, 9424, 9462, 9500, 9538, 9576, 9614, 9652, 9690
};

unsigned int mult_74[256] = {0, 74, 148, 222, 296, 370, 444, 518, 592, 666, 740, 814, 888, 962,
    1036, 1110, 1184, 1258, 1332, 1406, 1480, 1554, 1628, 1702, 1776, 1850, 1924, 1998, 2072, 2146,
    2220, 2294, 2368, 2442, 2516, 2590, 2664, 2738, 2812, 2886, 2960, 3034, 3108, 3182, 3256, 3330,
    3404, 3478, 3552, 3626, 3700, 3774, 3848, 3922, 3996, 4070, 4144, 4218, 4292, 4366, 4440, 4514,
    4588, 4662, 4736, 4810, 4884, 4958, 5032, 5106, 5180, 5254, 5328, 5402, 5476, 5550, 5624, 5698,
    5772, 5846, 5920, 5994, 6068, 6142, 6216, 6290, 6364, 6438, 6512, 6586, 6660, 6734, 6808, 6882,
    6956, 7030, 7104, 7178, 7252, 7326, 7400, 7474, 7548, 7622, 7696, 7770, 7844, 7918, 7992, 8066,
    8140, 8214, 8288, 8362, 8436, 8510, 8584, 8658, 8732, 8806, 8880, 8954, 9028, 9102, 9176, 9250,
    9324, 9398, 9472, 9546, 9620, 9694, 9768, 9842, 9916, 9990, 10064, 10138, 10212, 10286, 10360,
    10434, 10508, 10582, 10656, 10730, 10804, 10878, 10952, 11026, 11100, 11174, 11248, 11322, 11396,
    11470, 11544, 11618, 11692, 11766, 11840, 11914, 11988, 12062, 12136, 12210, 12284, 12358, 12432,
    12506, 12580, 12654, 12728, 12802, 12876, 12950, 13024, 13098, 13172, 13246, 13320, 13394, 13468,
    13542, 13616, 13690, 13764, 13838, 13912, 13986, 14060, 14134, 14208, 14282, 14356, 14430, 14504,
    14578, 14652, 14726, 14800, 14874, 14948, 15022, 15096, 15170, 15244, 15318, 15392, 15466, 15540,
    15614, 15688, 15762, 15836, 15910, 15984, 16058, 16132, 16206, 16280, 16354, 16428, 16502, 16576,
    16650, 16724, 16798, 16872, 16946, 17020, 17094, 17168, 17242, 17316, 17390, 17464, 17538, 17612,
    17686, 17760, 17834, 17908, 17982, 18056, 18130, 18204, 18278, 18352, 18426, 18500, 18574, 18648,
    18722, 18796, 18870
};

unsigned int mult_112[256] = {0, 112, 224, 336, 448, 560, 672, 784, 896, 1008, 1120, 1232, 1344, 1456,
    1568, 1680, 1792, 1904, 2016, 2128, 2240, 2352, 2464, 2576, 2688, 2800, 2912, 3024, 3136, 3248,
    3360, 3472, 3584, 3696, 3808, 3920, 4032, 4144, 4256, 4368, 4480, 4592, 4704, 4816, 4928, 5040,
    5152, 5264, 5376, 5488, 5600, 5712, 5824, 5936, 6048, 6160, 6272, 6384, 6496, 6608, 6720, 6832,
    6944, 7056, 7168, 7280, 7392, 7504, 7616, 7728, 7840, 7952, 8064, 8176, 8288, 8400, 8512, 8624,
    8736, 8848, 8960, 9072, 9184, 9296, 9408, 9520, 9632, 9744, 9856, 9968, 10080, 10192, 10304,
    10416, 10528, 10640, 10752, 10864, 10976, 11088, 11200, 11312, 11424, 11536, 11648, 11760, 11872,
    11984, 12096, 12208, 12320, 12432, 12544, 12656, 12768, 12880, 12992, 13104, 13216, 13328, 13440,
    13552, 13664, 13776, 13888, 14000, 14112, 14224, 14336, 14448, 14560, 14672, 14784, 14896, 15008,
    15120, 15232, 15344, 15456, 15568, 15680, 15792, 15904, 16016, 16128, 16240, 16352, 16464, 16576,
    16688, 16800, 16912, 17024, 17136, 17248, 17360, 17472, 17584, 17696, 17808, 17920, 18032, 18144,
    18256, 18368, 18480, 18592, 18704, 18816, 18928, 19040, 19152, 19264, 19376, 19488, 19600, 19712,
    19824, 19936, 20048, 20160, 20272, 20384, 20496, 20608, 20720, 20832, 20944, 21056, 21168, 21280,
    21392, 21504, 21616, 21728, 21840, 21952, 22064, 22176, 22288, 22400, 22512, 22624, 22736, 22848,
    22960, 23072, 23184, 23296, 23408, 23520, 23632, 23744, 23856, 23968, 24080, 24192, 24304, 24416,
    24528, 24640, 24752, 24864, 24976, 25088, 25200, 25312, 25424, 25536, 25648, 25760, 25872, 25984,
    26096, 26208, 26320, 26432, 26544, 26656, 26768, 26880, 26992, 27104, 27216, 27328, 27440, 27552,
    27664, 27776, 27888, 28000, 28112, 28224, 28336, 28448, 28560
};

unsigned int mult_94[256] = {0, 94, 188, 282, 376, 470, 564, 658, 752, 846, 940, 1034, 1128, 1222,
    1316, 1410, 1504, 1598, 1692, 1786, 1880, 1974, 2068, 2162, 2256, 2350, 2444, 2538, 2632, 2726,
    2820, 2914, 3008, 3102, 3196, 3290, 3384, 3478, 3572, 3666, 3760, 3854, 3948, 4042, 4136, 4230,
    4324, 4418, 4512, 4606, 4700, 4794, 4888, 4982, 5076, 5170, 5264, 5358, 5452, 5546, 5640, 5734,
    5828, 5922, 6016, 6110, 6204, 6298, 6392, 6486, 6580, 6674, 6768, 6862, 6956, 7050, 7144, 7238,
    7332, 7426, 7520, 7614, 7708, 7802, 7896, 7990, 8084, 8178, 8272, 8366, 8460, 8554, 8648, 8742,
    8836, 8930, 9024, 9118, 9212, 9306, 9400, 9494, 9588, 9682, 9776, 9870, 9964, 10058, 10152, 10246,
    10340, 10434, 10528, 10622, 10716, 10810, 10904, 10998, 11092, 11186, 11280, 11374, 11468, 11562,
    11656, 11750, 11844, 11938, 12032, 12126, 12220, 12314, 12408, 12502, 12596, 12690, 12784, 12878,
    12972, 13066, 13160, 13254, 13348, 13442, 13536, 13630, 13724, 13818, 13912, 14006, 14100, 14194,
    14288, 14382, 14476, 14570, 14664, 14758, 14852, 14946, 15040, 15134, 15228, 15322, 15416, 15510,
    15604, 15698, 15792, 15886, 15980, 16074, 16168, 16262, 16356, 16450, 16544, 16638, 16732, 16826,
    16920, 17014, 17108, 17202, 17296, 17390, 17484, 17578, 17672, 17766, 17860, 17954, 18048, 18142,
    18236, 18330, 18424, 18518, 18612, 18706, 18800, 18894, 18988, 19082, 19176, 19270, 19364, 19458,
    19552, 19646, 19740, 19834, 19928, 20022, 20116, 20210, 20304, 20398, 20492, 20586, 20680, 20774,
    20868, 20962, 21056, 21150, 21244, 21338, 21432, 21526, 21620, 21714, 21808, 21902, 21996, 22090,
    22184, 22278, 22372, 22466, 22560, 22654, 22748, 22842, 22936, 23030, 23124, 23218, 23312, 23406,
    23500, 23594, 23688, 23782, 23876, 23970
};

unsigned int mult_18[256] = {128, 146, 164, 182, 200, 218, 236, 254, 272, 290, 308, 326, 344, 362,
    380, 398, 416, 434, 452, 470, 488, 506, 524, 542, 560, 578, 596, 614, 632, 650, 668, 686, 704,
    722, 740, 758, 776, 794, 812, 830, 848, 866, 884, 902, 920, 938, 956, 974, 992, 1010, 1028, 1046,
    1064, 1082, 1100, 1118, 1136, 1154, 1172, 1190, 1208, 1226, 1244, 1262, 1280, 1298, 1316, 1334,
    1352, 1370, 1388, 1406, 1424, 1442, 1460, 1478, 1496, 1514, 1532, 1550, 1568, 1586, 1604, 1622,
    1640, 1658, 1676, 1694, 1712, 1730, 1748, 1766, 1784, 1802, 1820, 1838, 1856, 1874, 1892, 1910,
    1928, 1946, 1964, 1982, 2000, 2018, 2036, 2054, 2072, 2090, 2108, 2126, 2144, 2162, 2180, 2198,
    2216, 2234, 2252, 2270, 2288, 2306, 2324, 2342, 2360, 2378, 2396, 2414, 2432, 2450, 2468, 2486,
    2504, 2522, 2540, 2558, 2576, 2594, 2612, 2630, 2648, 2666, 2684, 2702, 2720, 2738, 2756, 2774,
    2792, 2810, 2828, 2846, 2864, 2882, 2900, 2918, 2936, 2954, 2972, 2990, 3008, 3026, 3044, 3062,
    3080, 3098, 3116, 3134, 3152, 3170, 3188, 3206, 3224, 3242, 3260, 3278, 3296, 3314, 3332, 3350,
    3368, 3386, 3404, 3422, 3440, 3458, 3476, 3494, 3512, 3530, 3548, 3566, 3584, 3602, 3620, 3638,
    3656, 3674, 3692, 3710, 3728, 3746, 3764, 3782, 3800, 3818, 3836, 3854, 3872, 3890, 3908, 3926,
    3944, 3962, 3980, 3998, 4016, 4034, 4052, 4070, 4088, 4106, 4124, 4142, 4160, 4178, 4196, 4214,
    4232, 4250, 4268, 4286, 4304, 4322, 4340, 4358, 4376, 4394, 4412, 4430, 4448, 4466, 4484, 4502,
    4520, 4538, 4556, 4574, 4592, 4610, 4628, 4646, 4664, 4682, 4700, 4718
};

#define rgb2yvyu(r1, g1, b1, r2, g2, b2)                                                 \
    ({                                                                                   \
        uint8_t r12 = (r1 + r2) >> 1;                                                    \
        uint8_t g12 = (g1 + g2) >> 1;                                                    \
        uint8_t b12 = (b1 + b2) >> 1;                                                    \
        (uint8_t) ((r1 >> 2) + (g1 >> 1) + (b1 >> 3) + 16) +                             \
        ((uint8_t)(((mult_112[r12] - mult_94[g12] -  mult_18[b12]) >> 8) + 128) << 8) +  \
        ((uint8_t)((r2 >> 2) + (g2 >> 1) + (b2 >> 3) + 16) << 16) +                      \
        ((uint8_t)(((-mult_38[r12] - mult_74[g12] + mult_112[b12]) >> 8) + 128) << 24);  \
    })
