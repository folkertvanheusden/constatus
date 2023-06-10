// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <sys/resource.h>
#include <sys/time.h>

#include "meta.h"
#include "stats_tracker.h"

class db;

// C++ has no instanceOf
typedef enum { CT_HTTPSERVER, CT_MOTIONTRIGGER, CT_TARGET, CT_SOURCE, CT_LOOPBACK, CT_VIEW, CT_FILTER, CT_GUI, CT_DISCOVERABLE, CT_NONE } classtype_t;

class interface
{
protected:
	std::timed_mutex pause_lock;
	std::atomic_bool paused { false };

	mutable std::mutex th_lock;
	std::thread *th { nullptr };
	int user_count { 0 };
	bool on_demand { false };
	std::atomic_bool running { false };

	std::atomic_bool local_stop_flag { false } ;

	std::condition_variable cv_event;
	std::atomic_bool cv_event_notified { false };
	std::mutex m_event;
	std::string last_notification_subject;

	classtype_t ct { CT_NONE };
	std::string id, descr;

	std::shared_mutex error_lock;
	std::optional<error_state_t> error;

	void pauseCheck();

	static std::string db_url, db_user, db_pass;
	static std::mutex dbi_lock;
	db *dbi { nullptr };

	stats_tracker *st { nullptr };

	void join_thread(std::thread **const handle, const std::string & id, const std::string & descr);
	void register_thread_end(const std::string & which);
	void clear_error();
	void set_error(const std::string & err, const bool is_critical);

public:
	interface(const std::string & id, const std::string & descr);
	virtual ~interface();

	static meta * get_meta();

	static void register_database(const std::string & url, const std::string & user, const std::string & pass);
	db * get_db();

	std::string get_id() const { return id; }
	std::string get_id_hash_str() const;
	unsigned long get_id_hash_val() const;

	std::string get_descr() const { return descr; }
	classtype_t get_class_type() const { return ct; }

	virtual void notify_thread_of_event(const std::string & subject);

	virtual double get_cpu_usage() const { return st->get_cpu_usage(); }
	std::optional<double> get_fps() const { return st->get_fps(); }
	virtual int get_bw() const { return st->get_bw(); }

	virtual void start();
	bool is_running() const;
	bool work_required() const { return running; }
	bool pause();
	bool is_paused() const { return paused; }
	void unpause();
	void announce_stop();
	virtual void restart();
	virtual void stop();
	std::optional<error_state_t> get_last_error();
	void set_on_demand(const bool v);
	bool get_on_demand() const { return on_demand; };

	virtual void operator()() { }
};
