// (C) 2017-2023 by folkert van heusden, released under the MIT license
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
