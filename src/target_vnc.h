// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "target.h"

class target_vnc : public target
{
private:
	int fd;

public:
	target_vnc(const std::string & id, const std::string & descr, source *const s, const listen_adapter_t & la, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_end, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_vnc();

	void operator()() override;
};
