// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <string>
#include <tuple>

class http_auth {
private:
	const std::string auth_file;

protected:
	http_auth() { }

public:
	http_auth(const std::string & auth_file);
	virtual ~http_auth();

	virtual std::tuple<bool, std::string> authenticate(const std::string & username, const std::string & password);
};
