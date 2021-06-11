// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <string>
#include <cstring>
#include <time.h>

#include "filter_add_text.h"
#include "filter_add_bitmap.h"
#include "error.h"
#include "utils.h"

filter_add_bitmap::filter_add_bitmap(const std::string & which, const int x, const int y) : which(which), x(x), y(y)
{
}

filter_add_bitmap::~filter_add_bitmap()
{
}

void filter_add_bitmap::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
        int work_x = x < 0 ? x + w : x;
        int work_y = y < 0 ? y + h : y;

	std::pair<uint64_t, video_frame *> it;

	if (specific_int->get_meta()->get_bitmap(which, &it)) {
		int putw = std::min(w - work_x, it.second->get_w());
		int puth = std::min(h - work_y, it.second->get_h());

		uint8_t *data = it.second->get_data(E_RGB);

		for(int puty=0; puty<puth; puty++)
			memcpy(&in_out[(puty + work_y) * w * 3 + work_x * 3], &data[puty * it.second->get_w() * 3], putw * 3);

		delete it.second;
	}
}
