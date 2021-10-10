// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <cstring>
#include <unistd.h>
#include <sys/utsname.h>

#include "target.h"
#include "utils.h"
#include "error.h"
#include "log.h"
#include "source.h"
#include "filter.h"
#include "db.h"
#include "schedule.h"

std::string default_fmt = "%p%Y-%m-%d_%H-%M-%S.%u_%q";

std::string gen_filename(source *const s, const std::string & fmt, const std::string & store_path, const std::string & prefix, const std::string & ext, const uint64_t ts, const unsigned f_nr)
{
	time_t tv_sec = ts / 1000 / 1000;
	uint64_t tv_usec = ts % (1000 * 1000);

	struct tm tm;
	localtime_r(&tv_sec, &tm);

	std::string use_ext = !ext.empty() ? "." + ext : "";

	std::string fname = store_path + "/" + fmt + use_ext;
	fname = search_replace(fname, "%p", prefix);
	fname = search_replace(fname, "%Y", myformat("%04d", tm.tm_year + 1900));
	fname = search_replace(fname, "%m", myformat("%02d", tm.tm_mon + 1));
	fname = search_replace(fname, "%d", myformat("%02d", tm.tm_mday));
	fname = search_replace(fname, "%H", myformat("%02d", tm.tm_hour));
	fname = search_replace(fname, "%M", myformat("%02d", tm.tm_min));
	fname = search_replace(fname, "%S", myformat("%02d", tm.tm_sec));
	fname = search_replace(fname, "%u", myformat("%06ld", tv_usec));
	fname = search_replace(fname, "%q", myformat("%d", f_nr));
	fname = search_replace(fname, "%t", myformat("%lx", s->get_id_hash_val()));
	fname = search_replace(fname, "%$", s->get_id());

	struct utsname un;
	if (uname(&un) == -1)
		error_exit(true, "uname failed");
	fname = search_replace(fname, "%%{host}", un.nodename);

	for(;;) {
		size_t strt = fname.find("%%[");
		if (strt == std::string::npos)
			break;

		size_t e = fname.find("]", strt);
		if (e == std::string::npos)
			break;

		std::string m;
		std::string key = fname.substr(strt + 2, e);

		std::pair<uint64_t, std::string> its;
		if (s -> get_meta() -> get_string(key, &its))
			m = its.second;
		else {
			std::pair<uint64_t, double> itd;
			if (s -> get_meta() -> get_double(key, &itd))
				m = myformat("%f", itd.second);
			else {
				std::pair<uint64_t, int> iti;
				if (s -> get_meta() -> get_int(key, &iti))
					m = myformat("%d", iti.second);
				else {
					log(LL_ERR, "meta-key %s (string) not found", key.c_str());
				}
			}
		}

		std::string a = fname.substr(0, strt);
		std::string b = fname.substr(e + 1);

		fname = a + m + b;
	}

	return fname;
}

target::target(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : interface(id, descr), s(s), cfg(cfg), store_path(store_path), prefix(prefix), fmt(fmt), max_time(max_time), interval(interval), filters(filters), exec_start(exec_start), exec_cycle(exec_cycle), exec_end(exec_end), override_fps(override_fps), is_view_proxy(is_view_proxy), handle_failure(handle_failure), sched(sched)
{
	local_stop_flag = false;
	ct = CT_TARGET;
	current_event_nr = 0;
}

target::~target()
{
	free_filters(filters);

	for(auto f : pre_record)
		delete f;

	delete sched;
}

void target::register_file(const std::string & filename)
{
	log(id, LL_INFO, "Registered new file %s", filename.c_str());
	get_db() -> register_file(id, current_event_nr, filename);
}

void target::start(std::vector<video_frame *> & pre_record, const unsigned long event_nr)
{
	log(id, LL_INFO, "Starting new target thread with event-nr %lu", event_nr);
	this -> pre_record = pre_record;
	this -> current_event_nr = event_nr;

	local_stop_flag = false;

	interface::start();
}
