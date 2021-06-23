// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <assert.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "interface.h"
#include "utils.h"
#include "error.h"
#include "db.h"
#include "exec.h"
#include "log.h"

meta * interface::get_meta()
{
	static meta m;

	return &m;
}

std::string interface::db_url, interface::db_user, interface::db_pass;
std::mutex interface::dbi_lock;

void interface::register_database(const std::string & url, const std::string & user, const std::string & pass)
{
	db_url = url;
	db_user = user;
	db_pass = pass;
}

db * interface::get_db()
{
	const std::lock_guard<std::mutex> lck(dbi_lock);

	if (!dbi)
		dbi = new db(db_url, db_user, db_pass);

	return dbi;
}

interface::interface(const std::string & id, const std::string & descr) : paused(false), th(nullptr), local_stop_flag(false), ct(CT_NONE), id(id), descr(descr)
{
	st = new stats_tracker("st:" + id, false);

	st->start();
}

interface::~interface()
{
	if (dbi)
		delete dbi;

	delete st;
}

unsigned long interface::get_id_hash_val() const
{
	return hash(id);
}

std::string interface::get_id_hash_str() const
{
	return myformat("%lx", get_id_hash_val());
}

void interface::pauseCheck()
{
	pause_lock.lock();
	pause_lock.unlock();
}

bool interface::pause()
{
	if (paused)
		return false;

	if (!pause_lock.try_lock_for(std::chrono::milliseconds(100)))
		return false;

	paused = true;

	return true;
}

void interface::unpause()
{
	if (paused) {
		paused = false;
		pause_lock.unlock();
	}
}

void interface::start()
{
	const std::lock_guard<std::mutex> lock(th_lock);

	log(id, LL_DEBUG, "interface::start: user count was %d", user_count);

	if (user_count == 0) {
		log(id, LL_DEBUG, "Initial thread start");

		local_stop_flag = false;

		clear_error();

		// on_demand set or first start
		if (!th) {
			th = new std::thread(std::ref(*this));
			running = true;
		}
		else {
			log(id, LL_DEBUG, "Thread for \"%s\" was already started?", id.c_str());
		}
	}

	user_count++;
}

void interface::restart()
{
	const std::lock_guard<std::mutex> lock(th_lock);

	if (!th) {
		log(id, LL_DEBUG, "interface::restart: no thread running (%d users)", user_count);
		assert(user_count == 0);
		return;
	}

	// stop

	int old_user_count = user_count;
	user_count = 0;

	running = false;
	local_stop_flag = true;
	join_thread(&th, id, descr);

	st->reset_cpu_tracking();

	// (re-)start

	local_stop_flag = false;

	clear_error();

	th = new std::thread(std::ref(*this));
	running = true;

	user_count = old_user_count;
}

void interface::stop()
{
	const std::lock_guard<std::mutex> lock(th_lock);

	log(id, LL_DEBUG, "interface::stop: user count is now %d", user_count);

	if (th) {
		user_count--;

		assert(user_count >= 0);

		if (user_count == 0 && on_demand) {
			log(id, LL_DEBUG, "User count is 0; terminating thread");

			running = false;

			local_stop_flag = true;

			join_thread(&th, id, descr);

			st->reset_cpu_tracking();
		}
	}
	else {
		log(id, LL_INFO, "interface::stop: \"%s\" (\"%s\") was already stopped!", descr.c_str(), id.c_str());
	}
}

void interface::notify_thread_of_event(const std::string & subject)
{
	const std::lock_guard<std::mutex> lck(m_event);

	cv_event.notify_all();

	// it is problematic to let the motion_trigger_generic check
	// for motion and at the same time wait for events. so for
	// that use-case, set an atomic
	cv_event_notified = true;

	last_notification_subject = subject;
}

void interface::join_thread(std::thread **const handle, const std::string & id, const std::string & descr)
{
	if (*handle) {
		log(id, LL_INFO, "Waiting for thread \"%s\" to stop...", descr.c_str());

		(*handle)->join();

		delete *handle;
		*handle = nullptr;

		log(id, LL_INFO, "Thread \"%s\" stopped", descr.c_str());
	}
	else {
		log(id, LL_WARNING, "Thread \"%s\" (for \"%s\") was not running? (during join_thread)", id.c_str(), descr.c_str());
	}
}

bool interface::is_running() const
{
	const std::lock_guard<std::mutex> lock(th_lock);

	return th != nullptr;
}

std::optional<error_state_t> interface::get_last_error()
{
	const std::shared_lock<std::shared_mutex> lock(error_lock);

	return error;
}

void interface::clear_error()
{
	const std::lock_guard<std::shared_mutex> lock(error_lock);

	error.reset();
}

void interface::set_error(const std::string & err, const bool is_critical)
{
	log(id, LL_ERR, err.c_str());

	const std::lock_guard<std::shared_mutex> lock(error_lock);

	error = { err, is_critical };
}

void interface::register_thread_end(const std::string & which)
{
	log(id, LL_INFO, "Thread for \"%s\" terminating", which.c_str());

	const std::lock_guard<std::shared_mutex> lock(error_lock);

	if (!local_stop_flag && error.has_value() == false)
		error = { "Thread terminated unexpectedly", false };
}

void interface::set_on_demand(const bool v)
{
	on_demand = v;
}

// only used when terminating application
// in that case things should really stop and thus on_demand is
// set to true (on_demand really stops the interface)
void interface::announce_stop()
{
	on_demand = true;
	local_stop_flag = true;
}
