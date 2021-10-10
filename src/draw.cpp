// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include <stdint.h>
#include "gen.h"

void draw_horizonal(uint8_t *const target, const int tw, const int x, const int y, const int w, const rgb_t col)
{
	int offset = y * tw * 3;

	for(int drawx=x; drawx<x+w; drawx++) {
		target[offset + drawx * 3 + 0] = col.r;
		target[offset + drawx * 3 + 1] = col.g;
		target[offset + drawx * 3 + 2] = col.b;
	}
}

void draw_vertical(uint8_t *const target, const int tw, const int x, const int y, const int h, const rgb_t col)
{
	int offset = x * 3;

	for(int drawy=y; drawy<y+h; drawy++) {
		target[offset + drawy * tw * 3 + 0] = col.r;
		target[offset + drawy * tw * 3 + 1] = col.g;
		target[offset + drawy * tw * 3 + 2] = col.b;
	}
}

void draw_box(uint8_t *const target, const int tw, const int x1, const int y1, const int x2, const int y2, const rgb_t col)
{
	for(int drawy=y1; drawy<y2; drawy++) {
		int offset = drawy * tw * 3;

		for(int drawx=x1; drawx<x2; drawx++) {
			target[offset + drawx * 3 + 0] = col.r;
			target[offset + drawx * 3 + 1] = col.g;
			target[offset + drawx * 3 + 2] = col.b;
		}
	}
}
