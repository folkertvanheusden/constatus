// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "config.h"
#if HAVE_LIBPAM == 1
#include <string>
#include <tuple>

#include "http_auth.h"

class http_auth_pam : public http_auth
{
public:
	http_auth_pam();
	virtual ~http_auth_pam();

	std::tuple<bool, std::string> authenticate(const std::string & username, const std::string & password) override;
};
#endif
