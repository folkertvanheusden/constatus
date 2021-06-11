// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <string>

typedef enum
{
	none,
	upper_left,
	upper_center,
	upper_right,
	center_left,
	center_center,
	center_right,
	lower_left,
	lower_center,
	lower_right,
	xy,
	axy
} pos_type_t;

typedef struct {
	pos_type_t type;
	int x, y;
} pos_t;

pos_t pos_to_pos(const std::string & s_position);
std::tuple<int, int> pos_to_xy(const pos_t & pos, const int win, const int hin, const int tgt_w, const int tgt_h);
