// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "gen.h"
#include "filter_average.h"
#include "log.h"
#include "utils.h"

filter_average::filter_average(const size_t average_n) : average_n(average_n)
{
}

filter_average::~filter_average()
{
	for(auto p : h_frames)
		free(p);
}

void filter_average::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (h_frames.size() > average_n) {
		free(h_frames.at(0));
		h_frames.erase(h_frames.begin());
	}

	size_t n_bytes = IMS(w, h, 3);
	uint8_t *copy = (uint8_t *)malloc(n_bytes);
	memcpy(copy, in_out, n_bytes);
	h_frames.push_back(copy);

	if (h_frames.size() < average_n)
		return;

	for(size_t p=0; p<n_bytes; p++) {
		int v = 0;

		for(size_t a=0; a<average_n; a++)
			v += h_frames.at(a)[p];

		in_out[p] = v / average_n;
	}
}
