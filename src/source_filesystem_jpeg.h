// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_filesystem_jpeg : public source
{
private:
	const std::string path;

public:
	source_filesystem_jpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & path, const double fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds);
	~source_filesystem_jpeg();

	void operator()() override;
};
