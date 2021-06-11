// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>

#include "filter.h"

class filter_overlay : public filter
{
	const pos_t pos;
	int w, h;
	uint8_t *pixels;

public:
	filter_overlay(const std::string & file, const pos_t & pos);
	~filter_overlay();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
