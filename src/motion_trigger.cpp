// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stddef.h>
#include "gen.h"
#include "motion_trigger.h"
#include "log.h"
#include "utils.h"
#include "parameters.h"
#include "target.h"
#include "schedule.h"

motion_trigger::motion_trigger(const std::string & id, const std::string & descr, const std::map<std::string, parameter *> & detection_parameters, std::vector<target *> *const targets, schedule *const sched) : interface(id, descr), parameters(detection_parameters), targets(targets), sched(sched)
{
}

motion_trigger::~motion_trigger()
{
	delete sched;
}

#if HAVE_JANSSON == 1
json_t * motion_trigger::get_rest_parameters()
{
	json_t *result = json_array();

	for(auto & it : parameters) {
		json_t *record = json_object();
		json_object_set_new(record, "key", json_string(it.first.c_str()));
		json_object_set_new(record, "type", json_string(it.second->get_type() == V_INT ? "int" : "double"));

		std::string value = it.second->get_type() == V_INT ? myformat("%d", it.second->get_value_int()) : myformat("%f", it.second->get_value_double());

		json_object_set_new(record, "value", json_string(value.c_str()));

		json_array_append_new(result, record);
	}

	return result;
}

json_t * motion_trigger::rest_set_parameter(const std::string & key, const std::string & value)
{
	json_t *result = json_object();

	if (auto it = parameters.find(key); it != parameters.end()) {
		if (it->second->get_type() == V_INT)
			it->second->set_value(atoi(value.c_str()));
		else if (it->second->get_type() == V_DOUBLE)
			it->second->set_value(atof(value.c_str()));

		json_object_set_new(result, "msg", json_string("OK"));
		json_object_set_new(result, "result", json_true());
	}
	else {
		result = json_object();
		json_object_set_new(result, "msg", json_string("key not found"));
		json_object_set_new(result, "result", json_false());
	}

	return result;
}
#endif

void motion_trigger::register_notifier(interface *const i)
{
	event_clients.push_back(i);
}

void motion_trigger::unregister_notifier(interface *const i)
{
	for(size_t k=0; k<event_clients.size(); k++)  {
		if (event_clients.at(k) == i) {
			event_clients.erase(event_clients.begin() + k);
			return;
		}
	}

	log(id, LL_ERR, "motion_trigger::unregister_notifier: interface not found");
}

void motion_trigger::operator()()
{
}
