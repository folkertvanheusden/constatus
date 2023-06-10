// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "controls.h"

controls::controls()
{
}

controls::~controls()
{
}

void controls::reset()
{
}

bool controls::has_brightness()
{
	return false;
}

int controls::get_brightness()
{
	return 32767;
}

void controls::set_brightness(const int b)
{
}

bool controls::has_contrast()
{
	return false;
}

int controls::get_contrast()
{
	return 32767;
}

void controls::set_contrast(const int c)
{
}

bool controls::has_saturation()
{
	return false;
}

int controls::get_saturation()
{
	return 32767;
}

void controls::set_saturation(const int s)
{
}

void controls::apply(uint8_t *const target, const int w, const int h)
{
}
