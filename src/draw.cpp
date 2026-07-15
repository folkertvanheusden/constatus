// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include <stdint.h>
#include "gen.h"

void draw_horizonal(uint8_t *const target, const int tw, const int x, const int y, const int w, const rgb_t col)
{
	int offset = y * tw * 4;

	for(int drawx=x; drawx<x+w; drawx++) {
		target[offset + drawx * 4 + 0] = col.r;
		target[offset + drawx * 4 + 1] = col.g;
		target[offset + drawx * 4 + 2] = col.b;
	}
}

void draw_vertical(uint8_t *const target, const int tw, const int x, const int y, const int h, const rgb_t col)
{
	int offset = x * 4;

	for(int drawy=y; drawy<y+h; drawy++) {
		target[offset + drawy * tw * 4 + 0] = col.r;
		target[offset + drawy * tw * 4 + 1] = col.g;
		target[offset + drawy * tw * 4 + 2] = col.b;
	}
}

void draw_box(uint8_t *const target, const int tw, const int x1, const int y1, const int x2, const int y2, const rgb_t col)
{
	for(int drawy=y1; drawy<y2; drawy++) {
		int offset = drawy * tw * 4;

		for(int drawx=x1; drawx<x2; drawx++) {
			target[offset + drawx * 4 + 0] = col.r;
			target[offset + drawx * 4 + 1] = col.g;
			target[offset + drawx * 4 + 2] = col.b;
		}
	}
}
