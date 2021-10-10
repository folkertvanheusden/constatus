// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <string>
#include <thread>
#include <vector>

#include "interface.h"
#include "cfg.h"

class filter;
class schedule;
class source;
class target;

extern std::string default_fmt;

std::string gen_filename(source *const s, const std::string & fmt, const std::string & store_path, const std::string & prefix, const std::string & ext, const uint64_t ts, const unsigned f_nr);

class target : public interface
{
protected:
	source *const s;
	const std::string store_path, prefix, fmt;
	const int max_time;
	const double interval;
	const std::vector<filter *> *const filters;
	const std::string exec_start, exec_cycle, exec_end;
	const double override_fps;
	configuration_t *const cfg;
	const bool is_view_proxy, handle_failure;
	schedule *const sched;

	std::vector<video_frame *> pre_record;

	unsigned long current_event_nr;

	std::thread *exec_start_th { nullptr };
	std::thread *exec_end_th { nullptr };

	void register_file(const std::string & filename);

public:
	target(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target();

	void start(std::vector<video_frame *> & pre_record, const unsigned long event_nr);

	std::string get_target_dir() const { return store_path; }

	virtual void operator()() = 0;
};
