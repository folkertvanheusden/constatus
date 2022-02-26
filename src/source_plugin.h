// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

typedef void *(* sp_init_plugin_t)(source *const s, const char *const argument);
typedef void (* sp_uninit_plugin_t)(void *arg);

class source_plugin : public source
{
private:
	sp_init_plugin_t   init_plugin;
	sp_uninit_plugin_t uninit_plugin;
	void              *arg;
	void              *library;

public:
	source_plugin(const std::string & id, const std::string & descr, const int w, const int h, const std::string & exec_failure, const std::string & plugin_filename, const std::string & plugin_arg, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality);
	~source_plugin();

	void operator()() override;
};
