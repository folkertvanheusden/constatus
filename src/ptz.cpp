// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "log.h"
#include "ptz.h"
#include "ptz_v4l.h"

ptz::ptz() : interface("ptz", "")
{
}

ptz::~ptz()
{
}

ptz * ptz::check_is_supported(const int fd)
{
	ptz *rc = nullptr;

	rc = ptz_v4l::check_is_supported(fd);
	if (rc)
		return rc;

	log(LL_INFO, "No PTZ controls");

	return nullptr;
}

void ptz::pan(const double abs_angle)
{
	if (!pan_absolute(abs_angle))
		pan_relative(abs_angle - angle_pan);
}

void ptz::tilt(const double abs_angle)
{
	if (!tilt_absolute(abs_angle))
		tilt_relative(abs_angle - angle_tilt);
}

void ptz::operator()()
{
}
