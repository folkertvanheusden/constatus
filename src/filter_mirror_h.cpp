// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stddef.h>
#include <cstring>

#include "gen.h"
#include "filter_mirror_h.h"

filter_mirror_h::filter_mirror_h()
{
}

filter_mirror_h::~filter_mirror_h()
{
}

void filter_mirror_h::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	for(int y=0; y<h; y++) {
		const int yoff = y * w * 3;

		for(int x=0; x<w; x++) {
			int oin = yoff + x * 3;
			int oout = yoff + (w - x - 1) * 3;

			out[oout + 0] = in[oin + 0];
			out[oout + 1] = in[oin + 1];
			out[oout + 2] = in[oin + 2];
		}
	}
}
