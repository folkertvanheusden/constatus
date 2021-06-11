// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <stdint.h>
#include <string>

class resize;

class selection_mask
{
private:
	resize *const r;

	uint8_t *pixels;
	int w, h;

	uint8_t *cache;
	int cw, ch;

public:
	selection_mask(resize *const r, const std::string & file);
	virtual ~selection_mask();

	uint8_t *get_mask(const int rw, const int rh);
};
