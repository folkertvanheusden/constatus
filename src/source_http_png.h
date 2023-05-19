// (c) 2017-2022 by folkert van heusden, released under agpl v3.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_http_png : public source
{
private:
	const std::string url, auth;
	const bool ignore_cert;

public:
	source_http_png(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & url, const bool ignore_cert, const std::string & auth, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio);
	~source_http_png();

	void operator()() override;
};
