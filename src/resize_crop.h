// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <stdint.h>
#include <string>

#include "resize.h"

class resize_crop : public resize
{
private:
	const bool resize_crop_center, fill_max;

public:
	resize_crop(const bool resize_crop_center, const bool fill_max);
	virtual ~resize_crop();

	void do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out) override;
};
