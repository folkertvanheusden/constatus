// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_plugin.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

source_plugin::source_plugin(const std::string & id, const std::string & descr, const int w, const int h, const std::string & exec_failure, const std::string & plugin_filename, const std::string & plugin_arg, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds) :
	source(id, descr, exec_failure, max_fps, w, h, loglevel, filters, failure, c, jpeg_quality, text_feeds)
{
	library = dlopen(plugin_filename.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening source plugin library %s: %s", plugin_filename.c_str(), dlerror());

	init_plugin = (sp_init_plugin_t)find_symbol(library, "init_plugin", "video source plugin", plugin_filename.c_str());
	uninit_plugin = (sp_uninit_plugin_t)find_symbol(library, "uninit_plugin", "video source plugin", plugin_filename.c_str());

	arg = init_plugin(this, plugin_arg.c_str());
}

source_plugin::~source_plugin()
{
	stop();

	uninit_plugin(arg);

	dlclose(library);

	delete c;
}

void source_plugin::operator()()
{
	log(id, LL_INFO, "source plugins thread started");

	set_thread_name("src_plugins");

	// all work is performed in the plugin itself
	for(;!local_stop_flag;)
		myusleep(101000);

	register_thread_end("source plugin");
}
