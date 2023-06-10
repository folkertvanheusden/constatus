// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "schedule.h"
#include <string>
#include <time.h>
#include <vector>

#include "schedule.h"
#include "error.h"
#include "utils.h"

uint32_t time_to_val(const std::string & str)
{
	std::vector<std::string> *parts = split(str, ":");

	uint32_t out = 0;

	out += atoi(parts->at(0).c_str()) * 3600;

	if (parts->size() >= 2)
		out += atoi(parts->at(1).c_str()) * 60;

	if (parts->size() == 3)
		out += atoi(parts->at(2).c_str());

	delete parts;

	return out;
}

schedule::schedule(const std::vector<std::string> & sched)
{
//	schedule = ( "wed,14:10,15:00,on", "wed,15:10,16:00,on" )
	for(auto str : sched) {
		part_of_day pod { 0 };

		std::vector<std::string> *parts = split(str, ",");

		if (parts->size() != 4)
			error_exit(false, "schedule-line \"%s\" is incorrect", str.c_str());

		if (parts->at(0) == "sun")
			pod.day_of_week = 0;
		else if (parts->at(0) == "mon")
			pod.day_of_week = 1;
		else if (parts->at(0) == "tue")
			pod.day_of_week = 2;
		else if (parts->at(0) == "wed")
			pod.day_of_week = 3;
		else if (parts->at(0) == "thu")
			pod.day_of_week = 4;
		else if (parts->at(0) == "fri")
			pod.day_of_week = 5;
		else if (parts->at(0) == "sat")
			pod.day_of_week = 6;
		else
			error_exit(false, "day in \"%s\" not recognized", str.c_str());

		pod.start = time_to_val(parts->at(1));
		pod.end = time_to_val(parts->at(2));
		pod.state = parts->at(3) == "on";

		delete parts;

		s.push_back(pod);
	}
}

schedule::~schedule()
{
}

bool schedule::is_on()
{
	time_t t = time(nullptr);
	struct tm tm { 0 };
	localtime_r(&t, &tm);

	uint32_t cur_day_ts = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

	bool state = false;

	for(auto pod : s) {
		if (pod.day_of_week == tm.tm_wday && cur_day_ts >= pod.start && cur_day_ts < pod.end)
			state = pod.state;
	}

	return state;
}
