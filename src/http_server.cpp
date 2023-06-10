// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <algorithm>
#include <atomic>
#include <assert.h>
#include <cctype>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <jansson.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "instance.h"
#include "db.h"
#include "picio.h"
#include "source.h"
#include "utils.h"
#include "log.h"
#include "motion_trigger.h"
#include "meta.h"
#include "stats_tracker.h"
#include "source.h"
#include "target.h"
#include "target_avi.h"
#include "webservices.h"
#include "http_server.h"
#include "http_server_rest.h"
#include "http_server_support.h"
#include "icons.h"
#include "http_utils.h"
#include "stats_tracker.h"
#include "ws_server.h"
#include "ws_server_tls.h"
#include "view.h"
#include "http_auth.h"
#include "filter.h"
#include "controls.h"
#include "exec.h"
#include "http_cookies.h"
#include "default-stylesheet.h"

stats_tracker http_server::global_st { "global-http", false };

std::string date_header(const uint64_t ts)
{
	char date_str[128];

	time_t now = (ts ? ts : get_us()) / 1000000;

	struct tm tm;
	gmtime_r(&now, &tm);

	strftime(date_str, sizeof date_str, "%a, %d %b %Y %H:%M:%S %Z", &tm);

	return date_str;
}

bool sort_files_last_change(const file_t &left, const file_t & right)
{
	return left.last_change < right.last_change;
}

bool sort_files_size(const file_t &left, const file_t & right)
{
	return left.size < right.size;
}

bool sort_files_name(const file_t &left, const file_t & right)
{
	return left.name.compare(right.name) < 0;
}

std::string get_http_200_header(const std::string & cookie, const std::string & mime_type)
{
	return myformat("HTTP/1.0 200 OK\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
		"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
		"Date: %s\r\n"
		"%s"
		"Content-Type: %s\r\n"
		"Connection: close\r\n"
		"\r\n", date_header(0).c_str(), cookie.c_str(), mime_type.c_str());
}

std::string get_http_302_header(const std::string & target, const std::string & cookie = "")
{
	return myformat("HTTP/1.0 302 redirect\r\n"
		"Location: %s\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
		"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
		"Date: %s\r\n"
		"%s"
		"Content-Type: text/html\r\n"
		"Connection: close\r\n"
		"\r\n", target.c_str(), date_header(0).c_str(), cookie.c_str());
}

const std::string get_html_header(const bool columns, const std::string & controls = "")
{
	return std::string("<!DOCTYPE html>\n"
	"<html>\n"
	"\n"
	"<head>\n"
	" <meta charset=\"utf-8\">\n"
	" <meta name=\"viewport\" content=\"width=device-width, initial-scale=1, minimum-scale=1, shrink-to-fit=no\">\n"
	" <link rel=\"stylesheet\" href=\"stylesheet.css\">\n"
	" <link rel=\"shortcut icon\" href=\"favicon.ico\">\n"
	"<title>" NAME "</title>\n"
	"</head>\n"
	"\n"
	"<body>\n"
	" <header>\n"
	"  <h1><a href=\"index.html\">" NAME "</a>") +
	controls +
	std::string("</h1>\n"
	" </header>\n"
	"\n") + (columns ? " <main class=\"columns\">\n" : "<main>");
}

const std::string html_tail =
	" </main>\n"
	"\n"
	" <footer>\n"
	"  " NAME " " VERSION " is written by <a href=\"mailto:mail@vanheusden.com\">folkert van heusden</a><br>\n"
	"  <a href=\"https://vanheusden.com/constatus/\">vanheusden.com/constatus</a>\n"
	" </footer>\n"
	"</body>\n"
	"\n"
	"</html>\n";

std::string http_server::get_websocket_js()
{
	std::string reply;

#if HAVE_WEBSOCKETPP == 1
	if (ws) {
		ws_server *wsp = (ws_server *)ws;

		reply += "<script>\n";
		reply += "var peersSet = new Set();\n";

		reply += "function getCookie(cname) {\n";
		reply += "var name = cname + \"=\";\n";
		reply += "var decodedCookie = decodeURIComponent(document.cookie);\n";
		reply += "var ca = decodedCookie.split(';');\n";
		reply += "for(var i = 0; i <ca.length; i++) {\n";
		reply += "var c = ca[i];\n";
		reply += "while (c.charAt(0) == ' ') {\n";
		reply += "c = c.substring(1);\n";
		reply += "}\n";
		reply += "if (c.indexOf(name) == 0) {\n";
		reply += "return c.substring(name.length, c.length);\n";
		reply += "}\n";
		reply += "}\n";
		reply += "return \"\";\n";
		reply += "}\n";

		reply += "function start() {\n";
		if (websocket_url.empty()) {
			reply += "if (location.protocol == 'https:')\n";
			reply += myformat("s = new WebSocket('wss://' + location.hostname + ':%d/?' + getCookie('auth'));\n", wsp->get_port());
			reply += "else\n";
			reply += myformat("s = new WebSocket('ws://' + location.hostname + ':%d/?' + getCookie('auth'));\n", wsp->get_port());
		}
		else {
			reply += myformat("s = new WebSocket('%s?' + getCookie('auth'));\n", websocket_url.c_str());
		}

		reply += "s.onclose = function() { console.log('Websocket closed'); setTimeout(function(){ start(); }, 500); };\n";

		reply += "s.onopen = function() { console.log('Websocket connected');\n";
		reply += "setInterval(function() {\n";
		reply += "console.log('gstats');\n";
		reply += "s.send(JSON.stringify({ type: 'gstats' }));\n";
       		reply += "}, 500);\n";
		reply += "}\n";

		reply += "s.onmessage = function (event) {\n";
		reply += "try {\n";
		reply += "var msg = JSON.parse(event.data);\n";
		reply += "console.log(msg);\n";
		reply += "update_peers = false;\n";

		reply += "if (msg.type == 'register-peer') { peersSet.add(msg.data); update_peers = true; }\n";
		reply += "if (msg.type == 'unregister-peer') { peersSet.delete(msg.data); update_peers = true; }\n";

		reply += "if (update_peers) { var ul = document.getElementById('peers'); while (ul.firstChild) ul.removeChild(ul.firstChild); for (let item of peersSet) { var li = document.createElement('li'); li.appendChild(document.createTextNode(item)); ul.appendChild(li); } }\n";

		reply += "if (msg.type == 'motion-detected') { document.getElementById(msg.id).className = 'alert'; setTimeout(function(){ document.getElementById(msg.id).classList.remove('alert');  }, 3000); }\n";

		reply += "if (msg.type == 'gstats') {\n";
		reply += "document.getElementById('gbw').innerHTML = msg.gbw.toFixed(2);\n";
		reply += "document.getElementById('gcc').innerHTML = Math.ceil(msg.gcc);\n";
		reply += "document.getElementById('gcpu').innerHTML = msg.gcpu.toFixed(2);\n";
		reply += "}\n";

		reply += "} catch (error) {\n";
		reply += "console.error(error);\n";
		reply += "}\n";

		reply += "}\n";
		reply += "}\n";

		reply += "start();\n";

		reply += "</script>\n";
	}
#endif

	return reply;
}

std::string full_screen_js()
{
	std::string reply = "<script>function fs(which) {";
	reply += "elem = document.getElementById(which);\n";
	reply += "if (elem.requestFullscreen) { elem.requestFullscreen();\n";
	reply += "} else if (elem.mozRequestFullScreen) { /* Firefox */\n";
	reply += "elem.mozRequestFullScreen();\n";
	reply += "} else if (elem.webkitRequestFullscreen) { /* Chrome, Safari and Opera */\n";
	reply += "elem.webkitRequestFullscreen();\n";
	reply += "} else if (elem.msRequestFullscreen) { /* IE/Edge */\n";
	reply += "elem.msRequestFullscreen();\n";
	reply += "}\n";
	reply += "}</script>\n";

	return reply;
}

std::optional<std::pair<std::string, std::string> > find_header(const std::vector<std::string> & header_lines, const std::string & search_key)
{
	for(auto l : header_lines) {
		size_t pos = l.find(':');

		if (pos == std::string::npos)
			continue;

		std::string key = l.substr(0, pos);
		if (compare_equal_wo_case(key, search_key)) {
			std::string temp = l.substr(pos + 1);

			while(!temp.empty() && temp.at(0) == ' ')
				temp = temp.substr(1);

			return std::pair<std::string, std::string>(key, temp);
		}
	}

	return { };
}

std::map<std::string, std::string> find_pars(const std::string & q_in)
{
	std::map<std::string, std::string> pars;

	std::vector<std::string> *parts = split(q_in, "&");

	for(auto pair : *parts) {
		std::string key, value;

		if (ssize_t is = pair.find('='); is == std::string::npos)
			key = pair;
		else {
			key = pair.substr(0, is);
			value = pair.substr(is + 1);
		}

		pars.insert(std::pair<std::string, std::string>(key, value));
	}

	delete parts;

	return pars;
}

void http_server::send_404(http_thread_t *const ct, const std::string & path, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	log(LL_INFO, "Path %s not found", path.c_str());

	std::string reply = get_http_200_header(cookie, "text/html");
	reply += get_html_header(true);
	reply += myformat("<section><ul><li><strong>Unfortunately the page \"%s\" is not known</strong></li></ul></section>", path.c_str());
	reply += html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

const std::string action_failed = "<h2>%s</h2><p>Action failed</p>";
const std::string action_succeeded = "<h2>%s</h2><p>Action succeeded</p>";

void http_server::add_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars)
{
	std::string reply = get_http_302_header("index.html");

	auto db_it = pars.find("name");
	if (db_it != pars.end()) {
		std::string dashboard_name = db_it->second;

		log(LL_INFO, "Adding dashboard %s for %s", dashboard_name.c_str(), username.c_str());

		get_db()->add_id_to_dashboard(username, dashboard_name, DE_PLACEHOLDER, "--dummy--");
	}

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::add_to_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars)
{
	std::string reply = get_http_302_header("dashboard");

	auto db_it = pars.find("dashboard");
	if (db_it != pars.end()) {
		if (auto int_it = pars.find("int"); int_it != pars.end()) {

			std::string dashboard_name = db_it->second;

			reply = get_http_302_header("dashboard?dashboard=" + dashboard_name);

			log(LL_INFO, "Adding %s to dashboard %s of %s", int_it->second.c_str(), dashboard_name.c_str(), username.c_str());

			if (pars.find("view-proxy") != pars.end())
				get_db()->add_id_to_dashboard(username, dashboard_name, DE_VIEW, int_it->second);
			else
				get_db()->add_id_to_dashboard(username, dashboard_name, DE_SOURCE, int_it->second);
		}
	}

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::remove_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars)
{
	std::string reply = get_http_302_header("index.html");

	auto db_it = pars.find("dashboard");
	if (db_it != pars.end())
		get_db()->remove_dashboard(username, db_it->second);
	else
		log(LL_INFO, "dashboard missing in get-string");

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::remove_from_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars)
{
	auto db_it = pars.find("dashboard");
	if (db_it != pars.end()) {
		if (auto int_it = pars.find("nr"); int_it != pars.end()) {
			std::string dashboard_name = db_it->second;

			log(LL_INFO, "Removing %s from dashboard %s of %s", int_it->second.c_str(), dashboard_name.c_str(), username.c_str());

			get_db()->del_id_from_dashboard(username, dashboard_name, atoi(int_it->second.c_str()));
		}

		std::string reply = get_http_302_header("dashboard?dashboard=" + db_it->second);

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else {
		send_404(ct, "dashboard", pars, "");
	}
}

void http_server::send_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	auto db_it = pars.find("dashboard");
	if (db_it == pars.end()) {
		send_404(ct, "dashboard", pars, cookie);
		return;
	}

	auto dashboard_name = db_it->second;

	auto views = get_db()->get_dashboard_ids(username, dashboard_name);

	std::string reply = get_http_200_header(cookie, "text/html") + get_html_header(true, "<div id=\"fs\" class=\"controls\"></div>");

	reply += full_screen_js();

	reply += get_websocket_js();

	bool empty = true;

	for(auto v : views) {
		bool ok = true;

		if (v.type == DE_SOURCE) {
			std::string img_url, page_url;
			mjpeg_stream_url(cfg, v.id, &img_url, &page_url);

			instance *inst = nullptr;
			interface *i = nullptr;
			find_by_id(cfg, v.id, &inst, &i);

			if (i) {
				const std::string id = i->get_id();

				reply += myformat("<section><figure><div class=\"fs-div\"><img class=\"fs-big\" onclick=\"fs('%s')\" src=\"%s\" id=\"%s\" title=\"%s\"><img class=\"fs-small\" onclick=\"fs('%s')\" src=\"images/fullscreen.svg\"></div></figure><ul><li><a href=\"remove-from-dashboard?nr=%ld&dashboard=%s\">remove</a></ul></section>", id.c_str(), img_url.c_str(), id.c_str(), i->get_descr().c_str(), id.c_str(), v.nr, dashboard_name.c_str());
			}
			else {
				ok = false;
			}

			empty = false;
		}
		else if (v.type == DE_VIEW) {
			view *sv = find_view(this->views, v.id);

			if (sv) {
				const std::string id = sv->get_id();

				reply += myformat("<section><figure><div class=\"fs-div\"><img class=\"fs-big\" onclick=\"fs('%s')\" src=\"stream.mjpeg?int=%s&view-proxy\" id=\"%s\" class=\"\" title=\"%s\"><img class=\"fs-small\" onclick=\"fs('%s')\" src=\"images/fullscreen.svg\"></div></figure><ul><li><a href=\"remove-from-dashboard?nr=%lu&dashboard=%s\">remove</a></ul></section>", id.c_str(), v.id.c_str(), id.c_str(), sv->get_descr().c_str(), id.c_str(), v.nr, dashboard_name.c_str());
			}
			else {
				ok = false;
			}

			empty = false;
		}
		else {
			// DE_PLACEHOLDER
		}

		if (!ok)
			reply += myformat("<section><figure><strong>ID %s not found</strong></figure><ul><li><a href=\"remove-from-dashboard?nr=%lu&dashboard=%s\">remove</a></ul></section>", v.id.c_str(), v.nr, dashboard_name.c_str());
	}

	if (empty)
		reply += myformat("<section><ul><li><strong>Please add sources/views</strong></li></ul></figure>");
	else {
		reply += "<script>";
		reply += "document.getElementById('fs').insertAdjacentHTML('afterbegin', '<a href=\"fs-db.html?width=' + window.outerWidth + '&height=' + window.outerHeight + '&dashboard=" + dashboard_name + "\"><img src=\"images/fullscreen.svg\"></a> &nbsp; <a href=\"del-dashboard?dashboard=" + dashboard_name + "\"><img src=\"images/trash.svg\"></a>');";
		reply += "</script>";
	}

	reply += html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::send_fs_db_html(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	auto db_it = pars.find("dashboard");
	if (db_it == pars.end()) {
		send_404(ct, "dashboard", pars, cookie);
		return;
	}

	auto dashboard_name = db_it->second;

	auto width_it = pars.find("width");
	if (width_it == pars.end()) {
		send_404(ct, "dashboard", pars, cookie);
		return;
	}

	int width = atoi(width_it->second.c_str());

	auto height_it = pars.find("height");
	if (height_it == pars.end()) {
		send_404(ct, "dashboard", pars, cookie);
		return;
	}

	int height = atoi(height_it->second.c_str());

	// send html headers & page header asap
	std::string reply = get_http_200_header(cookie, "text/html") + get_html_header(false) + full_screen_js();

	reply += "<style>#page{ display: none; }\n";
	reply += "#loading{ display: block; }</style>\n";

	reply += "<div id=\"loading\"><section><h3>Please wait while loading...</h3><p><br></p></section></div>";

	set_no_delay(ct->hh.fd, true);
	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");

	reply.clear();

	reply += "<div id=\"page\">";

	// now start loading etc

	auto views = get_db()->get_dashboard_ids(username, dashboard_name);

	std::mutex value_locks;
	int max_xres = 0, max_yres = 0, n_screens = 0;
	std::vector<std::thread *> thread_handles;

	for(auto v : views) {
		if (v.type == DE_SOURCE) {
			thread_handles.push_back(new std::thread([this, ct, v, dashboard_name, &max_xres, &max_yres, &n_screens, &value_locks]() {
				set_thread_name("dbS" + dashboard_name);

				instance *inst = nullptr;
				interface *i = nullptr;
				find_by_id(cfg, v.id, &inst, &i);

				if (i) {
					source *s = (source *)i;

					s->start();
					s->wait_for_meta();

					std::unique_lock<std::mutex> lock(value_locks);

					//log(LL_DEBUG, "%s: %dx%d", s->get_id().c_str(), s->get_width(), s->get_height());
					max_xres = std::max(max_xres, s->get_width());
					max_yres = std::max(max_yres, s->get_height());
					n_screens++;

					lock.unlock();

					s->stop();

				}
			}));
		}
		else if (v.type == DE_VIEW) {
			thread_handles.push_back(new std::thread([this, ct, v, dashboard_name, &max_xres, &max_yres, &n_screens, &value_locks]() {
				set_thread_name("dbV" + dashboard_name);

				view *sv = find_view(this->views, v.id);

				if (sv) {
					sv->start();
					sv->wait_for_meta();

					std::unique_lock<std::mutex> lock(value_locks);

					// log(LL_DEBUG, "%s: %dx%d", sv->get_id().c_str(), sv->get_width(), sv->get_height());
					max_xres = std::max(max_xres, sv->get_width());
					max_yres = std::max(max_yres, sv->get_height());
					n_screens++;

					lock.unlock();

					sv->stop();
				}
			}));
		}
	}

	for(auto t : thread_handles) {
		t->join();
		delete t;
	}

	// max sure they're at least one because they're from user input
	int screen_w = std::max(1, width);
	int screen_h = std::max(1, height);

	int nw = ceil(sqrt(n_screens));
	int nh = ceil(double(n_screens) / nw);

	double x_ratio = max_xres * nw / double(screen_w);
	double y_ratio = max_yres * nh / double(screen_h);

	double smallest_ratio = std::min(x_ratio, y_ratio);

	max_xres *= smallest_ratio;
	max_yres *= smallest_ratio;
	
	width = max_xres * nw;
	height = max_yres * nh;

	reply += "<section><h3>Click on picture to start full screen</h3>";
	reply += myformat("<img onclick=\"fs('pic')\" id=\"pic\" src=\"fs-db.mjpeg?dashboard=%s&width=%d&height=%d&nw=%d&nh=%d&feed_w=%d&feed_h=%d\">", dashboard_name.c_str(), width, height, nw, nh, max_xres, max_yres);
	reply += "</section>";

	reply += "<script>\n";
	reply += "document.getElementById(\"page\").style.display = \"block\";\n";
	reply += "document.getElementById(\"loading\").style.display = \"none\";\n";
	reply += "</script>\n";

	reply += "</div>";

	reply += html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::file_menu_db(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + get_html_header(false);
	reply += "<h2>files "+ get_id() + "</h2>\n";

	/// certain file
	auto it_file = pars.find("file_nr");
	if (it_file != pars.end()) {
		std::string filename = get_db()->retrieve_filename(atol(it_file->second.c_str()));

		size_t last_slash = filename.rfind('/');
		filename = last_slash == std::string::npos ? filename : filename.substr(last_slash + 1);

		reply += "<section><h3>" + filename + "</h3>\n";
		reply += myformat("<video controls autoplay><source src=\"send-file-db?file=%lu\">Your browser does not support the video tag.</video>\n", atol(it_file->second.c_str()));
		reply += "</section>\n";
	}

	/// files for a certain day
	auto it_day = pars.find("date");
	if (it_day != pars.end()) {
		auto files_for_a_date = get_db()->list_files_for_a_date(it_day->second);

		reply += "<section><h3>files for " + it_day->second + "</h3>\n";

		reply += "<table>\n";
		reply += "<tr>\n";
		reply += "<th>name</th>\n";
		reply += "<th>age</th>\n";
		reply += "<th>size</th>\n";
		reply += "<th>download</th>\n";
		reply += "</tr>\n";
		for(auto file_pair : files_for_a_date) {
			size_t last_slash = file_pair.second.rfind('/');

			std::string file_size = get_file_size(file_pair.second);

			auto file_age = get_file_age(file_pair.second);

			std::string file = last_slash == std::string::npos ? file_pair.second : file_pair.second.substr(last_slash + 1);

			reply += "<tr>\n";
			reply += myformat("<td><a href=\"?file_nr=%lu\">%s</a></td>", file_pair.first, file.c_str());
			reply += "<td><time title=\"" + file_age.first + "\">" + file_age.second + "</time></td>";
			reply += "<td>" + file_size + "</td>";
			reply += myformat("<td><a href=\"send-file-db?download&file_nr=%lu\">download</a></td>", file_pair.first);
			reply += "</tr>\n";
		}
		reply += "</table>\n";

		reply += "</section>\n";
	}

	/// days with data
	std::vector<std::string> days_with_data = get_db()->list_days_with_data();

	reply += "<section><h3>days with data</h3>\n";

	reply += "<p>";
	for(auto d : days_with_data) {
		reply += "<a style=\"white-space: nowrap;\" href=\"?date=" + d + "\">" + d + "</a> &nbsp; ";
	}
	reply += "</p></section>";

	reply += html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::file_menu(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + page_header + "<section><h2>list of files in " + snapshot_dir + "</h2><table>";

	auto *files = load_filelist(snapshot_dir, with_subdirs, "");

	std::string order;

	if (auto order_it = pars.find("order"); order_it != pars.end())
		order = order_it -> second;

	if (order == "" || order == "last-change")
		std::sort(files -> begin(), files -> end(), sort_files_last_change);
	else if (order == "name")
		std::sort(files -> begin(), files -> end(), sort_files_name);
	else if (order == "size")
		std::sort(files -> begin(), files -> end(), sort_files_size);
	else
		reply += myformat("Ignoring unknown sort method %s", order.c_str());

	reply += "<tr><th><a href=\"?order=name\">name</a></th><th><a href=\"?order=last-change\">last change</a></th><th><a href=\"?order=size\">size</a></th><th></th></tr>";

	for(auto file : *files) {
		auto age = get_file_age(file.last_change);

		std::string dl = "download&";
		std::string view = myformat("<a href=\"view-file?file=%s\">view</a>", file.name.c_str());

		if (file.name.size() > 3) {
			std::string ext = file.name.substr(file.name.size() - 3);

			if (ext == "log" || ext == "txt" || ext == "png" || ext == "jpg" || ext == "mp4") {
				dl.clear();

				view = myformat("<a href=\"send-file?file=%s\">view</a>", file.name.c_str());
			}
		}

		reply += "<tr><td><a title=\"" + age.first + "\" href=\"send-file?" + dl + "file=" + file.name + "\">" + file.name + "</a></td><td><time title=\"" + age.first + "\">" + age.second + "</time></td><td>" + get_file_size(file.size) + "</td><td>" + view + "</td></tr>";
	}

	delete files;

	reply += "</table></section>" + html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void calc_global_http_stats(const configuration_t *const cfg, int *const bw, int *const conn_count)
{
	*bw = 0;
	*conn_count = 0;

	std::unique_lock<std::timed_mutex> lock(cfg->lock, std::defer_lock);

	if (lock.try_lock_for(std::chrono::milliseconds(150))) {
		for(auto inst : cfg->instances) {
			for(auto i : inst->interfaces) {
				if (i->get_class_type() == CT_HTTPSERVER) {
					http_server *hs = static_cast<http_server *>(i);
					(*bw) += hs->get_bw();
					(*conn_count) += hs->get_connection_count();
				}
			}
		}

		for(auto i : cfg->global_http_servers) {
			http_server *hs = static_cast<http_server *>(i);
			(*bw) += hs->get_bw();
			(*conn_count) += hs->get_connection_count();
		}
	}
}

void http_server::mjpeg_stream_url(configuration_t *const cfg, const std::string & id, std::string *const img_url, std::string *const page_url)
{
	instance *inst = nullptr;
	interface *i = nullptr;
	find_by_id(cfg, id, &inst, &i);

	if (!inst) {
		*img_url = *page_url = "id unknown";
		return;
	}

	*img_url = myformat("stream.mjpeg?inst=%s", url_escape(inst -> name).c_str()); 
	*page_url = myformat("index.html?inst=%s", url_escape(inst -> name).c_str()); 
}

void http_server::register_peer(const bool is_register, const std::string & peer_name)
{
#if HAVE_WEBSOCKETPP == 1
	ws_server *pws = static_cast<ws_server *>(ws);
	if (pws && ws_privacy == false) {
		json_t *json = json_object();

		if (is_register)
			json_object_set_new(json, "type", json_string("register-peer"));
		else
			json_object_set_new(json, "type", json_string("unregister-peer"));

		json_object_set_new(json, "data", json_string(peer_name.c_str()));

		char *js = json_dumps(json, JSON_COMPACT);
		std::string msg = js;
		free(js);

		json_decref(json);

		pws->push(msg, peer_name + "|register");
	}
#endif

	if (notify_viewer_script.empty() == false)
		fire_and_forget(notify_viewer_script, myformat("%d", is_register));
}

void http_server::send_stylesheet(http_thread_t *const ct, const std::string & cookie)
{
	size_t last_slash = stylesheet.rfind('/');
	std::string path = last_slash == std::string::npos ? cfg->search_path: stylesheet.substr(0, last_slash);
	std::string file = last_slash == std::string::npos ? stylesheet.c_str() : stylesheet.substr(last_slash + 1);

	log(id, LL_DEBUG, "Send stylesheet \"%s\"", stylesheet.c_str());

	if (file.empty())
		file = "stylesheet.css";

	if (std::filesystem::exists(path + file)) {
		send_file(ct->hh, path, file.c_str(), false, st, cookie, false);
	}
	else {
		log(id, LL_WARNING, "\"%s\" not found, using built-in stylesheet", file.c_str());

		std::string reply = get_http_200_header(cookie, "text/css") + "\r\n";

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, (const char *)sc_css, sc_css_len) <= 0)
			log(id, LL_DEBUG, "short write");
	}
}

void http_server::send_copypaste(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	std::string key;

	if (auto key_it = pars.find("buffer"); key_it != pars.end())
		key = key_it -> second;

	std::pair<uint64_t, video_frame *> value;
	if (get_meta()->get_bitmap(key, &value)) {
		std::string headers = get_http_200_header(cookie, "image/jpeg");

		if (WRITE_SSL(ct->hh, headers.c_str(), headers.size()) <= 0)
			log(LL_DEBUG, "short write on response header");

		auto rc = value.second->get_data_and_len(E_JPEG);

		if (WRITE_SSL(ct->hh, (const char *)std::get<0>(rc), std::get<1>(rc)) <= 0)
			log(LL_DEBUG, "short write on response data");

		delete value.second;
	}
}

void http_server::send_gstats(http_thread_t *const ct, const std::string & cookie)
{
        json_t *json = json_object();

        struct rusage ru { 0 };
        getrusage(RUSAGE_SELF, &ru);
        json_object_set_new(json, "rss", json_integer(ru.ru_maxrss));

        json_object_set_new(json, "gbw", json_real(http_server::global_st.get_bw() / 1024.0));
        json_object_set_new(json, "gcc", json_real(http_server::global_st.get_cc()));

        json_object_set_new(json, "gcpu", json_real(cfg->st->get_cpu_usage() * 100.0));

        char *js = json_dumps(json, JSON_COMPACT);
        std::string reply = myformat("HTTP/1.0 200 OK\r\nServer: " NAME " " VERSION "\r\nContent-Type: application/json\r\n%s\r\n%s", cookie.c_str(), js);
        free(js);

        json_decref(json);

        if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
                log(LL_DEBUG, "short write on response");
}

void http_server::send_stats(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	std::string interface_name;

	if (auto int_it = pars.find("module"); int_it != pars.end())
		interface_name = int_it -> second;

	interface *i = nullptr;
	if (interface_name.empty())
		log(LL_INFO, "stats: no module (interface) selected");
	else {
		i = find_interface_by_id(cfg, interface_name);

		if (i) {
			json_t *json = json_object();
			json_object_set_new(json, "cpu-usage", json_real(i->get_cpu_usage() * 100));
			auto fps = i->get_fps();
			json_object_set_new(json, "fps", json_real(fps.has_value() ? fps.value() : -1));
			json_object_set_new(json, "bw", json_integer(i->get_bw() / 1024));

			if (i->get_class_type() == CT_HTTPSERVER)
				json_object_set_new(json, "cc", json_integer(((http_server *)i)->get_connection_count()));

			char *js = json_dumps(json, JSON_COMPACT);
			std::string reply = myformat("HTTP/1.0 200 OK\r\nServer: " NAME " " VERSION "\r\nContent-Type: application/json\r\n%s\r\n%s", cookie.c_str(), js);
			free(js);

			json_decref(json);

			if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response");
		}
		else {
			log(LL_WARNING, "Module \"%s\" not found", interface_name.c_str());
		}
	}
}

bool http_server::send_snapshot(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie, const bool db)
{
	bool force_dl = pars.find("download") != pars.end();

	bool rc = false;

	std::string file;

	if (db) {
		if (auto it_file = pars.find("file_nr"); it_file != pars.end())
			file = get_db()->retrieve_filename(atol(it_file->second.c_str()));

		rc = send_file(ct->hh, snapshot_dir, file.c_str(), force_dl, st, cookie, true);
	}
	else {
		if (auto file_it = pars.find("file"); file_it != pars.end())
			file = file_it -> second;

		if (!file.empty() && validate_file(snapshot_dir, with_subdirs, file))
			rc = send_file(ct->hh, snapshot_dir, file.c_str(), force_dl, st, cookie, false);
	}

	if (!rc)
		send_404(ct, file, pars, cookie);

	return rc;
}

void http_server::view_view(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie)
{
	std::string id;

	if (auto view_it = pars.find("id"); view_it != pars.end())
		id = view_it -> second;

	view *v = find_view(this->views, id);

	std::string reply;
	if (v)
		reply = get_http_200_header(cookie, "text/html") + v -> get_html(pars);

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response");
}

void http_server::view_menu(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie, const std::string & username)
{
	std::string id;

	if (auto view_it = pars.find("id"); view_it != pars.end())
		id = view_it -> second;

	std::string reply = get_http_200_header(cookie, "text/html") + get_html_header(true);
	
	std::string iup = myformat("?int=%s&view-proxy", id.c_str());

	reply += "<section><ul>" +
		myformat("<li><a href=\"stream.mjpeg%s\">MJPEG without controls</a>"
			"<li><a href=\"stream.html%s\">MJPEG with controls</a>"
			"<li><a href=\"stream.ogg%s\">OGG (Theora) w/o controls</a>"
			"<li><a href=\"stream_ogg.html%s\">MJPEG with controls</a>"
			"<li><a href=\"stream.mpng%s\">MPNG stream w/o controls</a>"
			"<li><a href=\"image.jpg%s\">Show snapshot (JPEG)</a>"
			"<li><a href=\"image.png%s\">Show snapshot (PNG)</a>", iup.c_str(), iup.c_str(), iup.c_str(), iup.c_str(), iup.c_str(), iup.c_str(), iup.c_str());

	if (allow_admin) {
		reply += myformat("<li><a href=\"snapshot-img/%s\">Take a snapshot and store it on disk</a>"
				"<li><a href=\"snapshot-video/%s\">Start a video recording</a>", iup.c_str(), iup.c_str());
	}

	if (archive_acces)
		reply += "<li><a href=\"view-snapshots\">View recordings</a>";

	if (get_db()->using_db()) {
		auto dashboards = get_db()->get_dashboards(username);

		for(auto d : dashboards)
			reply += myformat("<li><a href=\"add-to-dashboard?int=%s&view-proxy&dashboard=%s\">Add to dashboard \"%s\"</a>", id.c_str(), d.c_str(), d.c_str());
	}

	reply += "</ul></section>";

	reply += html_tail; 

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response");
}

void http_server::view_file(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, const std::string & cookie)
{
	std::string file;

	if (auto file_it = pars.find("file"); file_it != pars.end())
		file = file_it -> second;

	std::string reply = get_http_200_header(cookie, "text/html") + page_header + 
		myformat("<video controls autoplay><source src=\"send-file?file=%s\">Your browser does not support the video tag.</video>", file.c_str())
		+ html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response");
}

void http_server::pause_module(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, const std::string & path, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + "???";

	std::string module;

	if (auto mod_it = pars.find("module"); mod_it != pars.end())
		module = mod_it -> second;

	std::unique_lock<std::timed_mutex> lck(cfg->lock);

	if (path == "pause" || path == "unpause") {
		if (::pause(inst, module, path == "pause"))
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "pause/unpause") + html_tail;
		else
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_failed.c_str(), "pause/unpause") + html_tail;
	}
	else {
		if (::toggle_pause(inst, module))
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "pause/unpause") + html_tail;
		else
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_failed.c_str(), "pause/unpause") + html_tail;
	}

	lck.unlock();

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::start_stop_module(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, const std::string & path, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + "???";

	std::string module;

	if (auto mod_it = pars.find("module"); mod_it != pars.end())
		module = mod_it -> second;

	std::unique_lock<std::timed_mutex> lck(cfg->lock);

	interface *i = find_interface_by_id(inst, module);

	if (path == "toggle-start-stop") {
		::toggle_start_stop(i);
		reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "start/stop") + html_tail;
	}
	else {
		if (::start_stop(i, path == "start"))
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "start/stop") + html_tail;
		else
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_failed.c_str(), "start/stop") + html_tail;
	}

	lck.unlock();

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::restart_module(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + "???";

	std::string module;

	if (auto mod_it = pars.find("module"); mod_it != pars.end())
		module = mod_it -> second;

	std::unique_lock<std::timed_mutex> lck(cfg->lock);

	interface *i = find_interface_by_id(inst, module);

	if (!i)
		reply = "???";
	else if (start_stop(i, false)) {
		if (start_stop(i, true))
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "restart") + html_tail;
		else
			reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_failed.c_str(), "restart") + html_tail;
	}

	lck.unlock();

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::take_snapshot(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, source *const s, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + "???";

	if (take_a_picture(s, snapshot_dir, quality, handle_failure))
		reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "snapshot image") + html_tail;
	else
		reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_failed.c_str(), "snapshot image") + html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::video_snapshot(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, source *const s, const bool is_view_proxy, const std::string & cookie)
{
	interface *i = start_a_video(s, snapshot_dir, quality, cfg, is_view_proxy);

	std::string reply = get_http_200_header(cookie, "text/html") + "???";
	if (i) {
		reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_succeeded.c_str(), "snapshot video") + html_tail;

		const std::lock_guard<std::timed_mutex> lock(cfg->lock);
		inst -> interfaces.push_back(i);
	}
	else {
		reply = get_http_200_header(cookie, "text/html") + page_header + myformat(action_failed.c_str(), "snapshot video") + html_tail;
	}

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::authorize(http_thread_t *const ct, const std::vector<std::string> & header_lines, bool *const auth_ok, std::string *const username, std::string *const cookie)
{
	auto authorization = find_header(header_lines, "Authorization");

	*auth_ok = false;

	if (authorization.has_value()) {
		std::vector<std::string> *parts = split(authorization.value().second, " ");

		if (parts->size() != 2)
			*auth_ok = false;
		else if (str_tolower(parts->at(0)) != "basic")
			*auth_ok = false;
		else {
			std::string decoded = base64_decode(parts->at(1));

			size_t colon = decoded.find(":");
			if (colon == std::string::npos)
				*auth_ok = false;
			else {
				*username = decoded.substr(0, colon);
				std::string password = un_url_escape(decoded.substr(colon + 1));

				auto result = auth->authenticate(*username, password);
				if (std::get<0>(result))
					*auth_ok = true;
				else {
					log(LL_WARNING, "HTTP authentication failed for %s: %s (authorize)", username->c_str(), std::get<1>(result).c_str());
					*auth_ok = false;
				}
			}
		}

		delete parts;

		if (*auth_ok) {
			std::string new_cookie = c->get_cookie(*username);

			*cookie = myformat("Set-Cookie: auth=%s\r\n", new_cookie.c_str());

			return;
		}

		return;
	}

	for(auto l : header_lines) {
		log(LL_DEBUG, "Check cookies %s", l.c_str());

		if (l.substr(0, 7) == "Cookie:") {
			std::string value = l.substr(7);
			while(value.empty() == false && value.at(0) == ' ')
				value = value.substr(1);
			std::vector<std::string> *parts = split(value, ";");

			for(auto kv : *parts) {
				std::string key = kv;
				while(key.empty() == false && key.at(0) == ' ')
					key = key.substr(1);

				if (key.substr(0, 5) == "auth=") {
					std::string value = key.substr(5);

					*username = c->get_cookie_user(value);

					if (username->empty() == false) {
						*auth_ok = true;

						*cookie = myformat("Set-Cookie: auth=%s\r\n", value.c_str());

						c->update_cookie(*username, value);
						break;
					}
				}
			}

			delete parts;
		}
	}
}

void http_server::send_stream_html(http_thread_t *const ct, const std::string & page_header, const std::string & iup, source *const s, const bool view_proxy, const std::string & cookie, const bool ogg)
{
	std::string reply = get_http_200_header(cookie, "text/html") + page_header;

	std::string caption = s->get_descr().empty() == false ? s->get_descr() : s->get_id();

	std::string vp = view_proxy ? "&view-proxy" : "";

	reply += get_websocket_js();

	reply += full_screen_js();

	const std::string id = s->get_id();

	if (ogg)
		reply += myformat("<section><figure><div class=\"fs-div\"><video class=\"fs-big\" onclick=\"fs('%s')\" src=\"stream.ogg%s%s\" id=\"%s\" autoplay><img class=\"fs-small\" onclick=\"fs('%s')\" src=\"images/fullscreen.svg\"></div><figcaption>%s</figcaption></figure>", id.c_str(), iup.c_str(), vp.c_str(), id.c_str(), id.c_str(), caption.c_str());
	else {
		reply += myformat("<section><figure><div class=\"fs-div\"><img class=\"fs-big\" onclick=\"fs('%s')\" src=\"stream.ogg%s%s\" id=\"%s\"><img class=\"fs-small\" onclick=\"fs('%s')\" src=\"images/fullscreen.svg\"></div><figcaption>%s</figcaption></figure>", id.c_str(), iup.c_str(), vp.c_str(), id.c_str(), id.c_str(), caption.c_str());
	}

	// controls

	reply += "<script>\n";
	reply += "function reset_controls() {\n";
	reply += "action_reset = async () => {\n";
	reply += myformat("const urlr = 'rest/%s/reset-controls/';\n", s->get_id().c_str());
	reply += "console.log(urlr);\n";
	reply += "await fetch(urlr);\n";

	reply += myformat("const urlb = 'rest/%s/get-brightness';\n", s->get_id().c_str());
	reply += "console.log(urlb);\n";
	reply += "const responseb = await fetch(urlb);\n";
	reply += "const jb = await responseb.json(); console.log(jb);\n";
	reply += "document.getElementById('brightness').value = jb['data'] * 100 / 65535;\n";

	reply += myformat("const urls = 'rest/%s/get-saturation';\n", s->get_id().c_str());
	reply += "console.log(urls);\n";
	reply += "const responses = await fetch(urls);\n";
	reply += "const j_s = await responses.json(); console.log(j_s);\n";
	reply += "document.getElementById('saturation').value = j_s['data'] * 100 / 65535;\n";

	reply += myformat("const urlc = 'rest/%s/get-contrast';\n", s->get_id().c_str());
	reply += "console.log(urlc);\n";
	reply += "const responsec = await fetch(urlc);\n";
	reply += "const jc = await responsec.json(); console.log(jc);\n";
	reply += "document.getElementById('contrast').value = jc['data'] * 100 / 65535;\n";

	reply += "}\n";
	reply += "action_reset();\n";
	reply += "}\n\n";
	reply += "function change(w) {\n";

	reply += "v = w.value;\n";
	reply += "if (v > 100) { v = 100; w.value = v; }\n";
	reply += "if (v < 0) { v = 0; w.value = v; }\n";
	reply += "const cur = 65536 * v / 100;\n";

	reply += "action_put = async () => {\n";
	reply += myformat("const url = 'rest/%s/set-' + w.id + '/' + cur;\n", s->get_id().c_str());
	reply += "console.log(url);\n";
	reply += "const response = await fetch(url);\n";
	reply += "const j = await response.json(); console.log(j);\n";
	reply += "}\n"; // async
	reply += "action_put();\n";

	reply += "}\n"; // change
	

	// pan tilt


	reply += "function reset_controls_pt() {\n";
	reply += "action_reset_pt = async () => {\n";

	reply += myformat("const urlp = 'rest/%s/get-pan';\n", s->get_id().c_str());
	reply += "console.log(urlp);\n";
	reply += "const reponsep = await fetch(urlp);\n";
	reply += "const jp = await reponsep.json(); console.log(jp);\n";
	reply += "document.getElementById('pan').value = jp['data'];\n";

	reply += myformat("const urlt = 'rest/%s/get-tilt';\n", s->get_id().c_str());
	reply += "console.log(urlt);\n";
	reply += "const reponset = await fetch(urlt);\n";
	reply += "const jt = await reponset.json(); console.log(jt);\n";
	reply += "document.getElementById('tilt').value = jt['data'];\n";

	reply += "}\n";
	reply += "action_reset_pt();\n";
	reply += "}\n\n";

	reply += "function change_pt(w) {\n";

	reply += "v = w.value;\n";
	reply += "if (v > 180) { v = 180; w.value = v; }\n";
	reply += "if (v < -180) { v = -180; w.value = v; }\n";
	reply += "const cur = v;\n";

	reply += "action_put_pt = async () => {\n";
	reply += myformat("const url = 'rest/%s/set-' + w.id + '/' + cur;\n", s->get_id().c_str());
	reply += "console.log(url);\n";
	reply += "const response = await fetch(url);\n";
	reply += "const j = await response.json(); console.log(j);\n";
	reply += "}\n"; // async
	reply += "action_put_pt();\n";

	reply += "}\n"; // change

	reply += "function reset() {\n";
	reply += "reset_controls();\n";
	reply += "reset_controls_pt();\n";
	reply += "}\n";
	reply += "</script>\n";

	reply += "<table>\n";

	bool button = false;

	controls *c = s->get_controls();
	if (c) {
		if (c->has_brightness()) {
			int v = c->get_brightness() * 100.0 / 65536;
			reply += myformat("<tr><td>brightness:</td><td><input size=5 type=\"text\" id=\"brightness\" value=\"%d\" onchange=\"change(this)\"></td></tr>\n", v);
			button = true;
		}

		if (c->has_contrast()) {
			int v = c->get_contrast() * 100.0 / 65536;
			reply += myformat("<tr><td>contrast:</td><td><input size=5 type=\"text\" id=\"contrast\" value=\"%d\" onchange=\"change(this)\"></td></tr>\n", v);
			button = true;
		}

		if (c->has_saturation()) {
			int v = c->get_saturation() * 100.0 / 65536;
			reply += myformat("<tr><td>saturation:</td><td><input size=5 type=\"text\" id=\"saturation\" value=\"%d\" onchange=\"change(this)\"></td></tr>\n", v);
			button = true;
		}
	}

	if (s->has_pan_tilt()) {
		double pan = 0., tilt = 0.;
		s->get_pan_tilt(&pan, &tilt);

		reply += myformat("<tr><td>pan:</td><td><input size=5 type=\"text\" id=\"pan\" value=\"%.2f\" onchange=\"change_pt(this)\"></td></tr>\n", pan);

		reply += myformat("<tr><td>tilt:</td><td><input size=5 type=\"text\" id=\"tilt\" value=\"%.2f\" onchange=\"change_pt(this)\"></td></tr>\n", tilt);

		button = true;
	}

	if (button)
		reply += myformat("<tr><td></td><td><input type=\"reset\" onclick=\"reset()\"></td></tr>\n");

	reply += "</table>\n";

	reply += "</section>" + html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::do_auth(http_thread_t *const ct, const std::vector<std::string> & header_lines)
{
	auto content_length = find_header(header_lines, "Content-Length");
	if (!content_length.has_value()) {
		log(get_id(), LL_WARNING, "Content-Length missing for do-auth");
		return;
	}

	char request_data[4096];

	int cl = atoi(content_length.value().second.c_str());
	if (cl >= sizeof request_data) { // not likely
		log(get_id(), LL_WARNING, "Content-Length >= %zu (do-auth)", sizeof request_data);
		return;
	}

	int h_n = 0;

	struct pollfd fds[1] { { ct->hh.fd, POLLIN, 0 } };

	while(!local_stop_flag && h_n < cl) {
		if (AVAILABLE_SSL(ct->hh) == 0 && poll(fds, 1, 250) == 0)
			continue;

		int rc = READ_SSL(ct->hh, &request_data[h_n], 1);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN) {
				log(LL_DEBUG, "EINTR/EAGAIN %d", errno);
				continue;
			}

			log(LL_DEBUG, "error receiving request data");
			return;
		}
		else if (rc == 0)
		{
			request_data[h_n] = 0x00;
			break;
		}

		h_n += rc;
		request_data[h_n] = 0x00;
	}

	std::vector<std::string> *lines = split(request_data, "&");

	std::string username, password, return_url;

	for(auto l : *lines) {
		std::vector<std::string> *parts = split(l, "=");

		if (parts->at(0) == "user" && parts->size() == 2)
			username = un_url_escape(parts->at(1));
		else if (parts->at(0) == "password" && parts->size() == 2)
			password = un_url_escape(parts->at(1));
		else if (parts->at(0) == "return-url" && parts->size() == 2)
			return_url = parts->at(1);

		delete parts;
	}

	delete lines;

	std::string cookie;
	auto result = auth->authenticate(username, password);
	if (std::get<0>(result)) {
		std::string new_cookie = c->get_cookie(username);
		log(LL_INFO, "Auth OK for %s: %s", username.c_str(), new_cookie.c_str());

		cookie = myformat("Set-Cookie: auth=%s\r\n", new_cookie.c_str());
	}
	else {
		log(LL_WARNING, "HTTP authentication failed for %s: %s (do_auth)", username.c_str(), std::get<1>(result).c_str());
	}

	std::string reply;
	if (cookie.empty())
		reply = get_http_302_header("auth.html?reason=1");
	else {
		reply = get_http_200_header(cookie, "text/html") + get_html_header(true);
		reply += "<section><h2>sign in success!</h2>\n";

		if (return_url.empty() || return_url.find("do-auth") != std::string::npos || return_url.find("logout") != std::string::npos)
			reply += "<p>Click <a href=\"index.html\">here</a> to proceed to the menu.</p>\n";
		else
			reply += myformat("<p>Click <a href=\"%s\">here</a> to return where you came from.</p>\n", un_url_escape(return_url).c_str());

		reply += "</section>" + html_tail;
	}

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response");
}

void http_server::logout(http_thread_t *const ct, const std::string & username)
{
	c->delete_cookie_for_user(username);

	std::string cookie = "Set-Cookie: auth=none; expires=Thu, 01 Jan 1970 00:00:01 GMT;\r\n";

	std::string reply = get_http_200_header(cookie, "text/html") + get_html_header(true);
	reply += "<section><h2>logout success</h2>\n";
	reply += "<p>Click <a href=\"auth.html?reason=2\">here</a> to log back in.</p>\n";
	reply += "</section>" + html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::send_auth_html(http_thread_t *const ct, const std::string & cookie, const std::vector<std::string> & header_lines, const std::map<std::string, std::string> & pars)
{
	std::string reply = get_http_200_header(cookie, "text/html") + get_html_header(true);

	reply += "<section><h2>please sign in</h2>\n";

	auto int_it = pars.find("reason");
	if (int_it != pars.end()) {
		std::string reason = int_it->second;
		if (reason == "1")
			reply += "<p>Username or password are not recognized</p>";
		else if (reason == "2")
			reply += "<p>You have been logged out</p>";
		else if (reason == "3")
			reply += "<p>Session timed-out</p>";
		else
			reply += "<p>Something went wrong</p>";
	}

	reply += "<form action=\"do-auth\" method=\"post\">\n";
	reply += "<label>account\n";
	reply += "<input type=\"text\" name=\"user\" value="">\n";
	reply += "</label>\n";
	reply += "<label>password\n";
	reply += "<input type=\"password\" name=\"password\" value="">\n";
	reply += "</label>\n";
	reply += "<button type=\"submit\">Sign in</button>\n";

	auto referer = find_header(header_lines, "Referer");
	if (referer.has_value())
		reply += myformat("<input type=\"hidden\" name=\"return-url\" value=\"%s\">\n", referer.value().second.c_str());

	reply += "</form>\n";

	reply += "</section>" + html_tail;

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::send_redirect_auth_html(http_thread_t *const ct)
{
	std::string reply = get_http_302_header("auth.html?reason=3");

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::send_index_html(http_thread_t *const ct, const std::string & page_header, const std::string & iup, instance *const inst, source *const s, const std::string & username, const std::string & cookie)
{
	std::string reply = "???";

	if (inst) {
		std::string controls_msg = (s == nullptr || s->get_controls() == nullptr || s->get_controls()->has_controls() == false) ?  "in a window" : "with image controls";

		reply = get_http_200_header(cookie, "text/html") + page_header + "<section>"
			"<ul>" +
			myformat(""
					"<li><a href=\"stream.mjpeg%s\">MJPEG without controls</a>"
					"<li><a href=\"stream.html%s\">MJPEG %s</a>"
					"<li><a href=\"stream.ogg%s\">OGG (Theora) w/o controls</a>"
					"<li><a href=\"stream_ogg.html%s\">OGG (Theora) %s</a>"
					"<li><a href=\"stream.mpng%s\">MPNG stream</a>"
					"<li><a href=\"image.jpg%s\">Show snapshot (JPEG)</a>"
					"<li><a href=\"image.png%s\">Show snapshot (PNG)</a>", iup.c_str(), iup.c_str(), controls_msg.c_str(), iup.c_str(), iup.c_str(), controls_msg.c_str(), iup.c_str(), iup.c_str(), iup.c_str()
				);

		if (allow_admin) {
			reply += myformat("<li><a href=\"snapshot-img/%s\">Take a snapshot and store it on disk</a>"
					"<li><a href=\"snapshot-video/%s\">Start a video recording</a>", iup.c_str(), iup.c_str());
		}

		if (archive_acces)
			reply += "<li><a href=\"view-snapshots\">View recordings</a>";

		if (get_db()->using_db()) {
			auto dashboards = get_db()->get_dashboards(username);

			for(auto d : dashboards)
				reply += myformat("<li><a href=\"add-to-dashboard?int=%s&dashboard=%s\">Add to dashboard \"%s\"</a>", s ? url_escape(s->get_id()).c_str() : "", d.c_str(), d.c_str());
		}

		reply += "</ul></section>";

		auto err = find_errors(inst);
		if (err.has_value()) {
			reply += "<section><h2>problem</h2>\n";
			reply += "<p><strong>" + err.value().first + "</strong><br>";
			reply += err.value().second.msg + "</p>";
			reply += "</section>\n";
		}

		if (allow_admin) {
			const std::lock_guard<std::timed_mutex> lock(cfg->lock);

			for(interface *i : inst -> interfaces)
				reply += describe_interface(inst, i, false);
		}

		reply += html_tail; 
	}
	else {
		reply = get_http_200_header(cookie, "text/html") + page_header;

		reply += get_motd(motd_file);

		reply += "<section><h2>views</h2>\n";
		reply += "<ul>\n";

		if (!this->views || this->views->interfaces.empty())
			reply += "<li><strong>No view(s) defined</strong>\n";
		else {
			if (this->views) {
				for(interface *v : this->views->interfaces)  {
					if (v->get_class_type() != CT_VIEW)
						continue;

					view *cv = (view *)v;

					std::map<std::string, std::string> pars;
					if (cv->get_html(pars).empty())
						reply += myformat("<li><a href=\"view-menu?id=%s\">%s</a>\n", cv -> get_id().c_str(), cv -> get_id().c_str());
					else
						reply += myformat("<li><a href=\"view-view?id=%s\">%s</a>\n", cv -> get_id().c_str(), cv -> get_id().c_str());

					if (!v->get_descr().empty())
						reply += myformat("<br> (%s)\n", v->get_descr().c_str());
				}
			}
		}

		reply += "</ul></section>\n";

		if (get_db()->using_db()) {
			reply += "<section><h2>dashboard(s)</h2>\n";
			reply += "<ul>\n";

			auto dashboards = get_db()->get_dashboards(username);

			if (dashboards.empty())
				reply += "<li><strong>No dashboard(s) defined</strong>\n";
			else {
				for(auto d : dashboards)
					reply += myformat("<li><a href=\"dashboard?dashboard=%s\">%s</a>\n", url_escape(d).c_str(), d.c_str());
			}

			reply += "</ul>";

			reply += "<form action=\"add-dashboard\" method=\"get\"><label>Name <input type=\"text\" name=\"name\"></label><button type=\"submit\">Add</button></form>";

			reply += "</section>\n";
		}

		reply += "<section><h2>instances</h2><ul>";
		for(instance *inst_cur : cfg -> instances) {
			if (inst_cur -> name == "views")
				continue;

			reply += myformat("<li><a href=\"index.html?inst=%s\">%s</a>", url_escape(inst_cur -> name).c_str(), inst_cur -> name.c_str());

			if (!inst_cur->descr.empty())
				reply += myformat("<br> (%s)", inst_cur->descr.c_str());
		}
		reply += "</ul></section>";

		reply += "<section><h2>stored copy/paste buffers</h2><ul>";
		auto keys = get_meta()->get_bitmap_keys();
		for(auto key : keys)
			reply += myformat("<li><a href=\"copypaste?buffer=%s\">%s</a>", key.c_str(), key.c_str());
		if (keys.empty())
			reply += "<li><strong>No buffers to view</strong>";
		reply += "</ul></section>";

		std::string server_id = get_id();
		int g_bw = 0, g_cc = 0;
		calc_global_http_stats(cfg, &g_bw, &g_cc);

		reply += "<section><h2>statistics</h2>\n";
		reply += "<h3>Global</h3><dl>\n";
		reply += myformat("<dt>HTTP video bandwidth</dt><dd><strong id=\"gbw\">%d </strong> <strong>kB/s</strong></dd>", g_bw / 1024);
		reply += myformat("<dt>HTTP connection count</dt><dd><strong id=\"gcc\">%d</strong></dd>", g_cc);
		reply += myformat("<dt>Global CPU usage</dt><dd><strong id=\"gcpu\">%d</strong> <strong>%%</strong></dd>", g_cc);
		reply += "</dl><h3>This HTTP server</h3><dl>\n";
		reply += myformat("<dt>CPU usage</dt><dd><strong id=\"cpu-%s\">%.2f%%</strong></dd>", server_id.c_str(), get_cpu_usage() * 100.0);
		reply += myformat("<dt>Bandwidth usage</dt><dd><strong id=\"bw-%s\">%d</strong><strong>kB/s</strong></dd>", server_id.c_str(), get_bw() / 1024);
		reply += myformat("<dt>Connection count</dt><dd><strong id=\"cc-%s\">%ld</strong></dd>", server_id.c_str(), get_connection_count());
		reply += "</dl></section>";

		reply += get_websocket_js();

		if (!ws_privacy) {
			reply += "<section><h2>active stream connections</h2><ul id=\"peers\">";
			bool emit_static_list = true;

#if HAVE_WEBSOCKETPP == 1
			if (ws)
				emit_static_list = false;
#endif

			if (emit_static_list) {
				bool any = false;
				for(auto p : get_active_connections()) {
					reply += myformat("<li>%s", p.c_str());
					any = true;
				}
				if (!any)
					reply += "<li><strong>No active http connections</strong>";
			}

			reply += "</ul></section>";
		}

		reply += emit_stats_refresh_js(server_id, false, true, true, true);

		if (username.empty() == false) {
			reply += "<section><h2>user menu</h2>\n";
			reply += myformat("<ul><li><a href=\"logout\">logout <strong>%s</strong></a></li></ul>", username.c_str());
			reply += "</section>";
		}

		reply += html_tail; 
	}

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0)
		log(LL_DEBUG, "short write on response header");
}

void http_server::send_favicon_ico(http_thread_t *const ct, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "text/html") + "\r\n";

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, (const char *)favicon_ico, sizeof favicon_ico) <= 0)
		log(LL_DEBUG, "short write");
}

void http_server::send_icon_jpg(http_thread_t *const ct, const std::string & cookie)
{
	std::string reply = get_http_200_header(cookie, "image/jpeg");

	if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, (const char *)fierman_icon_jpg, sizeof fierman_icon_jpg) <= 0)
		log(LL_DEBUG, "short write");
}

std::optional<std::string> http_server::get_headers(http_thread_t *const ct)
{
	char request_headers[65536];
	int h_n = 0;

	struct pollfd fds[1] { { ct->hh.fd, POLLIN, 0 } };

	for(;!local_stop_flag && !motion_compatible;) {
		if (AVAILABLE_SSL(ct->hh) == 0 && poll(fds, 1, 250) == 0)
			continue;

		int rc = READ_SSL(ct->hh, &request_headers[h_n], 1);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN) {
				log(LL_INFO, "EINTR/EAGAIN %d", errno);
				continue;
			}

			log(LL_DEBUG, "error receiving request headers");
			return { };
		}
		else if (rc == 0)
		{
			log(LL_INFO, "error receiving request headers");
			return { };
		}

		h_n += rc;

		request_headers[h_n] = 0x00;

		if (strstr(request_headers, "\r\n\r\n") || strstr(request_headers, "\n\n"))
			break;
	}

	if (h_n == 0)
		return { };

	return std::string(request_headers, h_n);
}

void http_server::handle_http_client(http_thread_t *const ct)
{
	sigset_t all_sigs;
	sigfillset(&all_sigs);
	pthread_sigmask(SIG_BLOCK, &all_sigs, nullptr);

	ct->peer_name = get_endpoint_name(ct->hh.fd);

	std::string username;

	auto request_headers = get_headers(ct);
	if (!request_headers.has_value() && !motion_compatible) {
		CLOSE_SSL(ct->hh);
		return;
	}

	std::vector<std::string> *header_lines = nullptr;
	if (!motion_compatible)
		header_lines = split(request_headers.value(), "\r\n");

	if (!motion_compatible && header_lines->empty()) {
		delete header_lines;
		CLOSE_SSL(ct->hh);
		return;
	}

	std::vector<std::string> *request_parts = nullptr;
	if (header_lines)
		request_parts = split(header_lines->at(0), " ");

	bool get_or_post = true;
	if (motion_compatible || request_parts->size() != 3) {
		// motion compatible means: just start sending immediately; motion
		// also doesn't (maybe current version does) wait for headers
	}
	else if (request_parts->at(0) == "GET" || request_parts->at(0) == "POST")
		get_or_post = true;
	else if (request_parts->at(0) == "HEAD")
		get_or_post = false;
	else {
		delete request_parts;
		delete header_lines;
		CLOSE_SSL(ct->hh);
		return;
	}

	std::string unescaped_path = motion_compatible || request_parts->size() != 3 ? "/" : un_url_escape(request_parts->at(1));

	std::string path = unescaped_path;
	if (!motion_compatible) {
		while(path.empty() == false && path.at(0) == '/')
			path = path.substr(1);
	}

	std::map<std::string, std::string> pars;

	instance *inst = limit_to;
	source *s = nullptr;
	bool auth_ok = true, is_view_proxy = false;
	std::string cookie;

	int    final_w   = resize_w;
	int    final_h   = resize_h;
	double final_fps = fps;
	bool   acc_fps   = false;

	if (!motion_compatible) {
		auto forwarded_for = find_header(*header_lines, "X-Forwarded-For");

		std::string ff;
		if (forwarded_for.has_value())
			ff = ct->peer_name = forwarded_for.value().second;

		if (auth)
			authorize(ct, *header_lines, &auth_ok, &username, &cookie);

		size_t q = path.find('?');
		if (q != std::string::npos) {
			std::string query_pars = path.substr(q + 1);
			path = path.substr(0, q);

			pars = find_pars(query_pars.c_str());

			auto int_it = pars.find("int");
			if (int_it != pars.end()) {
				find_by_id(cfg, int_it->second, &inst, (interface **)&s);

				if (!s)
					s = find_view(this->views, int_it->second);
			}

			if (auto inst_it = pars.find("inst"); inst_it != pars.end())
				inst = find_instance_by_name(cfg, inst_it -> second);

			if (auto w_it = pars.find("w"); w_it != pars.end()) {
				final_w = atoi(w_it -> second.c_str());

				if (final_w > resize_w && resize_w != -1)
					final_w = resize_w;
			}

			if (auto h_it = pars.find("h"); h_it != pars.end()) {
				final_h = atoi(h_it -> second.c_str());

				if (final_h > resize_h && resize_h != -1)
					final_h = resize_h;
			}

			if (auto fps_it = pars.find("fps"); fps_it != pars.end()) {
				final_fps = atof(fps_it -> second.c_str());

				if (final_fps > fps && fps > 0.0)
					final_fps = fps;
			}

			acc_fps = pars.find("acc-fps") != pars.end();

			if (!s)
				s = find_source(inst);
		}
	}

	std::string id = s ? s->get_id() : "";

	std::string endpoint_name = get_endpoint_name(ct->hh.fd);
	if (endpoint_name != ct->peer_name)
		log(id, LL_INFO, "HTTP(s) request by %s (%s): %s", endpoint_name.c_str(), ct->peer_name.c_str(), unescaped_path.c_str());
	else
		log(id, LL_INFO, "HTTP(s) request by %s: %s", endpoint_name.c_str(), unescaped_path.c_str());

	if (s == nullptr && motion_compatible)
		s = motion_compatible;

	if (s)
		s->start();

	auto vp_it = pars.find("view-proxy"); // interface
	is_view_proxy = vp_it != pars.end();

	// iup = instance url parameter
	std::string iup;

	if (is_view_proxy && s)
		iup = myformat("?int=%s", url_escape(s->get_id()).c_str());
	else if (inst)
		iup = myformat("?inst=%s", url_escape(inst->name).c_str());

	const std::string page_header = myformat(get_html_header(true).c_str(), inst ? (" / " + inst -> name).c_str() : "");
	const std::string page_header_no_col = myformat(get_html_header(false).c_str(), inst ? (" / " + inst -> name).c_str() : "");

	if (path.find("stylesheet.css") != std::string::npos)
		send_stylesheet(ct, cookie);
	else if (path == "favicon.ico")
		send_favicon_ico(ct, cookie);
	else if (path == "icon.jpg")
		send_icon_jpg(ct, cookie);
	else if (path == "auth.html")
		send_auth_html(ct, cookie, *header_lines, pars);
	else if (path == "do-auth" && request_parts && request_parts->at(0) == "POST" && auth)
		do_auth(ct, *header_lines);
	else if (!auth_ok)
		send_redirect_auth_html(ct);
	else if (path == "logout")
		logout(ct, username);
	else if ((path == "stream.mjpeg" || motion_compatible) && s) {
		ct->is_stream = true;

		register_peer(true, ct->peer_name);

		send_mjpeg_stream(ct->hh, s, final_fps, quality, get_or_post, time_limit, filters, cfg->r, final_w, final_h, cfg, is_view_proxy, handle_failure, &ct->st, cookie, acc_fps);

		register_peer(false, ct->peer_name);
	}
	else if (path == "stream.ogg" && s) {
		ct->is_stream = true;

		register_peer(true, ct->peer_name);

		send_theora_stream(ct->hh, s, final_fps, quality, get_or_post, time_limit, filters, cfg->r, final_w, final_h, cfg, is_view_proxy, handle_failure, &ct->st, cookie, acc_fps);

		register_peer(false, ct->peer_name);
	}
	else if (path == "stream.mpng" && s) {
		ct->is_stream = true;

		register_peer(true, ct->peer_name);

		send_mpng_stream(ct->hh, s, final_fps, get_or_post, time_limit, filters, cfg->r, final_w, final_h, cfg, is_view_proxy, handle_failure, &ct->st, cookie, acc_fps);

		register_peer(false, ct->peer_name);
	}
	else if (path == "image.png" && s)
		send_png_frame(ct->hh, s, get_or_post, filters, cfg->r, final_w, final_h, cfg, is_view_proxy, handle_failure, &ct->st, cookie);
	else if (path == "image.jpg" && s)
		send_jpg_frame(ct->hh, s, get_or_post, quality, filters, cfg->r, final_w, final_h, cfg, is_view_proxy, handle_failure, &ct->st, cookie);
	else if (path == "stream.html" && s)
		send_stream_html(ct, page_header, iup, s, is_view_proxy, cookie, false);
	else if (path == "stream_ogg.html" && s)
		send_stream_html(ct, page_header, iup, s, is_view_proxy, cookie, true);
	else if (path == "index.html" || path.empty())
		send_index_html(ct, page_header, iup, inst, s, username, cookie);
	else if (path == "copypaste" && (archive_acces || allow_admin))
		send_copypaste(ct, pars, cookie);
	else if (path == "fs-db.html")
		send_fs_db_html(ct, username, pars, cookie);
	else if (path == "fs-db.mjpeg")
		send_fs_db_mjpeg(ct, username, pars, cookie, quality);
	else if (path == "dashboard")
		send_dashboard(ct, username, pars, cookie);
	else if (path == "add-dashboard")
		add_dashboard(ct, username, pars);
	else if (path == "add-to-dashboard")
		add_to_dashboard(ct, username, pars);
	else if (path == "remove-from-dashboard")
		remove_from_dashboard(ct, username, pars);
	else if (path == "del-dashboard")
		remove_dashboard(ct, username, pars);
        else if (path == "gstats")
                send_gstats(ct, cookie);
	else if (path == "stats")
		send_stats(ct, pars, cookie);
	else if ((path == "view-snapshots/send-file" || path == "send-file") && (archive_acces || allow_admin))
		send_snapshot(ct, pars, cookie, false);
	else if (path == "send-file-db" && (archive_acces || allow_admin))
		send_snapshot(ct, pars, cookie, true);
	else if (path == "view-view" && this->views)
		view_view(ct, pars, cookie);
	else if (path == "view-menu" && this->views)
		view_menu(ct, pars, cookie, username);
	else if ((path == "view-snapshots/view-file" || path == "view-file") && (archive_acces || allow_admin))
		view_file(ct, pars, page_header, cookie);
	else if ((path == "view-snapshots" || path == "view-snapshots/") && (archive_acces || allow_admin)) {
#if HAVE_MYSQL == 1
		if (get_db()->using_db())
			file_menu_db(ct, pars, cookie);
		else
			file_menu(ct, pars, page_header_no_col, cookie);
#else
		file_menu(ct, pars, page_header_no_col, cookie);
#endif
	}
	else if (path == "images/play.svg") {
		std::string reply = get_http_200_header(cookie, "image/svg+xml");

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, (const char *)play_svg, sizeof play_svg) <= 0)
			log(LL_DEBUG, "short write");
	}
	else if (path == "images/stop.svg") {
		std::string reply = get_http_200_header(cookie, "image/svg+xml");

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, (const char *)stop_svg, sizeof stop_svg) <= 0)
			log(LL_DEBUG, "short write");
	}
	else if (path == "images/pause.svg") {
		std::string reply = get_http_200_header(cookie, "image/svg+xml");

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, (const char *)pause_svg, sizeof pause_svg) <= 0)
			log(LL_DEBUG, "short write");
	}
	else if (path == "images/fullscreen.svg") {
		std::string reply = get_http_200_header(cookie, "image/svg+xml");

		std::string content = "<svg xmlns=\"http://www.w3.org/2000/svg\" height=\"48\" width=\"48\" stroke=\"#2c6e2c\" fill=\"none\"><path d=\"M 10 22 v -12 h 12   M 38 22 v -12 h -12  M 38 26 v 12 h -12  M 10 26 v 12 h 12\" stroke-linejoin=\"round\" stroke-width=\"10\"/></svg>";

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, content.c_str(), content.size()) <= 0)
			log(LL_DEBUG, "short write");
	}
	else if (path == "images/trash.svg") {
		std::string reply = get_http_200_header(cookie, "image/svg+xml");

		// https://freesvg.org/trash-can-icon
		std::string content = "<svg xmlns=\"http://www.w3.org/2000/svg\" height=\"48\" width=\"42\" stroke=\"#2c6e2c\"><path d=\"M0 281.296l0 -68.355q1.953 -37.107 29.295 -62.496t64.449 -25.389l93.744 0l0 -31.248q0 -39.06 27.342 -66.402t66.402 -27.342l312.48 0q39.06 0 66.402 27.342t27.342 66.402l0 31.248l93.744 0q37.107 0 64.449 25.389t29.295 62.496l0 68.355q0 25.389 -18.553 43.943t-43.943 18.553l0 531.216q0 52.731 -36.13 88.862t-88.862 36.13l-499.968 0q-52.731 0 -88.862 -36.13t-36.13 -88.862l0 -531.216q-25.389 0 -43.943 -18.553t-18.553 -43.943zm62.496 0l749.952 0l0 -62.496q0 -13.671 -8.789 -22.46t-22.46 -8.789l-687.456 0q-13.671 0 -22.46 8.789t-8.789 22.46l0 62.496zm62.496 593.712q0 25.389 18.553 43.943t43.943 18.553l499.968 0q25.389 0 43.943 -18.553t18.553 -43.943l0 -531.216l-624.96 0l0 531.216zm62.496 -31.248l0 -406.224q0 -13.671 8.789 -22.46t22.46 -8.789l62.496 0q13.671 0 22.46 8.789t8.789 22.46l0 406.224q0 13.671 -8.789 22.46t-22.46 8.789l-62.496 0q-13.671 0 -22.46 -8.789t-8.789 -22.46zm31.248 0l62.496 0l0 -406.224l-62.496 0l0 406.224zm31.248 -718.704l374.976 0l0 -31.248q0 -13.671 -8.789 -22.46t-22.46 -8.789l-312.48 0q-13.671 0 -22.46 8.789t-8.789 22.46l0 31.248zm124.992 718.704l0 -406.224q0 -13.671 8.789 -22.46t22.46 -8.789l62.496 0q13.671 0 22.46 8.789t8.789 22.46l0 406.224q0 13.671 -8.789 22.46t-22.46 8.789l-62.496 0q-13.671 0 -22.46 -8.789t-8.789 -22.46zm31.248 0l62.496 0l0 -406.224l-62.496 0l0 406.224zm156.24 0l0 -406.224q0 -13.671 8.789 -22.46t22.46 -8.789l62.496 0q13.671 0 22.46 8.789t8.789 22.46l0 406.224q0 13.671 -8.789 22.46t-22.46 8.789l-62.496 0q-13.671 0 -22.46 -8.789t-8.789 -22.46zm31.248 0l62.496 0l0 -406.224l-62.496 0l0 406.224z\"/></svg>";

		if (WRITE_SSL(ct->hh, reply.c_str(), reply.size()) <= 0 || WRITE_SSL(ct->hh, content.c_str(), content.size()) <= 0)
			log(LL_DEBUG, "short write");
	}
	else if (is_rest && path.substr(0, 5) == "rest/") {
		log(LL_DEBUG, "REST: %s", &path[5]);
		run_rest(ct->hh, cfg, &path[5], pars, snapshot_dir, quality);
	}
	else if (!allow_admin) {
		goto do404;
	}
	else if (path == "pause" || path == "unpause" || path == "toggle-pause")
		pause_module(ct, pars, page_header, inst, path, cookie);
	else if (path == "start" || path == "stop" || path == "toggle-start-stop")
		start_stop_module(ct, pars, page_header, inst, path, cookie);
	else if (path == "restart")
		restart_module(ct, pars, page_header, inst, cookie);
	else if (path == "snapshot-img/")
		take_snapshot(ct, pars, page_header, s, cookie);
	else if (path == "snapshot-video/")
		video_snapshot(ct, pars, page_header, inst, s, is_view_proxy, cookie);
	else {
do404:
		send_404(ct, path, pars, cookie);
	}

	CLOSE_SSL(ct->hh);

	delete request_parts;
	delete header_lines;

	if (s)
		s -> stop();
}

void http_server::handle_http_client_thread(http_thread_t *const ct)
{
	set_thread_name("http_client");

	try {
#if HAVE_OPENSSL == 1
		if (ct -> hh.sh)
			ACCEPT_SSL(ct->hh);
#endif

		handle_http_client(ct);
	}
	catch(const std::runtime_error & re) {
		log(LL_ERR, "Runtime error for %s: %s", ct->peer_name.c_str(), re.what());
	}
	catch(const std::exception & ex) {
		log(LL_ERR, "std::exception for %s: %s", ct->peer_name.c_str(), ex.what());
	}
	catch(...) {
		log(LL_ERR, "Exception in handle_http_client_thread for %s", ct->peer_name.c_str());
	}

	ct->is_terminated = true;
}

void http_server::register_trigger_notifier(instance *const inst, const bool is_register, http_server *const me)
{
	std::vector<interface *> triggers = find_motion_triggers(inst);

	for(auto i : triggers) {
		if (is_register)
			((motion_trigger *)i)->register_notifier(me);
		else
			((motion_trigger *)i)->unregister_notifier(me);
	}
}

http_server::http_server(configuration_t *const cfg, http_auth *const auth, instance *const limit_to, const std::string & id, const std::string & descr, const listen_adapter_t & la, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, const int resize_w, const int resize_h, source *const motion_compatible, const bool allow_admin, const bool archive_acces, const std::string & snapshot_dir, const bool with_subdirs, const bool is_rest, instance  *const views, const bool handle_failure, const ssl_pars_t *const sp, const std::string & stylesheet, const int websocket_port, const std::string & websocket_url, const int max_cookie_age, const std::string & motd_file, const bool ws_privacy, const std::string & notify_viewer_script) : cfg(cfg), la(la), auth(auth), interface(id, descr), fps(fps), quality(quality), time_limit(time_limit), filters(f), resize_w(resize_w), resize_h(resize_h), motion_compatible(motion_compatible), allow_admin(allow_admin), archive_acces(archive_acces), snapshot_dir(snapshot_dir), with_subdirs(with_subdirs), is_rest(is_rest), views(views), limit_to(limit_to), handle_failure(handle_failure), stylesheet(stylesheet), websocket_url(websocket_url), max_cookie_age(max_cookie_age), motd_file(motd_file), ws_privacy(ws_privacy), notify_viewer_script(notify_viewer_script)
{
	ct = CT_HTTPSERVER;

	fd = start_listen(la);

	c = new http_cookies();

#if HAVE_WEBSOCKETPP == 1
	if (websocket_port != -1) {
		if (sp)
			ws = new ws_server_tls(cfg, id + "-wss", "websockets server (TLS)", { la.adapter, websocket_port, SOMAXCONN, false }, this, *sp, auth != nullptr, c);
		else
			ws = new ws_server(cfg, id + "-ws", "websockets server", { la.adapter, websocket_port, SOMAXCONN, false }, this, auth != nullptr, c);

		ws->start();

		if (limit_to)
			register_trigger_notifier(limit_to, true, this);
		else {
			for(auto i : cfg->instances)
				register_trigger_notifier(i, true, this);
		}
	}
#endif

#if HAVE_OPENSSL == 1
	if (sp) {
		this->sp = *sp;

		ssl_ctx = create_context(this->sp);
	}
#endif
}

http_server::~http_server()
{
	close(fd);

	free_filters(filters);

#if HAVE_OPENSSL == 1
	if (ssl_ctx)
		SSL_CTX_free(ssl_ctx);
#endif

	if (limit_to)
		register_trigger_notifier(limit_to, false, this);
	else {
		for(auto i : cfg->instances)
			register_trigger_notifier(i, false, this);
	}

#if HAVE_WEBSOCKETPP == 1
	if (ws) {
		ws->ws_stop();

		delete ws;
	}
#endif

	delete c;
}

std::pair<std::string, std::string> http_server::gen_video_url(instance *const i)
{
	source *s = find_source(i);

	if (!s)
		s = find_view(i);

	std::string base_url;
#if HAVE_OPENSSL == 1
	if (ssl_ctx)
		base_url = myformat("https://%s:%d", la.adapter.c_str(), la.port);
	else
#endif
	base_url = myformat("http://%s:%d", la.adapter.c_str(), la.port);

	return { s->get_descr() + " / video", myformat("%s/stream.mjpeg?inst=%s", base_url.c_str(), url_escape(i->name).c_str()) };
}

std::vector<std::pair<std::string, std::string> > http_server::get_motion_video_urls()
{
	std::vector<std::pair<std::string, std::string> > out;

	if (limit_to)
		out.push_back(gen_video_url(limit_to));
	else {
		for(auto i : cfg->instances)
			out.push_back(gen_video_url(i));
	}

	return out;
}

std::pair<std::string, std::string> http_server::gen_still_images_url(instance *const i)
{
	source *s = find_source(i);

	if (!s)
		s = find_view(i);

	std::string base_url;
#if HAVE_OPENSSL == 1
	if (ssl_ctx)
		base_url = myformat("https://%s:%d", la.adapter.c_str(), la.port);
	else
#endif
	base_url = myformat("http://%s:%d", la.adapter.c_str(), la.port);

	return { s->get_descr() + " / snapshot", myformat("%s/image.jpg?inst=%s", base_url.c_str(), url_escape(i->name).c_str()) };
}

std::vector<std::pair<std::string, std::string> > http_server::get_motion_still_images_urls()
{
	std::vector<std::pair<std::string, std::string> > out;

	if (limit_to)
		out.push_back(gen_still_images_url(limit_to));
	else {
		for(auto i : cfg->instances)
			out.push_back(gen_still_images_url(i));
	}

	return out;
}

std::string http_server::get_base_url()
{
#if HAVE_OPENSSL == 1
	if (ssl_ctx)
		return myformat("https://%s:%d", la.adapter.c_str(), la.port);
#endif

	return myformat("http://%s:%d", la.adapter.c_str(), la.port);
}

void http_server::purge_threads()
{
	const std::lock_guard<std::mutex> lock(data_lock);

	for(size_t i=0; i<data.size();) {
		if (data.at(i)->is_terminated) {
			data.at(i)->th->join();

			delete data.at(i);
			data.erase(data.begin() + i);
		}
		else {
			i++;
		}
	}
}

void http_server::operator()()
{
	set_thread_name("http_server");

	struct pollfd fds[] { { fd, POLLIN, 0 } };

	for(;!local_stop_flag;) {
		purge_threads();

		get_meta() -> set_int("$http-viewers$", std::pair<uint64_t, int>(0, data.size()));

		pauseCheck();

		if (poll(fds, 1, 500) == 0)
			continue;

		int cfd = accept(fd, nullptr, 0);
		if (cfd == -1)
			continue;

#if HAVE_OPENSSL == 1
		if (ssl_ctx)
			log(id, LL_DEBUG, "HTTPS connected with: %s", get_endpoint_name(cfd).c_str());
		else
#endif
		log(id, LL_DEBUG, "HTTP connected with: %s", get_endpoint_name(cfd).c_str());

		http_thread_t *ct = new http_thread_t();
		ct->st.start();

#if HAVE_OPENSSL == 1
		if (ssl_ctx) {
			ct -> hh.sh = SSL_new(ssl_ctx);
			SSL_set_fd(ct->hh.sh, cfd);
		}
#endif
		ct -> hh.fd = cfd;

		ct->base_url = get_base_url();
#if HAVE_OPENSSL == 1
		if (ssl_ctx)
			ct->base_url = myformat("https://%s:%d/", la.adapter.c_str(), la.port);
		else
#endif
		ct->base_url = myformat("http://%s:%d/", la.adapter.c_str(), la.port);

		ct -> is_terminated = false;

		st->track_cpu_usage();

		ct -> th = new std::thread(&http_server::handle_http_client_thread, this, ct);

		if (ct -> th == nullptr) {
			log(id, LL_ERR, "new std::thread failed (http server): %s", strerror(errno));
			CLOSE_SSL(ct->hh);
			delete ct;

			const std::lock_guard<std::mutex> lock(data_lock);

			for(auto d : data)
				log(id, LL_INFO, "Still connected with %s", d->peer_name.c_str());
		}
		else {
			const std::lock_guard<std::mutex> lock(data_lock);

			data.push_back(ct);
		}

		c->clean_cookies(max_cookie_age);
	}

	for(auto d : data)
		d->th->join();

	for(auto d : data)
		delete d;

	log(id, LL_INFO, "HTTP server thread terminating");
}

double http_server::get_cpu_usage() const
{
	double total = st->get_cpu_usage();

	const std::lock_guard<std::mutex> lock(data_lock);

	for(auto d : data)
		total += d->st.get_cpu_usage();

	return total;
}

int http_server::get_bw() const
{
	int total = st->get_bw();

	const std::lock_guard<std::mutex> lock(data_lock);

	for(auto d : data)
		total += d->st.get_bw();

	return total;
}

size_t http_server::get_connection_count() const
{
	size_t n = 0;

	const std::lock_guard<std::mutex> lock(data_lock);

	for(auto d : data)
		n += d->is_stream;

	return n;
}

std::vector<std::string> http_server::get_active_connections() const
{
	std::vector<std::string> out;

	const std::lock_guard<std::mutex> lock(data_lock);

	for(auto d : data) {
		if (d->is_stream)
			out.push_back(d->peer_name);
	}

	return out;
}

void http_server::publish_motion_detected(void *ws, const std::string & subject)
{
#if HAVE_WEBSOCKETPP == 1
	if (ws) {
		ws_server *wsp = (ws_server *)ws;

		json_t *json = json_object();

		json_object_set_new(json, "type", json_string("motion-detected"));

		json_object_set_new(json, "id", json_string(subject.c_str()));

		char *js = json_dumps(json, JSON_COMPACT);
		std::string msg = js;
		free(js);

		json_decref(json);

		wsp->push(msg, "");
	}
#endif
}

void http_server::notify_thread_of_event(const std::string & subject)
{
	interface::notify_thread_of_event(subject);

	publish_motion_detected(ws, subject);

	view *v = find_view_by_child(cfg, subject);

	if (v)
		publish_motion_detected(ws, v->get_id());
}
