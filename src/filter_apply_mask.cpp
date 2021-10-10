// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include <cstring>

#include "gen.h"
#include "filter_apply_mask.h"
#include "selection_mask.h"

filter_apply_mask::filter_apply_mask(selection_mask *const pixel_select_bitmap, const bool soft_mask) : psb(pixel_select_bitmap), soft_mask(soft_mask)
{
}

filter_apply_mask::~filter_apply_mask()
{
	delete psb;
}

void filter_apply_mask::apply(instance *const inst, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	const uint8_t *const sb = psb -> get_mask(w, h);

	if (soft_mask) {
		int tr = 0, tg = 0, tb = 0, tn = 0;

		for(int i=0, o=0; i<w*h; i++, o += 3) {
			if (sb[i])
				continue;

			tr += in_out[o + 0];
			tg += in_out[o + 1];
			tb += in_out[o + 2];
			tn++;

			in_out[o + 0] = tr / tn;
			in_out[o + 1] = tg / tn;
			in_out[o + 2] = tb / tn;
		}
	}
	else {
		for(int i=0, o=0; i<w*h; i++, o += 3) {
			if (sb[i])
				continue;

			in_out[o + 0] = in_out[o + 1] = in_out[o + 2] = 0;
		}
	}
}
