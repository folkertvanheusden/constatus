// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <atomic>
#include <thread>

#include "interface.h"

class db;

class cleaner : public interface
{
private:
	std::thread *th;
	std::atomic_bool local_stop_flag;
	db *dbi;
	const int check_interval, purge_interval, cookies_max_age;

public:
	cleaner(const std::string & db_url, const std::string & db_user, const std::string & dp_pass, const int check_interval, const int purge_interval, const int cookies_max_age);
	virtual ~cleaner();

	void start() override;

	void operator()() override;
};
