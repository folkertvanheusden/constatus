// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
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
