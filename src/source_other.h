// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_other : public source
{
private:
	const std::string o_inst, o_id;
	source *const other;

public:
	source_other(const std::string & id, const std::string & descr, source *const other, const std::string & exec_failure, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, resize *const r, const int resize_w, const int resize_h);
	~source_other();

	void operator()() override;
};
