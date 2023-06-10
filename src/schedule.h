// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <stdint.h>
#include <string>
#include <vector>

typedef struct
{
	uint8_t day_of_week;
	uint32_t start, end; // in seconds
	bool state;
} part_of_day;

class schedule
{
private:
	std::vector<part_of_day> s;

public:
	schedule(const std::vector<std::string> & sched);
	~schedule();

	bool is_on();
};
