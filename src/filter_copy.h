// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <stdint.h>

#include "filter.h"

class filter_copy : public filter
{
private:
	const std::string remember_as;
	const int x, y, w, h;

public:
	filter_copy(const std::string & remember_as, const int x, const int y, const int w, const int h);
	virtual ~filter_copy();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
