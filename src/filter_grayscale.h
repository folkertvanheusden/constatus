// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "filter.h"

typedef enum { G_FAST, G_CIE_1931, G_PAL_NTSC, G_LIGHTNESS } to_grayscale_t;

class filter_grayscale : public filter
{
private:
	const to_grayscale_t mode;

public:
	filter_grayscale(const to_grayscale_t mode);
	~filter_grayscale();

	bool uses_in_out() const override { return true; }
	void apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out) override;
};
