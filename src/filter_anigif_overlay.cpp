// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#if HAVE_IMAGICK == 1
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <Magick++.h>

#include "error.h"
#include "filter_anigif_overlay.h"
#include "picio.h"
#include "log.h"

filter_anigif_overlay::filter_anigif_overlay(const std::string & file, const pos_t & pos) : pos(pos)
{
	Magick::InitializeMagick(nullptr);

	std::vector<Magick::Image> tempList;
	Magick::readImages(&tempList, file.c_str());

	Magick::coalesceImages(&imageList, tempList.begin(), tempList.end());

	log(LL_INFO, "%zu pages in animation %s", imageList.size(), file.c_str());
}

filter_anigif_overlay::~filter_anigif_overlay()
{
}

void filter_anigif_overlay::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	Magick::Image & m = imageList.at((ts / 100000) % imageList.size());

	int this_w = m.columns();
	int this_h = m.rows();

        auto p = pos_to_xy(pos, this_w, this_h, w, h);
	int work_x = std::get<0>(p);
	int work_y = std::get<1>(p);

	int cw = std::min(this_w, w);
	int ch = std::min(this_h, h);

	if (cw <= 0 || ch <= 0)
		return;

	int ex = std::min(w - work_x, cw);
	int ey = std::min(h - work_y, ch);

	const Magick::PixelPacket *pixels = m.getConstPixels(0, 0, this_w, this_h);

	for(int y=0; y<ey; y++) {
		const int ty = y + work_y;

		if (ty < 0)
			continue;
		if (ty >= h)
			break;

		for(int x=0; x<ex; x++) {
			const int tx = x + work_x;

			if (tx < 0)
				continue;
			if (tx >= w)
				break;

			const int in_offset = y * this_w + x;
			const int out_offset = ty * w * 3 + tx * 3;

			int opacity = pixels[in_offset].opacity;

			in_out[out_offset + 0] = (in_out[out_offset + 0] * opacity + pixels[in_offset].red * (65535 - opacity)) / 65536;
			in_out[out_offset + 1] = (in_out[out_offset + 1] * opacity + pixels[in_offset].green * (65535 - opacity)) / 65536;
			in_out[out_offset + 2] = (in_out[out_offset + 2] * opacity + pixels[in_offset].blue * (65535 - opacity)) / 65536;
		}
	}
}
#endif
