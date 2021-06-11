// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <cstring>

#include "resize_crop.h"
#include "log.h"
#include "utils.h"

resize_crop::resize_crop(const bool resize_crop_center, const bool fill_max) : resize_crop_center(resize_crop_center), fill_max(fill_max)
{
	log(LL_INFO, "Generic resize_crop instantiated");
}

resize_crop::~resize_crop()
{
}

void resize_crop::do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	size_t out_size = IMS(wout, hout, 3);

	*out = (uint8_t *)allocate_0x00(out_size);

	if (fill_max) {
		// resize
		double scale_w = wout / double(win);
		double scale_h = hout / double(hin);

		double diff_w = fabs(1.0 - scale_w);
		double diff_h = fabs(1.0 - scale_h);

		int temp_w = 0, temp_h = 0;

		if (diff_w < diff_h) {
			temp_w = wout;
			temp_h = hout * scale_h;
		}
		else {
			temp_w = wout * scale_w;
			temp_h = hout;
		}

		uint8_t *temp = nullptr;
		resize::do_resize(win, hin, in, temp_w, temp_h, &temp);

		// crop
		int xoffset = resize_crop_center && wout > temp_w ? (wout - temp_w) / 2 : 0;
		int yoffset = resize_crop_center && hout > temp_h ? (hout - temp_h) / 2 : 0;

		int new_width = std::min(temp_w, wout);

		for(int y=0; y<std::min(temp_h, hout); y++)
			memcpy(&(*out)[(y + yoffset) * wout * 3 + xoffset * 3], &temp[y * temp_w * 3], new_width * 3);

		free(temp);
	}
	else {
		int xoffset = resize_crop_center && wout > win ? (wout - win) / 2 : 0;
		int yoffset = resize_crop_center && hout > hin ? (hout - hin) / 2 : 0;

		int new_width = std::min(win, wout);

		for(int y=0; y<std::min(hin, hout); y++)
			memcpy(&(*out)[(y + yoffset) * wout * 3 + xoffset * 3], &in[y * win * 3], new_width * 3);
	}
}
