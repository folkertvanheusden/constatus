// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_LIBV4L2 == 1
#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include "controls_v4l.h"
#include "log.h"

controls_v4l::controls_v4l(const int fd) : fd(fd)
{
        struct v4l2_queryctrl queryctrl { 0 };

        queryctrl.id = V4L2_CID_BRIGHTNESS;
 
	if (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0) {
		brightness_min = queryctrl.minimum;
		default_brightness = queryctrl.default_value;
		brightness_max = queryctrl.maximum;
	}

        queryctrl.id = V4L2_CID_CONTRAST;
 
	if (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0) {
		contrast_min = queryctrl.minimum;
		default_contrast = queryctrl.default_value;
		contrast_max = queryctrl.maximum;
	}

        queryctrl.id = V4L2_CID_SATURATION;
 
	if (ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0) {
		saturation_min = queryctrl.minimum;
		default_saturation = queryctrl.default_value;
		saturation_max = queryctrl.maximum;
	}
}

controls_v4l::~controls_v4l()
{
}

void controls_v4l::reset()
{
	struct v4l2_control control { 0 };

	control.id = V4L2_CID_BRIGHTNESS;
	control.value = default_brightness;
	ioctl(fd, VIDIOC_S_CTRL, &control);

	control.id = V4L2_CID_CONTRAST;
	control.value = default_contrast;
	ioctl(fd, VIDIOC_S_CTRL, &control);

	control.id = V4L2_CID_SATURATION;
	control.value = default_saturation;
	ioctl(fd, VIDIOC_S_CTRL, &control);
}

bool controls_v4l::has_brightness()
{
	struct v4l2_queryctrl queryctrl { 0 };
	queryctrl.id = V4L2_CID_BRIGHTNESS;

	return ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 && (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) == 0;
}

int controls_v4l::get_brightness()
{
	struct v4l2_control control { 0 };
	control.id = V4L2_CID_BRIGHTNESS;

	if (ioctl(fd, VIDIOC_G_CTRL, &control))
		log(LL_ERR, "VIDIOC_G_CTRL for V4L2_CID_BRIGHTNESS failed: %s", strerror(errno));

	return (control.value - brightness_min) * 65536 / (brightness_max - brightness_min);
}

void controls_v4l::set_brightness(const int b)
{
	struct v4l2_control control { 0 };
	control.id = V4L2_CID_BRIGHTNESS;
	control.value = b * (brightness_max - brightness_min) / 65536 + brightness_min;

	if (ioctl(fd, VIDIOC_S_CTRL, &control))
		log(LL_ERR, "VIDIOC_S_CTRL for V4L2_CID_BRIGHTNESS failed: %s", strerror(errno));
}

bool controls_v4l::has_contrast()
{
	struct v4l2_queryctrl queryctrl { 0 };
	queryctrl.id = V4L2_CID_CONTRAST;

	return ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 && (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) == 0;
}

int controls_v4l::get_contrast()
{
	struct v4l2_control control { 0 };
	control.id = V4L2_CID_CONTRAST;

	if (ioctl(fd, VIDIOC_G_CTRL, &control))
		log(LL_ERR, "VIDIOC_G_CTRL for V4L2_CID_CONTRAST failed: %s", strerror(errno));

	return (control.value - contrast_min) * 65536 / (contrast_max - contrast_min);
}

void controls_v4l::set_contrast(const int c)
{
	struct v4l2_control control { 0 };
	control.id = V4L2_CID_CONTRAST;
	control.value = c * (contrast_max - contrast_min) / 65536 + contrast_min;

	if (ioctl(fd, VIDIOC_S_CTRL, &control))
		log(LL_ERR, "VIDIOC_S_CTRL for V4L2_CID_CONTRAST failed: %s", strerror(errno));
}

bool controls_v4l::has_saturation()
{
	struct v4l2_queryctrl queryctrl { 0 };
	queryctrl.id = V4L2_CID_SATURATION;

	return ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 && (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) == 0;
}

int controls_v4l::get_saturation()
{
	struct v4l2_control control { 0 };
	control.id = V4L2_CID_SATURATION;

	if (ioctl(fd, VIDIOC_G_CTRL, &control))
		log(LL_ERR, "VIDIOC_G_CTRL for V4L2_CID_SATURATION failed: %s", strerror(errno));

	return (control.value - saturation_min) * 65536 / (saturation_max - saturation_min);
}

void controls_v4l::set_saturation(const int s)
{
	struct v4l2_control control { 0 };
	control.id = V4L2_CID_SATURATION;
	control.value = s * (saturation_max - saturation_min) / 65536 + saturation_min;

	if (ioctl(fd, VIDIOC_S_CTRL, &control))
		log(LL_ERR, "VIDIOC_S_CTRL for V4L2_CID_SATURATION failed: %s", strerror(errno));
}

void controls_v4l::apply(uint8_t *const target, const int w, const int h)
{
	// v4l2 has hardware support for setting these
	// settings, so this method is empty
}
#endif
