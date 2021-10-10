// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stdlib.h>
#include <cstring>

#include "resize.h"
#include "log.h"
#include "utils.h"
#include "pos.h"

void picture_in_picture(uint8_t *const tgt, const int tgt_w, const int tgt_h, const uint8_t *const in, const int win, const int hin, const pos_t pos)
{
	auto p = pos_to_xy(pos, win, hin, tgt_w, tgt_h);

	if (pos.type != none) {
		int tx = std::get<0>(p);
		int ty = std::get<1>(p);

		int width = std::min(win, tgt_w - win);

		if (width > 0) {
			int x_offset = 0;

			if (tx < 0) {
				x_offset = -tx;
				tx = 0;
			}

			for(int y=0; y<hin; y++) {
				int yo = y + ty;

				if (yo < 0)
					continue;
				if (yo >= tgt_h)
					break;

				memcpy(&tgt[yo * tgt_w * 3 + tx * 3], &in[y * win * 3 + x_offset * 3], (width - x_offset) * 3);
			}
		}
	}
}

void picture_in_picture(resize *const r, uint8_t *const tgt, const int tgt_w, const int tgt_h, const uint8_t *const in, const int win, const int hin, const int perc, const pos_t pos)
{
	int wout = win * perc / 100;
	int hout = hin * perc / 100;

	uint8_t *temp = nullptr;
	r->do_resize(win, hin, in, wout, hout, &temp);

	picture_in_picture(tgt, tgt_w, tgt_h, temp, wout, hout, pos);

	free(temp);
}

resize::resize()
{
	log(LL_INFO, "Generic resizer instantiated");
}

resize::~resize()
{
}

void resize::do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	*out = (uint8_t *)malloc(IMS(wout, hout, 3));

	const double maxw = std::max(win, wout);
	const double maxh = std::max(hin, hout);

	const double hins = hin / maxh;
	const double wins = win / maxw;
	const double houts = hout / maxh;
	const double wouts = wout / maxw;

	for(int y=0; y<maxh; y++) {
		const int out_scaled_y = y * houts;
		if (out_scaled_y >= hout)
			break;

		const int out_scaled_o = out_scaled_y * wout * 3;

		const int in_scaled_y = y * hins;
		const int in_scaled_o = in_scaled_y * win * 3;

		for(int x=0; x<maxw; x++) {
			int ino = in_scaled_o + int(x * wins) * 3;
			int outo = out_scaled_o + int(x * wouts) * 3;

			(*out)[outo + 0] = in[ino + 0];
			(*out)[outo + 1] = in[ino + 1];
			(*out)[outo + 2] = in[ino + 2];
		}
	}
}
