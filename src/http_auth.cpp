// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <fstream>
#include <iostream>

#include "http_auth.h"
#include "log.h"

http_auth::http_auth(const std::string & auth_file) : auth_file(auth_file)
{
}

http_auth::~http_auth()
{
}

std::tuple<bool, std::string> http_auth::authenticate(const std::string & username, const std::string & password)
{
	log(LL_INFO, "Authenticate user %s", username.c_str());

	std::ifstream fh(auth_file);
	if (!fh.is_open())
		return std::make_tuple(false, "Cannot open auth file");

	std::string f_username, f_password;
	if (!getline(fh, f_username)) {
		fh.close();
		return std::make_tuple(false, "Username missing in auth file");
	}

	if (!getline(fh, f_password)) {
		fh.close();
		return std::make_tuple(false, "Password missing in auth file");
	}

	fh.close();

	if (username != f_username)
		return std::make_tuple(false, "Username/password mismatch");

	if (password != f_password)
		return std::make_tuple(false, "Username/password mismatch");

	return std::make_tuple(true, "Ok");
}
