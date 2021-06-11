// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>

#include "filter.h"

class resize;

class filter_chromakey : public filter
{
protected:
	source *const cks;
	resize *const r;

public:
	filter_chromakey(source *const cks, resize *const r);
	~filter_chromakey();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
