// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stddef.h>
#include <cstring>

#include "gen.h"
#include "filter_grayscale.h"

filter_grayscale::filter_grayscale(const to_grayscale_t mode) : mode(mode)
{
}

filter_grayscale::~filter_grayscale()
{
}

void filter_grayscale::apply_io(instance *const inst, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *in, uint8_t *out)
{
	const int n_pixels = w * h;

	if (mode == G_FAST) {
		for(int i=0; i<n_pixels; i++) {
			int g = *in++;
			g += *in++;
			g += *in++;
			g /= 3;

			*out++ = g;
			*out++ = g;
			*out++ = g;
		}
	}
	else if (mode == G_CIE_1931) {
		for(int i=0; i<n_pixels; i++) {
			int r = *in++;
			int g = *in++;
			int b = *in++;

			int gray = (r * 2126 + g * 7152 + b * 722) / (21226 + 7152 + 722);

			*out++ = gray;
			*out++ = gray;
			*out++ = gray;
		}
	}
	else if (mode == G_PAL_NTSC) {
		for(int i=0; i<n_pixels; i++) {
			int r = *in++;
			int g = *in++;
			int b = *in++;

			int gray = (r * 299 + g * 587 + b * 114) / (299 + 587 + 114);

			*out++ = gray;
			*out++ = gray;
			*out++ = gray;
		}
	}
	else if (mode == G_LIGHTNESS) {
		for(int i=0; i<n_pixels; i++) {
			int r = *in++;
			int g = *in++;
			int b = *in++;

			int min_ = std::min(std::min(r, g), b);
			int max_ = std::max(std::max(r, g), b);

			int gray = (min_ + max_) / 2;

			*out++ = gray;
			*out++ = gray;
			*out++ = gray;
		}
	}
}
