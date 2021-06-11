// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <stddef.h>
#include "gen.h"
#include "view_html_all.h"
#include "http_server.h"
#include "instance.h"
#include "utils.h"

view_html_all::view_html_all(configuration_t *const cfg, const std::string & id, const std::string & descr, const std::vector<view_src_t> & sources, const int jpeg_quality) : view(cfg, id, descr, -1, -1, sources, jpeg_quality)
{
}

view_html_all::~view_html_all()
{
}

std::string view_html_all::get_html(const std::map<std::string, std::string> & pars) const
{
	std::string reply = "<head>"
				"<link href=\"stylesheet.css\" rel=\"stylesheet\" media=\"screen\">"
				"<link rel=\"shortcut icon\" href=\"favicon.ico\">"
				"<title>" NAME " " VERSION "</title>"
				"</head>"
				"<div class=\"vidmain\">";

	for(instance * inst : cfg -> instances) {
		source *const s = find_source(inst);

		if (!s || s -> get_class_type() == CT_VIEW)
			continue;

		int w = s -> get_width();
		int use_h = s -> get_height() * (320.0 / w);

		reply += myformat("<div class=\"vid\"><p><a href=\"index.html?inst=%s\">%s</p><img src=\"stream.mjpeg?inst=%s\" width=320 height=%d></a></div>", url_escape(inst -> name).c_str(), inst -> name.c_str(), url_escape(inst -> name).c_str(), use_h);
	}

	reply += "</div>";

	return reply;
}

void view_html_all::operator()()
{
}
