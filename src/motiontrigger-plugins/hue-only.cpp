// (C) 2019 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

extern "C" {

void * init_motion_trigger(const char *const parameter)
{
	return nullptr;
}

// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
void rgb_to_hsv(const uint8_t r, const uint8_t g, const uint8_t b, uint8_t *const h, uint8_t *const s, uint8_t *const v)
{
	uint8_t rgb_min, rgb_max;

	rgb_min = r < g ? (r < b ? r : b) : (g < b ? g : b);
	rgb_max = r > g ? (r > b ? r : b) : (g > b ? g : b);
	uint8_t diff = rgb_max - rgb_min;

	*v =rgb_max;
	if (*v == 0)
	{
		*h = 0;
		*s = 0;
		return;
	}

	*s = 255 * long(diff) / *v;
	if (*s == 0)
	{
		*h = 0;
		return;
	}

	if (rgb_max == r)
		*h = 0 + 43 * (g - b) / diff;
	else if (rgb_max == g)
		*h = 85 + 43 * (b - r) / diff;
	else
		*h = 171 + 43 * (r - g) / diff;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const pf, uint8_t *const cf, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	int count = 0, idx = 0;

	for(int offset=0; offset<w*h*3; offset+=3) {
		uint8_t hc, sc, vc;
		uint8_t hp, sp, vp;

		rgb_to_hsv(pf[offset + 0], pf[offset + 1], pf[offset + 2], &hp, &sp, &vp);
		rgb_to_hsv(cf[offset + 0], cf[offset + 1], cf[offset + 2], &hc, &sc, &vc);

		if (pixel_selection_bitmap && !pixel_selection_bitmap[idx++])
			continue;

		count += abs(hc - hp) > 16;
	}

	return count > w * h * 0.01;
}

void uninit_motion_trigger(void *arg)
{
}

}
