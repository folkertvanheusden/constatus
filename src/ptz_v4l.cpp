// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include <errno.h>
#include <cstring>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include "ptz_v4l.h"
#include "log.h"

ptz_v4l::ptz_v4l(const int fd) : fd(fd)
{
	log(id, LL_INFO, "ptz/v4l instantiated");
}

ptz_v4l::~ptz_v4l()
{
}

bool ptz_v4l::pan_relative(const double angle)
{
        struct v4l2_control par_pan{ 0 };
        par_pan.id = V4L2_CID_PAN_RELATIVE;
	par_pan.value = angle * 100;

        if (ioctl(fd, VIDIOC_S_CTRL, &par_pan) == -1) {
		log(id, LL_DEBUG, "Cannot pan (relative) camera to %f (%d): %s", angle, par_pan.value, strerror(errno));
		return false;
	}

	angle_pan += angle;

	return true;
}

bool ptz_v4l::tilt_relative(const double angle)
{
        struct v4l2_control par_tilt{ 0 };
        par_tilt.id = V4L2_CID_TILT_RELATIVE;
	par_tilt.value = angle * 100;

        if (ioctl(fd, VIDIOC_S_CTRL, &par_tilt) == -1) {
		log(id, LL_DEBUG, "Cannot tilt (relative) camera to %f (%d): %s", angle, par_tilt.value, strerror(errno));
		return false;
	}

	angle_tilt += angle;

	return true;
}

bool ptz_v4l::pan_absolute(const double angle)
{
        struct v4l2_control par_pan{ 0 };
        par_pan.id = V4L2_CID_PAN_ABSOLUTE;
	par_pan.value = angle * 100;

        if (ioctl(fd, VIDIOC_S_CTRL, &par_pan) == -1) {
		log(id, LL_DEBUG, "Cannot pan (absolute) camera to %f (%d): %s", angle, par_pan.value, strerror(errno));
		return false;
	}

	angle_pan = angle;

	return true;
}

bool ptz_v4l::tilt_absolute(const double angle)
{
        struct v4l2_control par_tilt{ 0 };
        par_tilt.id = V4L2_CID_TILT_ABSOLUTE;
	par_tilt.value = angle * 100;

        if (ioctl(fd, VIDIOC_S_CTRL, &par_tilt) == -1) {
		log(id, LL_DEBUG, "Cannot tilt (absolute) camera to %f (%d): %s", angle, par_tilt.value, strerror(errno));
		return false;
	}

	angle_tilt = angle;

	return true;
}

void ptz_v4l::reset_to_center()
{
        struct v4l2_control par_pan{ 0 };
        par_pan.id = V4L2_CID_PAN_RESET;

        if (ioctl(fd, VIDIOC_S_CTRL, &par_pan) == -1)
		log(id, LL_DEBUG, "Cannot reset camera to center (pan): %s", strerror(errno));
	else
		angle_pan = 0;

        struct v4l2_control par_tilt{ 0 };
        par_tilt.id = V4L2_CID_TILT_RESET;

        if (ioctl(fd, VIDIOC_S_CTRL, &par_tilt) == -1)
		log(id, LL_DEBUG, "Cannot reset camera to center (tilt): %s", strerror(errno));
	else
		angle_tilt = 0;
}

ptz_v4l * ptz_v4l::check_is_supported(const int fd)
{
	// test pan/tilt reset
        struct v4l2_control par{ 0 };
        par.id = V4L2_CID_PAN_RESET;

        if (ioctl(fd, VIDIOC_S_CTRL, &par) == 0) {
		log(LL_INFO, "Device supports video4linux PTZ controls");
		return new ptz_v4l(fd);
	}

	return nullptr;
}

void ptz_v4l::operator()()
{
}
