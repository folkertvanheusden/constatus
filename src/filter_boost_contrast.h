// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "filter.h"

class filter_boost_contrast : public filter
{
public:
	filter_boost_contrast();
	~filter_boost_contrast();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
