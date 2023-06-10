// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <map>
#include <mutex>
#include <string>
#include <time.h>

class http_cookies
{
private:
	std::mutex lock;
	std::map<std::string, std::pair<std::string, time_t> > cookies;

public:
	http_cookies();
	virtual ~http_cookies();

	void clean_cookies(const int max_age);
	std::string get_cookie(const std::string & user);
	std::string get_cookie_user(const std::string & cookie_key);
	void update_cookie(const std::string & user, const std::string & key);
	void delete_cookie_for_user(const std::string & user);
};
