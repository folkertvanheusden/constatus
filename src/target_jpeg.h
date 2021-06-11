// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "target.h"

class target_jpeg : public target
{
private:
	const int quality;

	int f_nr { 0 };
	bool is_start { true };

	std::string write_frame(video_frame * put_f);

public:
	target_jpeg(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_jpeg();

	void operator()() override;
};
