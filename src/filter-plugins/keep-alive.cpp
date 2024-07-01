// (C) 2024 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <cairo/cairo.h>

#include "cairo.h"
#include "../utils.h"

extern "C" {
void * init_filter(const char *const parameter)
{
	return new uint64_t();
}

void apply_filter(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	uint32_t *temp = NULL;
	cairo_surface_t *const cs = rgb_to_cairo(current_frame, w, h, &temp);
	cairo_t *const cr = cairo_create(cs);

	uint64_t t_diff = ts - *reinterpret_cast<uint64_t *>(arg);
	if (t_diff >= 500000) {
		int r = std::min(w, h) / 20;  // 5%
		///
		cairo_set_source_rgb(cr, 0.6, 0.8, 1.0);
		cairo_move_to(cr, r * 1.5, r * 1.5);
		cairo_arc(cr, r * 1.5, r * 1.5, r, 0.0, 2 * M_PI);
		cairo_fill(cr);
		cairo_stroke(cr);
		///
		if (t_diff >= 1000000)
			*reinterpret_cast<uint64_t *>(arg) = ts;
	}

	cairo_to_rgb(cs, w, h, result);

	cairo_destroy(cr);
	cairo_surface_destroy(cs);
	free(temp);
}

void uninit_filter(void *arg)
{
	delete reinterpret_cast<uint64_t *>(arg);
}
}
