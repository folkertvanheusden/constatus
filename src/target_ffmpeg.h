// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "config.h"
#if HAVE_FFMPEG == 1
#include "target.h"

class filter;
class source;

class target_ffmpeg : public target
{
private:
	const std::string parameters, type;
	unsigned bitrate;

public:
	target_ffmpeg(const std::string & id, const std::string & descr, const std::string & parameters, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int max_time, const double interval, const std::string & type, const int bitrate, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_ffmpeg();

	void operator()() override;
};
#endif
