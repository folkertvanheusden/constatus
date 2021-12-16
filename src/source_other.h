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

public:
	source_other(const std::string & id, const std::string & descr, const std::string & o_inst, const std::string & o_id, resize *const r, const int resize_w, const int resize_h, const std::vector<filter *> *const filters);
	~source_other();

	void operator()() override;
};
