// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <string>
#include "pos.h"

typedef enum { F_MESSAGE, F_SIMPLE, F_NOTHING } failure_mode_t;

typedef struct {
	failure_mode_t fm;
	std::string bg_bitmap, message;
	pos_t position;
} failure_t;
