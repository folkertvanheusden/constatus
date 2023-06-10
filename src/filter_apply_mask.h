// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "filter.h"

class selection_mask;

class filter_apply_mask : public filter
{
private:
	selection_mask *const psb;
	const bool soft_mask;

public:
	filter_apply_mask(selection_mask *const pixel_select_bitmap, const bool soft_mask);
	~filter_apply_mask();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
