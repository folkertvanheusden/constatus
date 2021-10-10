// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include <stdlib.h>
#include <string>
#include <tuple>
#include <vector>
#include "pos.h"
#include "utils.h"
#include "error.h"

pos_t pos_to_pos(const std::string & s_position)
{
	pos_t pos{ center_center, 0, 0 };

	if (s_position == "upper-left")
		pos.type = upper_left;
	else if (s_position == "upper-center")
		pos.type = upper_center;
	else if (s_position == "upper-right")
		pos.type = upper_right;
	else if (s_position == "center-left")
		pos.type = center_left;
	else if (s_position == "center-center")
		pos.type = center_center;
	else if (s_position == "center-right")
		pos.type = center_right;
	else if (s_position == "lower-left")
		pos.type = lower_left;
	else if (s_position == "lower-center")
		pos.type = lower_center;
	else if (s_position == "lower-right")
		pos.type = lower_right;
	else if (s_position.substr(0, 3) == "xy:") {
		pos.type = xy;
		std::vector<std::string> *parts = split(s_position.substr(3), ",");
		if (parts->size() != 2)
			error_exit(false, "Position \"xy\" must be in format \"xy:...x...,...y...\"");

		pos.x = atoi(parts->at(0).c_str());
		pos.y = atoi(parts->at(1).c_str());
		delete parts;
	}
	else if (s_position.substr(0, 4) == "axy:") {
		pos.type = axy;
		std::vector<std::string> *parts = split(s_position.substr(4), ",");
		if (parts->size() != 2)
			error_exit(false, "Position \"axy\" must be in format \"xy:...x...,...y...\"");

		pos.x = atoi(parts->at(0).c_str());
		pos.y = atoi(parts->at(1).c_str());
		delete parts;
	}
	else if (s_position == "none")
		pos.type = none;
	else
		error_exit(false, "(text-)position %s is not understood", s_position.c_str());

	return pos;
}

std::tuple<int, int> pos_to_xy(const pos_t & pos, const int win, const int hin, const int tgt_w, const int tgt_h)
{
	int hwin = win / 2;
	int hhin = hin / 2;

	const int qw = tgt_w / 4, qh = tgt_h / 4;
	int tx = 0, ty = 0;

	if (pos.type == upper_left) {
		tx = qw * 1 - hwin * 3 / 2;
		ty = qh - hhin * 3 / 2;
	}
	else if (pos.type == upper_center) {
		tx = qw * 2 - hwin;
		ty = qh - hhin * 3 / 2;
	}
	else if (pos.type == upper_right) {
		tx = qw * 3 - hwin * 2 / 3;
		ty = qh - hhin * 3 / 2;
	}
	else if (pos.type == center_left) {
		tx = qw * 1 - hwin * 3 / 2;
		ty = qh * 2 - hhin;
	}
	else if (pos.type == center_center) {
		tx = qw * 2 - hwin;
		ty = qh * 2 - hhin;
	}
	else if (pos.type == center_right) {
		tx = qw * 3 - hwin * 2 / 3;
		ty = qh * 2 - hhin;
	}
	else if (pos.type == lower_left) {
		tx = qw * 1 - hwin * 3 / 2;
		ty = qh * 3 - hhin * 2 / 3;
	}
	else if (pos.type == lower_center) {
		tx = qw * 2 - hwin;
		ty = qh * 3 - hhin * 2 / 3;
	}
	else if (pos.type == lower_right) {
		tx = qw * 3 - hwin * 2 / 3;
		ty = qh * 3 - hhin * 2 / 3;
	}
	else if (pos.type == xy || pos.type == axy) {
		tx = pos.x;
		ty = pos.y;

		if (pos.type == xy) {
			while(tx < 0)
				tx += tgt_w;
			while(ty < 0)
				ty += tgt_h;

			while(tx >= tgt_w)
				tx -= tgt_w;
			while(ty >= tgt_h)
				ty -= tgt_h;
		}
	}

	return std::make_tuple(tx, ty);
}
