// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <cstring>

#include "gen.h"
#include "filter_boost_contrast.h"

filter_boost_contrast::filter_boost_contrast()
{
}

filter_boost_contrast::~filter_boost_contrast()
{
}

void filter_boost_contrast::apply(instance *const inst, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	uint8_t lowest_br = 255, highest_br = 0;

	for(int i=0; i<w*h*3; i+=3) {
		int g = (in_out[i + 0] + in_out[i + 1] + in_out[i + 2]) / 3;

		if (g < lowest_br)
			lowest_br = g;
		else if (g > highest_br)
			highest_br = g;
	}

	if (highest_br == lowest_br || (highest_br == 255 && lowest_br == 0))
		return;

	double mul = 255.0 / (highest_br - lowest_br);
	// printf("%f\n", mul);

	uint8_t *p = in_out;
	uint8_t *const pend = p + w * h * 3;

	for(;p < pend; p++)
		*p = (*p - lowest_br) * mul;
}
