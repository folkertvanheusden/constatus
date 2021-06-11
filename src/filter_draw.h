// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>

#include "filter.h"

class filter_draw : public filter
{
protected:
	const int x, y, w, h;
	const rgb_t col;

public:
	filter_draw(const int x, const int y, const int w, const int h, const rgb_t col);
	~filter_draw();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
