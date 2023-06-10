// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <string>
#include "pos.h"

typedef enum { F_MESSAGE, F_SIMPLE, F_NOTHING } failure_mode_t;

typedef struct {
	failure_mode_t fm;
	std::string bg_bitmap, message;
	pos_t position;
} failure_t;
