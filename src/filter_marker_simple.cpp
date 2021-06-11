// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "gen.h"
#include "filter_marker_simple.h"
#include "log.h"
#include "motion_trigger.h"
#include "utils.h"
#include "selection_mask.h"

void update_pixel(uint8_t *const out, const int x, const int y, const int w, const sm_pixel_t m)
{
	const int o = y * w * 3 + x * 3;

	switch(m.mode) {
		case m_red:
			out[o + 0] = 255;
			break;

		case m_red_invert:
			out[o + 0] ^= 255;
			break;

		case m_invert:
			out[o + 0] ^= 255;
			out[o + 1] ^= 255;
			out[o + 2] ^= 255;
			break;

		case m_color:
			out[o + 0] = m.r;
			out[o + 1] = m.g;
			out[o + 2] = m.b;
			break;
	}
}

void draw_circle_do(uint8_t *const tgt, const int w, int xc, int yc, int x, int y, const sm_pixel_t m)
{
	update_pixel(tgt, xc + x, yc + y, w, m);
	update_pixel(tgt, xc - x, yc + y, w, m);
	update_pixel(tgt, xc + x, yc - y, w, m);
	update_pixel(tgt, xc - x, yc - y, w, m);
	update_pixel(tgt, xc + y, yc + x, w, m);
	update_pixel(tgt, xc - y, yc + x, w, m);
	update_pixel(tgt, xc + y, yc - x, w, m);
	update_pixel(tgt, xc - y, yc - x, w, m);
}

void draw_circle_bresenham(uint8_t *const tgt, const int w, int xc, int yc, int r, const sm_pixel_t m)
{
	int x = 0, y = r;
	int d = 3 - 2 * r;

	draw_circle_do(tgt, w, xc, yc, x, y, m);

	while (y >= x) {
		x++;

		if (d > 0) {
			y--;
			d = d + 4 * (x - y) + 10;
		}
		else {
			d = d + 4 * x + 6;
		}

		draw_circle_do(tgt, w, xc, yc, x, y, m);
	}
}

void draw_border(uint8_t *const tgt, const int width, const int height, const int border_width)
{
	for(int bw=0; bw<border_width; bw++) {
		int add = 255 * (border_width - bw);

		for(int x=0; x<width; x++) {
			int offset1 = x * 3 + bw * width * 3;
			tgt[offset1] = (add + tgt[offset1] * bw) / border_width;
			int offset2 = x * 3 + (height - bw - 1) * width * 3;
			tgt[offset2] = (add + tgt[offset2] * bw) / border_width;
		}

		for(int y=0; y<height; y++) {
			int offset1 = y * width * 3 + bw * 3;
			tgt[offset1] = (add + tgt[offset1] * bw) / border_width;
			int offset2 = y * width * 3 + (width - bw - 1) * 3;
			tgt[offset2] = (add + tgt[offset2] * bw) / border_width;
		}
	}
}

filter_marker_simple::filter_marker_simple(instance *const i, const sm_pixel_t pixelIn, const sm_type_t type, selection_mask *const sb, const int noise_level, const double percentage_pixels_changed, const int thick, const bool store_motion_bitmap) : pixel(pixelIn), type(type), pixel_select_bitmap(sb), noise_level(noise_level), percentage_pixels_changed(percentage_pixels_changed), thick(thick), i(i), store_motion_bitmap(store_motion_bitmap)
{
	for(auto mi : find_motion_triggers(i))
		static_cast<motion_trigger *>(mi)->register_notifier(this);
}

filter_marker_simple::~filter_marker_simple()
{
	for(auto mi : find_motion_triggers(i))
		static_cast<motion_trigger *>(mi)->unregister_notifier(this);

	delete pixel_select_bitmap;
}

void filter_marker_simple::notify_thread_of_event(const std::string & subject)
{
	changed = true;
}

void filter_marker_simple::apply(instance *const inst, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (!prev)
		return;

        bool motion = false;
        if (this->i) { // when we're waiting for an other object to signal us
                motion = changed.exchange(false);

                if (!motion)
                        return;
        }

	bool *diffs = nullptr;

	const int nl3 = noise_level * 3;

	uint8_t *psb = pixel_select_bitmap ? pixel_select_bitmap -> get_mask(w, h) : NULL;

	int cn = 0;

	if (psb) {
		diffs = new bool[w * h]();

		for(int i=0; i<w*h; i++) {
			if (!psb[i])
				continue;

			int i3 = i * 3;

			int diff = abs(in_out[i3 + 0] - prev[i3 + 0]) + abs(in_out[i3 + 1] - prev[i3 + 1]) + abs(in_out[i3 + 2] - prev[i3 + 2]);

			cn += diffs[i] = diff >= nl3;
		}
	}
	else {
		diffs = new bool[w * h];

		for(int i=0; i<w*h*3; i += 3) {
			int diff = abs(in_out[i + 0] - prev[i + 0]) + abs(in_out[i + 1] - prev[i + 1]) + abs(in_out[i + 2] - prev[i + 2]);

			cn += diffs[i / 3] = diff >= nl3;
		}
	}

	if (cn < percentage_pixels_changed * w * h / 100 || cn == 0) {
		delete [] diffs;
		return;
	}

	if (type == t_border) {
		draw_border(in_out, w, h, thick);
		delete [] diffs;
		return;
	}

	int cx = 0, cy = 0;

	for(int o=0; o<w*h; o++) {
		if (diffs[o]) {
			int x = o % w;
			int y = o / w;

			cx += x;
			cy += y;
		}
	}

	cx /= cn;
	cy /= cn;

	meta *const m = specific_int->get_meta();

	if (m) {
		m -> set_int("$motion-center-x$", std::pair<uint64_t, int>(0, cx));
		m -> set_int("$motion-center-y$", std::pair<uint64_t, int>(0, cy));
	}

	int xdist = 0, ydist = 0;

	for(int o=0; o<w*h; o++) {
		if (diffs[o]) {
			int x = o % w;
			int y = o / w;

			xdist += abs(x - cx);
			ydist += abs(y - cy);
		}
	}

	delete [] diffs;

	xdist /= cn;
	ydist /= cn;

	if (m) {
		m -> set_int("$motion-width$", std::pair<uint64_t, int>(0, xdist));
		m -> set_int("$motion-height$", std::pair<uint64_t, int>(0, ydist));
	}

	int xmin = std::max(0, cx - xdist), xmax = std::min(w - 1, cx + xdist);
	int ymin = std::max(0, cy - ydist), ymax = std::min(h - 1, cy + ydist);

	if (store_motion_bitmap && m) {
		int wbox = xmax - xmin;
		int hbox = ymax - ymin;

		size_t n_bytes = IMS(wbox, hbox, 3);
		uint8_t *data = (uint8_t *)malloc(n_bytes);

		for(int y=0; y<hbox; y++)
			memcpy(&data[y * wbox * 3], &in_out[(ymin + y) * w * 3 + xmin * 3], wbox * 3);

		video_frame *vf = new video_frame(m, 100, ts, wbox, hbox, data, n_bytes, E_RGB);

		m -> set_bitmap("$motion-box$", std::pair<uint64_t, video_frame *>(0, vf));
	}

	log(id, LL_DEBUG_VERBOSE, "%d,%d - %d,%d", xmin, ymin, xmax, ymax);

	for(int d=0; d<thick; d++) {
		if (type == t_box) {
			for(int y=ymin + d; y<ymax - d; y++) {
				update_pixel(in_out, xmin, y, w, pixel);
				update_pixel(in_out, xmax, y, w, pixel);
			}

			for(int x=xmin + d; x<xmax - d; x++) {
				update_pixel(in_out, x, ymin, w, pixel);
				update_pixel(in_out, x, ymax, w, pixel);
			}
		}
		else if (type == t_cross) {
			for(int y=ymin + d; y<ymax - d; y++)
				update_pixel(in_out, cx, y, w, pixel);

			for(int x=xmin + d; x<xmax - d; x++)
				update_pixel(in_out, x, cy, w, pixel);
		}
		else if (type == t_circle) {
			draw_circle_bresenham(in_out, w, cx, cy, std::min(xdist, ydist) - d, pixel);
		}
	}
}
