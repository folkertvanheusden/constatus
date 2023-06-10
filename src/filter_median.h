// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <stdint.h>

#include "filter.h"

class filter_median : public filter
{
private:
	const size_t median_n;
	std::vector<uint8_t *> h_frames;

public:
	filter_median(const size_t median_n);
	virtual ~filter_median();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
