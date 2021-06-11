// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <assert.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <vector>

#include "cleaner.h"
#include "utils.h"
#include "error.h"
#include "log.h"
#include "db.h"
#include "exec.h"

cleaner::cleaner(const std::string & db_url, const std::string & db_user, const std::string & db_pass, const int check_interval, const int purge_interval, const int cookies_max_age) : interface("cleaner", "maintenance"), th(nullptr), local_stop_flag(false), check_interval(check_interval), purge_interval(purge_interval), cookies_max_age(cookies_max_age)
{
	dbi = new db(db_url, db_user, db_pass);
}

cleaner::~cleaner()
{
	delete dbi;
}

void cleaner::start()
{
	assert(th == nullptr);

	local_stop_flag = false;

	if (check_interval > 0 && purge_interval > 0)
		th = new std::thread(std::ref(*this));
}

void cleaner::operator()()
{
	log(LL_INFO, "Cleaner thread started");

	set_thread_name("cleaner");

	for(;!local_stop_flag;) {
		log(LL_INFO, "cleaning (check interval: %d, max. file age: %d days)", check_interval, purge_interval);
		std::vector<std::string> files = dbi->purge(purge_interval);

		for(auto file : files) {
			log(LL_INFO, "delete file %s", file.c_str());

			if (unlink(file.c_str()) == -1)
				log(LL_ERR, "Cannot delete %s: %s", file.c_str(), strerror(errno));
		}

		mysleep(check_interval * 1000000ll, &local_stop_flag, NULL);
	}

	log(LL_INFO, "Cleaner thread terminating");
}
