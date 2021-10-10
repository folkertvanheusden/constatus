// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <dlfcn.h>
#include <cstring>

#include "gen.h"
#include "error.h"
#include "filter_plugin.h"

void *find_symbol(void *const dl, const std::string & filename, const std::string & function, const bool is_fatal)
{
	void *sym = dlsym(dl, function.c_str());
	if (!sym && is_fatal)
		error_exit(true, "Failed finding filter plugin \"%s\" in %s", function.c_str(), filename.c_str());

	return sym;
}

void *load_library(const std::string & filename)
{
	void *library = dlopen(filename.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening filter plugin library %s", filename.c_str());

	return library;
}

filter_plugin::filter_plugin(const std::string & filter_filename, const std::string & parameter)
{
	library = load_library(filter_filename.c_str());

	init_filter = (init_filter_t)find_symbol(library, filter_filename, "init_filter");

	apply_filter = (apply_filter_t)find_symbol(library, filter_filename, "apply_filter");

	uninit_filter = (uninit_filter_t)find_symbol(library, filter_filename, "uninit_filter");

	arg = init_filter(parameter.c_str());
}

filter_plugin::~filter_plugin()
{
	if (arg)
		uninit_filter(arg);

	dlclose(library);
}

void filter_plugin::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	apply_filter(arg, ts, w, h, prev, in, out);
}
