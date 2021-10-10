// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <stdint.h>

#include "filter.h"

class selection_mask;

class filter_motion_only : public filter
{
private:
	selection_mask *const pixel_select_bitmap;
	uint8_t *prev1, *prev2;
	const int noise_level;
	const bool diff_value;

public:
	filter_motion_only(selection_mask *const pixel_select_bitmap, const int noise_level, const bool diff_value);
	virtual ~filter_motion_only();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
