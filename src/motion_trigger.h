// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#include <atomic>
#if HAVE_JANSSON == 1
#include <jansson.h>
#endif
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

#include "interface.h"

class filter;
class parameter;
class schedule;
class selection_mask;
class source;
class target;

class motion_trigger : public interface
{
protected:
	std::map<std::string, parameter *> parameters;

	std::vector<interface *> event_clients;

	std::vector<target *> *const targets;

	schedule *const sched;

public:
	motion_trigger(const std::string & id, const std::string & descr, const std::map<std::string, parameter *> & detection_parameters, std::vector<target *> *const targets, schedule *const sched);
	virtual ~motion_trigger();

	virtual bool check_motion() { return false; }

#if HAVE_JANSSON == 1
	json_t * get_rest_parameters();
	json_t * rest_set_parameter(const std::string & key, const std::string & value);
#endif

	void register_notifier(interface *const i);
	void unregister_notifier(interface *const i);

	virtual void operator()() override;
};
