// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stddef.h>
#include "gen.h"
#include "view.h"
#include "http_server.h"
#include "utils.h"
#include "controls.h"

view::view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, std::vector<filter *> *const filters, const int jpeg_quality) : source(id, descr, "", width, height, cfg->r, filters, new controls(), jpeg_quality, cfg->text_feeds), cfg(cfg), sources(sources)
{
	ct = CT_VIEW;
}

view::view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, const int jpeg_quality) : source(id, descr, "", width, height, cfg->r, new controls(), jpeg_quality, cfg->text_feeds), cfg(cfg), sources(sources)
{
	ct = CT_VIEW;
}

view::~view()
{
	delete c;
}

void view::operator()()
{
}

source *view::get_current_source()
{
	return this;
}

std::optional<double> view::get_fps()
{
	double tfps = 0.;
	int nfps = 0;

	for(size_t i=0; i<sources.size(); i++) {
		instance *next_inst = nullptr;
		interface *next_source = nullptr;
		find_by_id(cfg, sources.at(i).id, &next_inst, &next_source);

		if (next_source) {
			auto fps = ((source *)next_source)->get_fps();

			if (fps.has_value()) {
				tfps+= fps.value();
				nfps++;
			}
		}
	}

	if (nfps)
		return tfps / nfps;

	return { };
}
