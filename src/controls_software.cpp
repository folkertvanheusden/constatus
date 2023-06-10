// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "controls_software.h"
#include "utils.h"

controls_software::controls_software()
{
	default_brightness = default_contrast = default_saturation = 32767;
}

controls_software::~controls_software()
{
}

void controls_software::reset()
{
	brightness = default_brightness;
	contrast = default_contrast;
	saturation = default_saturation;
}

bool controls_software::has_brightness()
{
	return true;
}

int controls_software::get_brightness()
{
	return brightness;
}

void controls_software::set_brightness(const int b)
{
	brightness = b;
}

bool controls_software::has_contrast()
{
	return true;
}

int controls_software::get_contrast()
{
	return contrast;
}

void controls_software::set_contrast(const int c)
{
	contrast = c;
}

bool controls_software::has_saturation()
{
	return true;
}

int controls_software::get_saturation()
{
	return saturation;
}

void controls_software::set_saturation(const int s)
{
	saturation = s;
}

void controls_software::apply(uint8_t *const target, const int width, const int height)
{
	if (contrast == 32767 && brightness == 32767 && saturation == 32767)
		return;

	const int n_pixels = width * height;

	double apply_contrast = contrast / 32767.0; // max 2.0
	double apply_brightness = brightness / 32767.0; // max 2.0
	double apply_saturation = saturation / 32767.0; // max 2.0

	for(int i=0; i<n_pixels; i++) {
		const int o = i * 3;

		double h { 0. }, l { 0. }, s { 0. };
		rgb_to_hls(target[o + 0], target[o + 1], target[o + 2], &h, &s, &l);

		l *= apply_brightness;
		if (l > 1.)
			l = 1.;

		s *= apply_saturation;
		if (s > 1.)
			s = 1.;

		double R = 0., G = 0., B = 0.;
		hls_to_rgb(h / 360.0, l, s, &R, &G, &B);

		R = (R - 0.5) * apply_contrast + 0.5;
		G = (G - 0.5) * apply_contrast + 0.5;
		B = (B - 0.5) * apply_contrast + 0.5;

		if (R < 0.)
			R = 0.;
		else if (R > 1.0)
			R = 1.0;

		if (G < 0.)
			G = 0.;
		else if (G > 1.0)
			G = 1.0;

		if (B < 0.)
			B = 0.;
		else if (B > 1.0)
			B = 1.0;

		target[o + 0] = R * 255.;
		target[o + 1] = G * 255.;
		target[o + 2] = B * 255.;
	}
}
