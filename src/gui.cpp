// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "utils.h"
#include "log.h"
#include "gen.h"
#include "filter.h"
#include "gui.h"

gui::gui(configuration_t *const cfg, const std::string & id, const std::string & descr, source *const s, const double fps, const int w, const int h, std::vector<filter *> *const filters, const bool handle_failure) : interface(id, descr), cfg(cfg), s(s), fps(fps), w(w), h(h), filters(filters), handle_failure(handle_failure)
{
	local_stop_flag = false;
	ct = CT_GUI;
}

gui::~gui()
{
	free_filters(filters);
}

void gui::operator()()
{
}
