// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once
#include <condition_variable>
#include <string>
#include <thread>

#include "source.h"

class source_http_mjpeg : public source
{
private:
	const std::string url;
	const bool ignore_cert;

public:
	source_http_mjpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & url, const bool ignore_cert, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds);
	virtual ~source_http_mjpeg();

	void operator()() override;
};
