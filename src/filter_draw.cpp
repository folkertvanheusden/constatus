// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <cstring>

#include "gen.h"
#include "error.h"
#include "filter_draw.h"
#include "picio.h"

filter_draw::filter_draw(const int x, const int y, const int w, const int h, const rgb_t col) : x(x), y(y), w(w), h(h), col(col)
{
}

filter_draw::~filter_draw()
{
}

void filter_draw::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w_in, const int h_in, const uint8_t *const prev, uint8_t *const in_out)
{
        int work_x = x < 0 ? x + w : x;
        int work_y = y < 0 ? y + h : y;

	int sx = std::min(work_x, w_in), sy = std::min(work_y, h_in);
	int ex = std::min(work_x + w, w_in), ey = std::min(work_y + h, h_in);

	for(int wy=sy; wy<ey; wy++) {
		for(int wx=sx; wx<ex; wx++) {
			const int out_offset = wy * w_in * 3 + wx * 3;

			in_out[out_offset + 0] = col.r;
			in_out[out_offset + 1] = col.g;
			in_out[out_offset + 2] = col.b;
		}
	}
}
