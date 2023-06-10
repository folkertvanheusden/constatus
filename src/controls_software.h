// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "controls.h"

class controls_software : public controls
{
private:
	int brightness{ 32767 }, contrast{ 32767 }, saturation{ 32767 };

public:
	controls_software();
	virtual ~controls_software();

	virtual void reset();

	virtual bool has_controls() const { return true; }

	virtual bool has_brightness();
	virtual int get_brightness();
	virtual void set_brightness(const int b);

	virtual bool has_contrast();
	virtual int get_contrast();
	virtual void set_contrast(const int c);

	virtual bool has_saturation();
	virtual int get_saturation();
	virtual void set_saturation(const int s);

	virtual bool requires_apply() { return true; }
	virtual void apply(uint8_t *const target, const int w, const int h);
};
