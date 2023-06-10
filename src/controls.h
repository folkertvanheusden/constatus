// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <stdint.h>

class controls
{
protected:
	int default_brightness { 0 }, default_contrast { 0 }, default_saturation { 0 };

public:
	controls();
	virtual ~controls();

	virtual void reset();

	virtual bool has_controls() const { return false; }

	virtual bool has_brightness();
	virtual int get_brightness();
	virtual void set_brightness(const int b);

	virtual bool has_contrast();
	virtual int get_contrast();
	virtual void set_contrast(const int c);

	virtual bool has_saturation();
	virtual int get_saturation();
	virtual void set_saturation(const int s);

	virtual bool requires_apply() { return false; }
	virtual void apply(uint8_t *const target, const int w, const int h);
};
