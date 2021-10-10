// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "target.h"

class target_new_source : public target
{
private:
	const std::string new_s_id, new_s_descr;

	std::mutex new_source_lock;
        source *new_source{ nullptr };

public:
	target_new_source(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const std::string & new_s_id, const std::string & new_s_descr, schedule *const sched);
	virtual ~target_new_source();

	source * get_new_source();

	void operator()() override;
};
