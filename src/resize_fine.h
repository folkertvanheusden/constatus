// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <stdint.h>

#include "resize.h"

class resize_fine : public resize
{
public:
	resize_fine();
	virtual ~resize_fine();

	void do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out) override;
};
