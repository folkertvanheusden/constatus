// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0

#include <vector>

#include "http_cookies.h"
#include "utils.h"

http_cookies::http_cookies()
{
}

http_cookies::~http_cookies()
{
}

void http_cookies::clean_cookies(const int max_age)
{
	const std::lock_guard<std::mutex> lck(lock);

	std::vector<std::string> to_erase;
	time_t now = time(nullptr);

	for(auto it : cookies) {
		if (it.second.second + max_age < now)
			to_erase.push_back(it.first);
	}

	for(auto cookie : to_erase)
		cookies.erase(cookies.find(cookie));
}

std::string http_cookies::get_cookie(const std::string & user)
{
	std::string rc;

	const std::lock_guard<std::mutex> lck(lock);

	for(auto it : cookies) {
		if (it.second.first == user) {
			rc = it.first;
			break;
		}
	}

	if (rc.empty()) {
		// gen & insert
		constexpr int rnd_len = 16;
		uint8_t *rnd = gen_random(rnd_len);
		std::string rnd_str = bin_to_hex(rnd, rnd_len);
		free(rnd);

		cookies.insert({ rnd_str, { user, time(nullptr) } });

		rc = rnd_str;
	}

	return rc;
}

std::string http_cookies::get_cookie_user(const std::string & cookie_key)
{
	std::string rc;

	const std::lock_guard<std::mutex> lck(lock);

	auto it = cookies.find(cookie_key);
	if (it != cookies.end())
		rc = it->second.first;

	return rc;
}

void http_cookies::update_cookie(const std::string & user, const std::string & key)
{
	const std::lock_guard<std::mutex> lck(lock);

	auto it = cookies.find(key);
	if (it != cookies.end())
		it->second.second = time(nullptr);
}

void http_cookies::delete_cookie_for_user(const std::string & user)
{
	const std::lock_guard<std::mutex> lck(lock);

	std::vector<std::string> to_erase;
	time_t now = time(nullptr);

	for(auto it : cookies) {
		if (it.first == user) {
			cookies.erase(cookies.find(it.first));
			break;
		}
	}
}
