// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "gen.h"
#include "filter_copy.h"
#include "log.h"
#include "utils.h"

filter_copy::filter_copy(const std::string & remember_as, const int x, const int y, const int w, const int h) : remember_as(remember_as), x(x), y(y), w(w), h(h)
{
}

filter_copy::~filter_copy()
{
}

void filter_copy::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
        int work_x = this->x < 0 ? this->x + w : this->x;
        int work_y = this->y < 0 ? this->y + h : this->y;

	size_t n_bytes = IMS(this->w, this->h, 3);
	uint8_t *data = (uint8_t *)malloc(n_bytes);

	for(int y=0; y<this->h; y++)
		memcpy(&data[y * this->w * 3], &in_out[(work_y + y) * w * 3 + work_x * 3], this->w * 3);

	meta *const m = specific_int->get_meta();

	video_frame *vf = new video_frame(m, 100, ts, this->w, this->h, data, n_bytes, E_RGB);

	m -> set_bitmap(remember_as, std::pair<uint64_t, video_frame *>(0, vf));
}
