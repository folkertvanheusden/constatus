// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <stdint.h>
#include <string>

#include "gen.h"
#include "pos.h"

class resize
{
public:
	resize();
	virtual ~resize();

	virtual void do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out);
};

void picture_in_picture(resize *const r, uint8_t *const tgt, const int tgt_w, const int tgt_h, const uint8_t *const in, const int win, const int hin, const int perc, const pos_t pos);
void picture_in_picture(uint8_t *const tgt, const int tgt_w, const int tgt_h, const uint8_t *const in, const int win, const int hin, const pos_t pos);
