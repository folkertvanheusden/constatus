// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <cstring>

#include "error.h"
#include "filter_overlay.h"
#include "picio.h"

filter_overlay::filter_overlay(const std::string & file, const pos_t & pos) : pos(pos)
{
	FILE *fh = fopen(file.c_str(), "rb");
	if (!fh)
		error_exit(true, "%s failed to read", file.c_str());

	read_PNG_file_rgba(true, fh, &w, &h, &pixels);

	fclose(fh);
}

filter_overlay::~filter_overlay()
{
	free(pixels);
}

void filter_overlay::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
        auto p = pos_to_xy(pos, this->w, this->h, w, h);
	int work_x = std::get<0>(p);
	int work_y = std::get<1>(p);

	int cw = std::min(this->w, w);
	int ch = std::min(this->h, h);

	if (cw <= 0 || ch <= 0)
		return;

	int ex = std::min(w - work_x, cw);
	int ey = std::min(h - work_y, ch);

	for(int y=0; y<ey; y++) {
		const int ty = y + work_y;

		for(int x=0; x<ex; x++) {
			const int tx = x + work_x;

			const int out_offset = ty * w * 3 + tx * 3;
			const int pic_offset = y * this->w * 4 + x * 4;

			uint8_t alpha = pixels[pic_offset + 3], ialpha = 255 - alpha;
			in_out[out_offset + 0] = (int(pixels[pic_offset + 0]) * alpha + (in_out[out_offset + 0] * ialpha)) / 256;
			in_out[out_offset + 1] = (int(pixels[pic_offset + 1]) * alpha + (in_out[out_offset + 1] * ialpha)) / 256;
			in_out[out_offset + 2] = (int(pixels[pic_offset + 2]) * alpha + (in_out[out_offset + 2] * ialpha)) / 256;
		}
	}
}
