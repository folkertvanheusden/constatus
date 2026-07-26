// (C) 2024-2026 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <cairo/cairo.h>

#include "cairo.h"
#include "filter_keep_alive.h"
#include "utils.h"


filter_keep_alive::filter_keep_alive()
{
}

filter_keep_alive::~filter_keep_alive()
{
}

void filter_keep_alive::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	uint32_t *temp = NULL;
	cairo_surface_t *const cs = rgb_to_cairo(in, w, h, &temp);
	cairo_t *const cr = cairo_create(cs);

	uint64_t t_diff = ts - prev_ts;
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
			prev_ts = ts;
	}

	cairo_to_rgb(cs, w, h, out);

	cairo_destroy(cr);
	cairo_surface_destroy(cs);
	free(temp);
}
