// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "gen.h"
#include "filter_median.h"
#include "log.h"
#include "utils.h"

filter_median::filter_median(const size_t median_n) : median_n(median_n)
{
}

filter_median::~filter_median()
{
	for(auto p : h_frames)
		free(p);
}

void filter_median::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (h_frames.size() > median_n) {
		free(h_frames.at(0));
		h_frames.erase(h_frames.begin());
	}

	size_t n_bytes = IMS(w, h, 3);
	uint8_t *copy = (uint8_t *)duplicate(in_out, n_bytes);

	h_frames.push_back(copy);

	if (h_frames.size() < median_n)
		return;

	std::vector<int> values;
	values.resize(median_n);

	for(size_t p=0; p<n_bytes; p++) {
		for(size_t a=0; a<median_n; a++)
			values.at(a) = h_frames.at(a)[p];

		std::sort(values.begin(), values.end());

		in_out[p] = values.at(median_n / 2);
	}
}
