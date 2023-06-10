// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <string>

#include "gen.h"
#include "utils.h"
#if HAVE_GSTREAMER == 1
#include "target_avi.h"
#endif
#if HAVE_FFMPEG == 1
#include "target_ffmpeg.h"
#else
#include "target.h"
#endif
#include "http_server.h"
#include "log.h"
#include "source.h"
#include "view.h"
#include "instance.h"

std::string emit_stats_refresh_js(const std::string & id, const bool fps, const bool cpu, const bool bw, const bool cc)
{
	std::string out = "<script>\n";

	out += "setInterval(function(){\n";

	out += "action = async () => {\n";
	out += "const response = await fetch('stats?module=" + id + "');\n";
	out += "if (response.ok) {\n";
	out += "const j = await response.json();\n";
	if (fps)
		out += "document.getElementById('fps-" + id + "').innerHTML = j['fps'];\n";
	if (cpu)
		out += "document.getElementById('cpu-" + id + "').innerHTML = j['cpu-usage'].toFixed(2) + '%';\n";
	if (bw)
		out += "document.getElementById('bw-" + id + "').innerHTML = j['bw'];\n";
	if (cc)
		out += "document.getElementById('cc-" + id + "').innerHTML = j['cc'];\n";
	out += "}\n";
	out += "}\n";
	out += "action();\n";
	out += "}, 1000);\n";

	out += "</script>";

	return out;
}

std::string describe_interface(const instance *const inst, const interface *const i, const bool short_)
{
	std::string inst_url = url_escape(inst -> name);

	std::string module_int = i->get_id();

	std::string type = "!!!";
	switch(i -> get_class_type()) {
		case CT_HTTPSERVER:
			type = "HTTP server";
			break;
		case CT_MOTIONTRIGGER:
			type = "motion trigger";
			break;
		case CT_TARGET:
			type = "stream writer";
			break;
		case CT_SOURCE:
			type = "source";
			break;
		case CT_LOOPBACK:
			type = "video4linux loopback";
			break;
		case CT_VIEW:
			type = "view";
			break;
		case CT_FILTER:
			type = "filter";
			break;
		case CT_GUI:
			type = "gui";
			break;
		case CT_DISCOVERABLE:
			type = "announcer";
			break;
		case CT_NONE:
			type = "???";
			break;
	}

	std::string out = (short_ ? "<h3>" + i->get_id() : (std::string("<section><h3>type: ") + type));
	out += "<div class=\"controls\">\n";
	out += myformat("<a href=\"restart?inst=%s&module=%s\"><img src=\"images/play.svg\"></a>\n", inst_url.c_str(), module_int.c_str());
	out += myformat("<a href=\"toggle-pause?inst=%s&module=%s\"><img src=\"images/pause.svg\"></a>\n", inst_url.c_str(), module_int.c_str());
	out += myformat("<a href=\"toggle-start-stop?inst=%s&module=%s\"><img src=\"images/stop.svg\"></a>\n", inst_url.c_str(), module_int.c_str());
	out += "</div>\n";
	out += "</h3>\n";

	out += "<dl>";
	if (!short_) {
		out += "<dt>description</dt><dd><strong>" + i -> get_descr() + "</strong></dd>";
		out += "<dt>type</dt><dd><strong>" + type + "</strong></dd>";
	}
	out += "<dt>CPU usage</dt><dd><strong id=\"cpu-" + module_int +"\">" + myformat("%.2f%%", i->get_cpu_usage() * 100.0) + "</strong></dd>";
	auto fps = i->get_fps();
	out += "<dt>FPS</dt><dd><strong id=\"fps-" + module_int + "\">" + myformat("%.2f", fps.has_value() ? fps.value() : 0) + "</strong></dd>";
	bool is_httpd = i->get_class_type() == CT_HTTPSERVER;
	if (is_httpd) {
		out += "<dt>bandwidth</dt><dd><strong id=\"bw-" + module_int + "\">" + myformat("%d", i->get_bw() / 1024) + "</strong><strong>kB/s</strong></dd>";
		out += "<dt>connection count</dt><dd><strong id=\"cc-" + module_int + "\">" + myformat("%ld", ((http_server *)i)->get_connection_count()) + "</strong></dd>";
	}
	out += "</dl>";

	out += emit_stats_refresh_js(module_int, true, true, is_httpd, is_httpd);

	if (!short_)
		out += "</section>";

	return out;
}

bool pause(instance *const cfg, const std::string & which, const bool p)
{
	bool rc = false;

	interface *i = find_interface_by_id(cfg, which);
	if (i != NULL) {
		if (p)
			rc = i -> pause();
		else {
			i -> unpause();
			rc = true;
		}
	}

	return rc;
}

bool toggle_pause(instance *const cfg, const std::string & which)
{
	bool rc = false;

	interface *i = find_interface_by_id(cfg, which);
	if (i != NULL) {
		if (!i->is_paused())
			rc = i -> pause();
		else {
			i -> unpause();
			rc = true;
		}
	}

	return rc;
}

bool start_stop(interface *const i, const bool strt)
{
	if (i == nullptr)
		return false;

	if (strt)
		i -> start();
	else
		i -> stop();

	return true;
}

bool toggle_start_stop(interface *const i)
{
	if (i == nullptr)
		return false;

	if (i->is_running())
		i -> stop();
	else
		i -> start();

	return true;
}

bool take_a_picture(source *const s, const std::string & snapshot_dir, const int quality, const bool handle_failure)
{
	if (!s)
		return false;

	uint64_t prev_ts = get_us();
	video_frame *pvf = s -> get_frame(handle_failure, prev_ts);

	if (!pvf)
		return false;

	std::string name = gen_filename(s, default_fmt, snapshot_dir, "snapshot-", "jpg", pvf->get_ts(), 0);
	create_path(name);

	log(LL_INFO, "Snapshot will be written to %s", name.c_str());

	FILE *fh = fopen(name.c_str(), "w");
	if (!fh) {
		log(LL_ERR, "fopen(%s) failed", name.c_str());
		delete pvf;
		return false;
	}

	auto img = pvf->get_data_and_len(E_JPEG);

	bool rc = fwrite(std::get<0>(img), 1, std::get<1>(img), fh) == std::get<1>(img);

	fclose(fh);

	delete pvf;

	return rc;
}

interface * start_a_video(source *const s, const std::string & snapshot_dir, const int quality, configuration_t *const cfg, const bool is_view_proxy)
{
	if (s) {
		const std::string cur_id = s->get_id() + "-" + myformat("%d", rand());
		const std::string descr = "snapshot started by HTTP server";

		std::vector<filter *> *const filters = new std::vector<filter *>();

#if HAVE_GSTREAMER == 1
		{
			interface *i = new target_avi(cur_id, descr, s, snapshot_dir, "snapshot-", default_fmt, quality, -1, -1, filters, "", "", "", -1, cfg, is_view_proxy, false, nullptr);
			i -> start();
			return i;
		}
#endif
#if HAVE_FFMPEG == 1
		{
			interface *i = new target_ffmpeg(cur_id, descr, "", s, snapshot_dir, "snapshot-", default_fmt, -1, -1, "mp4", 201000, filters, "", "", "", -1, cfg, is_view_proxy, false, nullptr);
			i -> start();
			return i;
		}
#endif

		delete filters;
	}

	return nullptr;
}

bool validate_file(const std::string & snapshot_dir, const bool with_subdirs, const std::string & filename)
{
	bool rc = false;

	auto *files = load_filelist(snapshot_dir, with_subdirs, "");

	for(auto cur : *files) {
		if (cur.name == filename) {
			rc = true;
			break;
		}
	}

	delete files;

	return rc;
}
