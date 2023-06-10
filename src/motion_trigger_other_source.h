// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <string>

#include "cfg.h"
#include "motion_trigger.h"

class filter;
class source;
class target;

class motion_trigger_other_source : public motion_trigger
{
private:
	source *const hr_s;
	instance *const lr_motion_trigger;

	std::atomic_bool motion_triggered { false };

public:
	motion_trigger_other_source(const std::string & id, const std::string & descr, source *const hr_s, instance *const lr_motion_trigger, std::vector<target *> *const targets, const std::map<std::string, parameter *> & detection_parameters, schedule *const sched);
	virtual ~motion_trigger_other_source();

	bool check_motion() override { return motion_triggered; }

	void operator()() override;
};
