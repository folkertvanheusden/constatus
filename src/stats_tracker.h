// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <sys/resource.h>
#include <sys/time.h>

class stats_tracker
{
private:
	const std::string id;
	const bool is_global;

	int prev_slot_ru { -1 };
	uint64_t prev_ru_ts { 0 }, latest_ru_ts { 0 };
	struct rusage latest_ru { 0 }, prev_ru { 0 };
	double cpu_stats[5] { 0. };

	int fps_counts[5]{ 0 }, last_fps_count_sec{ 0 };
	bool fps_set[5] { false };

	int bw_counts[5]{ 0 }, last_bw_count_sec{ 0 };
	int cc_counts[5]{ 0 }, last_cc_count_sec{ 0 };

	std::thread *th { nullptr };

	std::condition_variable cv_stop;
	mutable std::mutex m;
	bool cv_stop_notify { false };

public:
	stats_tracker(const std::string & id, const bool is_global);
	virtual ~stats_tracker();

	void start();
	void operator()();

	void track_cpu_usage();
	double get_cpu_usage() const;
	void reset_cpu_tracking();

	void track_fps();
	virtual std::optional<double> get_fps() const;

	void track_bw(const int bytes);
	virtual int get_bw() const;

	void track_cc(const int count);
	virtual double get_cc() const;
};
