// (C) 2017-2023 by folkert van heusden, released under the MIT license
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
	source_filesystem_jpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & path, const double fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio);
	~source_filesystem_jpeg();

	void operator()() override;
};
