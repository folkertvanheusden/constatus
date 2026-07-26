// (C) 2026 by folkert van heusden, released under the MIT license
#pragma once

#include "filter.h"

class filter_keep_alive : public filter
{
private:
	uint64_t prev_ts { 0 };

public:
	filter_keep_alive();
	virtual ~filter_keep_alive();

	bool uses_in_out() const override { return true; }
	void apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out) override;
};
