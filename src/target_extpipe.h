// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "target.h"
#include "picio.h"

class target_extpipe : public target
{
private:
	const int quality;
	const std::string cmd;
	transformer_t th;

	void store_frame(video_frame *const put_f, FILE *const p_fd);

public:
	target_extpipe(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const std::string & cmd, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_extpipe();

	void operator()() override;
};
