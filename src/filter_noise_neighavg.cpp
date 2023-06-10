// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stdlib.h>

#include "gen.h"
#include "filter_noise_neighavg.h"

filter_noise_neighavg::filter_noise_neighavg()
{
}

filter_noise_neighavg::~filter_noise_neighavg()
{
}

void filter_noise_neighavg::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	for(int y = 1; y<h-1; y++) {
		int yo = y * w * 3;

		for(int x=1; x<w-1; x++) {
			int out1 = 0, out2 = 0, out3 = 0;

			for(int yy=-1; yy<=1; yy++) {
				int yyo = (y + yy) * w * 3;

				for(int xx=-1; xx<=1; xx++) {
					if (xx == 0 && yy == 0)
						continue;

					out1 += in[yyo + (x + xx) * 3 + 0];
					out2 += in[yyo + (x + xx) * 3 + 1];
					out3 += in[yyo + (x + xx) * 3 + 2];
				}
			}

			out[yo + x * 3 + 0] = out1 / 8;
			out[yo + x * 3 + 1] = out2 / 8;
			out[yo + x * 3 + 2] = out3 / 8;
		}
	}
}
