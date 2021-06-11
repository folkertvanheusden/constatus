// (c) 2017-2020 by folkert van heusden, released under agpl v3.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_http_jpeg : public source
{
private:
	const std::string url, auth;
	const bool ignore_cert;

public:
	source_http_jpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & url, const bool ignore_cert, const std::string & auth, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality);
	~source_http_jpeg();

	void operator()() override;
};
