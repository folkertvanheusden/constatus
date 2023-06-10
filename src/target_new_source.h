// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "target.h"

class target_new_source : public target
{
private:
	const std::string                   new_s_id;
	const std::string                   new_s_descr;
	const bool                          rot90;
	const std::map<std::string, feed *> text_feeds;

	std::mutex                          new_source_lock;
        source                             *new_source { nullptr };

public:
	target_new_source(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const std::string & new_s_id, const std::string & new_s_descr, schedule *const sched, const bool rot90, const std::map<std::string, feed *> & text_feeds);
	virtual ~target_new_source();

	source * get_new_source();

	void operator()() override;
};
