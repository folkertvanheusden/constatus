// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <atomic>
#include <errno.h>
#if HAVE_JANSSON == 1
#include <jansson.h>
#endif
#include <map>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "source.h"
#include "filter.h"
#include "instance.h"
#include "utils.h"
#include "log.h"
#include "error.h"
#include "motion_trigger.h"
#include "http_server.h"
#include "http_server_support.h"
#include "http_utils.h"
#include "target.h"
#include "controls.h"

void run_rest(h_handle_t & hh, configuration_t *const cfg, const std::string & path, const std::map<std::string, std::string> & pars, const std::string & snapshot_dir, const int quality)
{
#if HAVE_JANSSON == 1
	int code = 200;
	std::vector<std::string> *parts = split(path, "/");

	json_t *json = json_object();

	bool cmd = parts -> at(0) == "cmd";

	std::unique_lock<std::timed_mutex> lck(cfg->lock);

	interface *i = (parts -> size() >= 1 && !cmd) ? find_interface_by_id(cfg, parts -> at(0)) : nullptr;

	if (parts -> size() < 2) {
		code = 500;
		json_object_set_new(json, "msg", json_string("ID/cmd missing"));
		json_object_set_new(json, "result", json_false());
	}
	else if (cmd && parts -> at(1) == "list") {
		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());

		json_t *out_arr = json_array();
		json_object_set_new(json, "data", out_arr);

		for(instance * inst : cfg -> instances) {
			for(interface *cur : inst -> interfaces) {
				json_t *record = json_object();

				json_object_set_new(record, "id", json_string(cur -> get_id().c_str()));

				json_object_set_new(record, "id-descr", json_string(cur -> get_descr().c_str()));

				switch(cur -> get_class_type()) {
					case CT_HTTPSERVER:
						json_object_set_new(record, "type", json_string("http-server"));
						break;
					case CT_MOTIONTRIGGER:
						json_object_set_new(record, "type", json_string("motion-trigger"));
						break;
					case CT_TARGET:
						json_object_set_new(record, "type", json_string("stream-writer"));
						break;
					case CT_SOURCE:
						json_object_set_new(record, "type", json_string("source"));
						break;
					case CT_LOOPBACK:
						json_object_set_new(record, "type", json_string("video4linux-loopback"));
						break;
					default:
						json_object_set_new(record, "type", json_string("INTERNAL ERROR"));
						break;
				}

				json_object_set_new(record, "running", json_string(cur -> is_paused() ? "false" : "true"));

				json_object_set_new(record, "enabled", json_string(cur -> is_running() ? "true" : "false"));

				json_array_append_new(out_arr, record);
			}
		}
	}
	// commands that require an ID
	else if (i == nullptr) {
		code = 404;
		json_object_set_new(json, "msg", json_string("ID not found"));
		json_object_set_new(json, "result", json_false());
	}
	else if (cmd && parts -> at(1) == "list-files" && i -> get_class_type() == CT_TARGET) {
		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());

		json_t *out_arr = json_array();
		json_object_set_new(json, "data", out_arr);

		auto *files = load_filelist(((target *)i) -> get_target_dir(), false, "");

		for(auto file : *files) {
			json_t *record = json_object();

			json_object_set_new(record, "file", json_string(file.name.c_str()));
			json_object_set_new(record, "mtime", json_integer(file.last_change));
			json_object_set_new(record, "size", json_integer(file.size));

			json_array_append_new(out_arr, record);
		}

		delete files;
	}
	else if (cmd && parts -> at(1) == "take-a-snapshot" && i -> get_class_type() == CT_SOURCE) {
		if (take_a_picture(static_cast<source *>(i), snapshot_dir, quality, false)) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (cmd && parts -> at(1) == "start-a-recording" && i -> get_class_type() == CT_SOURCE) {
		const bool is_view_proxy = pars.find("view-proxy") != pars.end();

		interface *str = start_a_video(static_cast<source *>(i), snapshot_dir, quality, cfg, is_view_proxy);

		if (str) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());

			find_instance_by_interface(cfg, i) -> interfaces.push_back(str);
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}

	}
	else if (parts -> at(1) == "pause") {
		if (i -> pause()) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "unpause") {
		i -> unpause();
		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());
	}
	else if (parts -> at(1) == "stop" || parts -> at(1) == "start") {
		if (start_stop(i, parts -> at(1) == "start")) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "configure-motion-trigger" && parts -> size() == 4) {
		json_decref(json);
		json = ((motion_trigger *)i) -> rest_set_parameter(parts -> at(2), parts -> at(3));
	}
	else if (parts -> at(1) == "list-motion-trigger") {
		json_t *result = ((motion_trigger *)i) -> get_rest_parameters();

		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());
		json_object_set_new(json, "data", result);
	}
	else if (parts -> at(1) == "set-pan") {
		source *s = static_cast<source *>(i);

		if (s && parts->size() == 3 && s->has_pan_tilt()) {
			double pan = 0., tilt = 0.;
			s->get_pan_tilt(&pan, &tilt);
			
			pan = atof(parts->at(2).c_str());
			s->pan_tilt(pan, tilt);

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("invalid parameters"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "set-tilt") {
		source *s = static_cast<source *>(i);

		if (s && parts->size() == 3 && s->has_pan_tilt()) {
			double pan = 0., tilt = 0.;
			s->get_pan_tilt(&pan, &tilt);
			
			tilt = atof(parts->at(2).c_str());
			s->pan_tilt(pan, tilt);

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("invalid parameters"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "set-brightness") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c && parts->size() == 3) {
			c->set_brightness(atoi(parts->at(2).c_str()));

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("invalid parameters"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "set-contrast") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c && parts->size() == 3) {
			c->set_contrast(atoi(parts->at(2).c_str()));

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("invalid parameters"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "set-saturation") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c && parts->size() == 3) {
			c->set_saturation(atoi(parts->at(2).c_str()));

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("invalid parameters"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "get-pan") {
		source *s = static_cast<source *>(i);

		if (s && s->has_pan_tilt()) {
			double pan = 0., tilt = 0.;
			s->get_pan_tilt(&pan, &tilt);

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
			json_object_set_new(json, "data", json_real(pan));
		}
		else {
			json_object_set_new(json, "msg", json_string("device has no such control"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "get-tilt") {
		source *s = static_cast<source *>(i);

		if (s && s->has_pan_tilt()) {
			double pan = 0., tilt = 0.;
			s->get_pan_tilt(&pan, &tilt);

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
			json_object_set_new(json, "data", json_real(tilt));
		}
		else {
			json_object_set_new(json, "msg", json_string("device has no such control"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "get-brightness") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c && c->has_brightness()) {
			int b = c->get_brightness();

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
			json_object_set_new(json, "data", json_integer(b));
		}
		else {
			json_object_set_new(json, "msg", json_string("device has no such control"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "get-contrast") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c && c->has_contrast()) {
			int contr = c->get_contrast();

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
			json_object_set_new(json, "data", json_integer(contr));
		}
		else {
			json_object_set_new(json, "msg", json_string("device has no such control"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "get-saturation") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c && c->has_saturation()) {
			int s = c->get_saturation();

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
			json_object_set_new(json, "data", json_integer(s));
		}
		else {
			json_object_set_new(json, "msg", json_string("device has no such control"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "reset-controls") {
		controls *c = static_cast<source *>(i)->get_controls();

		if (c) {
			c->reset();

			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("device has no such setting"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else {
		code = 404;
	}

	lck.unlock();

	delete parts;

	char *js = json_dumps(json, JSON_COMPACT);
	std::string reply = myformat("HTTP/1.0 %d\r\nServer: " NAME " " VERSION "\r\nContent-Type: application/json\r\nDate: %s\r\n\r\n%s", code, date_header(0).c_str(), js);
	free(js);

	json_decref(json);

	if (WRITE_SSL(hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on rest-response");
#endif
}
