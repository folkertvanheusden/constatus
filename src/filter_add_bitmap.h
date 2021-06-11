// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <stdint.h>
#include <string>

#include "filter.h"

class interface;

class filter_add_bitmap : public filter
{
private:
	const std::string which;
	const int x, y;

public:
	filter_add_bitmap(const std::string & which, const int x, const int y);
	~filter_add_bitmap();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
