// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "gen.h"
#include "filter_motion_only.h"
#include "log.h"
#include "utils.h"
#include "selection_mask.h"

filter_motion_only::filter_motion_only(selection_mask *const pixel_select_bitmap, const int noise_level, const bool diff_value) : pixel_select_bitmap(pixel_select_bitmap), noise_level(noise_level), diff_value(diff_value)
{
	prev1 = prev2 = nullptr;
}

filter_motion_only::~filter_motion_only()
{
	delete pixel_select_bitmap;

	free(prev1);
	free(prev2);
}

void filter_motion_only::apply(instance *const inst, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (!prev)
		return;

	size_t n = IMS(w, h, 3);

	if (prev1 == nullptr) {
		prev1 = (uint8_t *)malloc(n);
		prev2 = (uint8_t *)malloc(n);
	}

	memcpy(prev1, in_out, n);

	const int nl3 = noise_level * 3;

	uint8_t *psb = pixel_select_bitmap ? pixel_select_bitmap -> get_mask(w, h) : nullptr;

	if (psb) {
		for(int i=0; i<w*h; i++) {
			if (!psb[i])
				continue;

			int i3 = i * 3;

			int diff = abs(in_out[i3 + 0] - prev2[i3 + 0]) + abs(in_out[i3 + 1] - prev2[i3 + 1]) + abs(in_out[i3 + 2] - prev2[i3 + 2]);

			if (diff < nl3)
				in_out[i3 + 0] = in_out[i3 + 1] = in_out[i3 + 2] = 0;
			else if (diff_value) {
				in_out[i3 + 0] = in_out[i3 + 1] = in_out[i3 + 2] = diff / 3;
			}
		}
	}
	else {
		for(int i=0; i<w*h*3; i += 3) {
			int diff = abs(in_out[i + 0] - prev2[i + 0]) + abs(in_out[i + 1] - prev2[i + 1]) + abs(in_out[i + 2] - prev2[i + 2]);

			if (diff < nl3)
				in_out[i + 0] = in_out[i + 1] = in_out[i + 2] = 0;
			else if (diff_value) {
				in_out[i + 0] = in_out[i + 1] = in_out[i + 2] = diff / 3;
			}
		}
	}

	memcpy(prev2, prev1, n);
}
