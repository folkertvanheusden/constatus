// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include "interface.h"

class ptz : public interface
{
protected:
	double angle_pan { 0. }, angle_tilt { 0. };

	virtual bool pan_relative(const double angle) = 0;
	virtual bool tilt_relative(const double angle) = 0;
	virtual bool pan_absolute(const double angle) = 0;
	virtual bool tilt_absolute(const double angle) = 0;

public:
	ptz();
	virtual ~ptz();

	static ptz * check_is_supported(const int fd);

	virtual void pan(const double abs_angle);
	virtual void tilt(const double abs_angle);
	virtual void reset_to_center() = 0;

	double get_pan() const { return angle_pan; }
	double get_tilt() const { return angle_tilt; }

	virtual void operator()();
};
