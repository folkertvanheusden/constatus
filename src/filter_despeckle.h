// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "filter.h"

class filter_despeckle : public filter
{
private:
	const std::string pattern;

public:
	filter_despeckle(const std::string & pattern);
	~filter_despeckle();

	bool uses_in_out() const override { return true; }
	void apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out) override;
};
