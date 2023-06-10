// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include "ptz.h"

class ptz_v4l : public ptz
{
private:
	const int fd;

	ptz_v4l(const int fd);

	bool pan_relative(const double angle);
	bool tilt_relative(const double angle);
	bool pan_absolute(const double angle);
	bool tilt_absolute(const double angle);

public:
	virtual ~ptz_v4l();

	static ptz_v4l * check_is_supported(const int fd);

	void reset_to_center();

	virtual void operator()();
};
