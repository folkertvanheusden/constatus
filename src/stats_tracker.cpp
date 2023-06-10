// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include <cstring>
#include "stats_tracker.h"
#include "utils.h"
#include "error.h"

stats_tracker::stats_tracker(const std::string & id, const bool is_global) : id(id), is_global(is_global)
{
}

stats_tracker::~stats_tracker()
{
	m.lock();

	cv_stop_notify = true;

	cv_stop.notify_one();

	m.unlock();

	if (th) {
		th->join();

		delete th;
	}
}

void stats_tracker::start()
{
	th = new std::thread(std::ref(*this));
}

void stats_tracker::operator()()
{
	set_thread_name(id);

	for(;;) {
		std::unique_lock<std::mutex> lock(m);

		cv_stop.wait_for(lock, std::chrono::seconds(1));

		// m is locked by 'lock' will go out of scope and thus
		// release m in its destructor
		if (cv_stop_notify)
			break;

		uint64_t now = get_us();
		int slot_base = now / 1000000;
		int slot = slot_base % 5;

		if (latest_ru_ts != 0 && latest_ru_ts != prev_ru_ts) {
			if (prev_ru_ts) {
				if (prev_slot_ru != slot) {
					cpu_stats[slot] = 0;
					prev_slot_ru = slot;
				}

				struct timeval total_time_used { 0, 0 };
				timeradd(&latest_ru.ru_utime, &latest_ru.ru_stime, &total_time_used);

				struct timeval prev_time_used { 0, 0 };
				timeradd(&prev_ru.ru_utime, &prev_ru.ru_stime, &prev_time_used);

				struct timeval diff_time_used { 0, 0 };
				timersub(&total_time_used, &prev_time_used, &diff_time_used);

				double period = (latest_ru_ts - prev_ru_ts) / 1000000.0;

				cpu_stats[slot] += diff_time_used.tv_sec + diff_time_used.tv_usec / 1000000.0 * period;
			}

			prev_ru_ts = latest_ru_ts;
			prev_ru = latest_ru;
		}

		bw_counts[slot] = 0;

		cc_counts[slot] = 0;

		fps_counts[slot] = 0;
	}
}

void stats_tracker::track_cpu_usage()
{
	std::unique_lock<std::mutex> lock(m);

	latest_ru_ts = get_us();

	getrusage(is_global ? RUSAGE_SELF : RUSAGE_THREAD, &latest_ru);
}

double stats_tracker::get_cpu_usage() const
{
	std::unique_lock<std::mutex> lock(m);

	uint64_t now = get_us();
	int slot = (now / 1000000) % 5;

	double total = 0.;

	for(int i=0; i<5; i++) {
		if (i == slot)
			continue;

		total += cpu_stats[i];
	}

	double avg = total / 4;

	return avg;
}

void stats_tracker::reset_cpu_tracking()
{
	{
		std::unique_lock<std::mutex> lock(m);

		prev_slot_ru = -1;

		latest_ru_ts = prev_ru_ts = 0;

		memset(&latest_ru, 0x00, sizeof latest_ru);
		memset(&prev_ru, 0x00, sizeof prev_ru);

		memset(&cpu_stats, 0x00, sizeof cpu_stats);
	}

	track_cpu_usage();
}

void stats_tracker::track_fps()
{
	std::unique_lock<std::mutex> lock(m);

	int cur_counts_slot = (get_us() / 1000000) % 5;
	fps_counts[cur_counts_slot]++;

	fps_set[cur_counts_slot] = true;
}

std::optional<double> stats_tracker::get_fps() const
{
	int cur_counts_slot = (get_us() / 1000000) % 5;

	std::unique_lock<std::mutex> lock(m);

	int total = 0, n = 0;
	for(int i=0; i<5; i++) {
		if (i != cur_counts_slot && fps_set[i]) {
			total += fps_counts[i];
			n++;
		}
	}

	if (n == 0)
		return { };

	return total / double(n);
}

void stats_tracker::track_bw(const int bytes)
{
	int cur_counts_slot = (get_us() / 1000000) % 5;

	std::unique_lock<std::mutex> lock(m);

	bw_counts[cur_counts_slot] += bytes;
}

int stats_tracker::get_bw() const
{
	int cur_counts_slot = (get_us() / 1000000) % 5;

	std::unique_lock<std::mutex> lock(m);

	int total = 0, n = 0;
	for(int i=0; i<5; i++) {
		if (i != cur_counts_slot) {
			total += bw_counts[i];
			n++;
		}
	}

	return total / 4;
}

void stats_tracker::track_cc(const int count)
{
	int cur_counts_slot = (get_us() / 1000000) % 5;

	std::unique_lock<std::mutex> lock(m);

	cc_counts[cur_counts_slot] = count;
}

double stats_tracker::get_cc() const
{
	std::unique_lock<std::mutex> lock(m);

	int total = 0;
	for(int i=0; i<5; i++) 
		total += cc_counts[i];

	return total / 5.0;
}
