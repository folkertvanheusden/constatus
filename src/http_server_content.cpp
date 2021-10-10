// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <atomic>
#include <stdio.h>
#include <string>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>

#include "source.h"
#include "filter.h"
#include "utils.h"
#include "log.h"
#include "view.h"
#include "picio.h"
#include "error.h"
#include "webservices.h"
#include "http_server.h"
#include "http_utils.h"
#include "resize.h"
#include "db.h"
#include "cfg.h"
#include "view_all.h"

void http_server::send_mjpeg_stream(h_handle_t & hh, source *s, double fps, int quality, bool get, int time_limit, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie)
{
	bool first = true;

	video_frame *prev_frame = nullptr;

	bool sc = resize_h != -1 || resize_w != -1;
	bool nf = filters == nullptr || filters -> empty();

	uint64_t prev = 0;
	time_t end = time(nullptr) + time_limit;
	for(;(time_limit <= 0 || time(nullptr) < end) && !local_stop_flag;) {
		uint64_t before_ts = get_us();

		video_frame *pvf = s -> get_frame(handle_failure, prev);

		if (pvf) {
			prev = pvf->get_ts();

			// send header
			constexpr char term[] = "\r\n";

			if (first) {
				first = false;

				std::string reply_headers = myformat(
					"HTTP/1.0 200 OK\r\n"
					"Cache-Control: no-cache\r\n"
					"Pragma: no-cache\r\n"
					"Server: " NAME " " VERSION "\r\n"
					"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
					"Last-Modified: %s\r\n"
					"Date: %s\r\n"
					"Connection: close\r\n"
					"%s"
					"Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
					"\r\n", date_header(prev).c_str(), date_header(0).c_str(), cookie.c_str());

				if (WRITE_SSL(hh, reply_headers.c_str(), reply_headers.size()) <= 0) {
					log(LL_DEBUG, "short write on response header");
					delete pvf;
					break;
				}

				if (!get) {
					delete pvf;
					break;
				}
			}
			else if (WRITE_SSL(hh, term, strlen(term)) <= 0)
			{
				log(LL_DEBUG, "short write on terminating cr/lf: %s", strerror(errno));
				delete pvf;
				break;
			}

			st->track_bw(2);

			// decode, encode, etc frame
			if (nf && !sc) {
				auto rc = pvf->get_data_and_len(E_JPEG);

				char img_h[4096] = { 0 };
				int header_len = snprintf(img_h, sizeof img_h, 
					"--myboundary\r\n"
					"Content-Type: image/jpeg\r\n"
					"Content-Length: %zu\r\n"
					"\r\n", std::get<1>(rc));

				if (WRITE_SSL(hh, img_h, header_len) <= 0)
				{
					log(LL_DEBUG, "short write on boundary header: %s", strerror(errno));
					delete pvf;
					break;
				}

				if (WRITE_SSL(hh, (const char *)std::get<0>(rc), std::get<1>(rc)) <= 0)
				{
					log(LL_DEBUG, "short write on img data: %s", strerror(errno));
					delete pvf;
					break;
				}

				st->track_bw(header_len + std::get<1>(rc));

				delete pvf;
			}
			else {
				if (resize_h != -1 || resize_w != -1) {
					// printf("%dx%d => %dx%d\n", pvf->get_w(), pvf->get_h(), resize_w, resize_h);
					video_frame *temp = pvf->do_resize(r, resize_w, resize_h);
					delete pvf;
					pvf = temp;
				}

				if (filters && !filters->empty()) {
					source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
					instance *inst = find_instance_by_interface(cfg, cur_s);

					video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
					delete pvf;
					pvf = temp;
				}

				delete prev_frame;
				prev_frame = pvf;

				auto rc = pvf->get_data_and_len(E_JPEG);

				char img_h[4096] = { 0 };
				int header_len = snprintf(img_h, sizeof img_h, 
					"--myboundary\r\n"
					"Content-Type: image/jpeg\r\n"
					"Content-Length: %zu\r\n"
					"\r\n", std::get<1>(rc));

				if (WRITE_SSL(hh, img_h, header_len) <= 0) {
					log(LL_INFO, "short write on boundary header: %s", strerror(errno));
					break;
				}

				if (WRITE_SSL(hh, (const char *)std::get<0>(rc), std::get<1>(rc)) <= 0) {
					log(LL_INFO, "short write on img data: %s", strerror(errno));
					break;
				}

				st->track_bw(header_len + std::get<1>(rc));
			}
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;
}

void http_server::send_mpng_stream(h_handle_t & hh, source *s, double fps, bool get, const int time_limit, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie)
{
	bool first = true;

	video_frame *prev_frame = nullptr;

	uint64_t prev = 0;
	time_t end = time(nullptr) + time_limit;
	for(;(time_limit <= 0 || time(nullptr) < end) && !local_stop_flag;) {
		uint64_t before_ts = get_us();

		video_frame *pvf = s -> get_frame(handle_failure, prev);

		if (pvf) {
			source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
			instance *inst = find_instance_by_interface(cfg, cur_s);

			prev = pvf->get_ts();

			if (resize_h != -1 || resize_w != -1) {
				video_frame *temp = pvf->do_resize(r, resize_w, resize_h);
				delete pvf;
				pvf = temp;
			}

			if (filters && !filters->empty()) {
				video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;
			}

			char *data_out = nullptr;
			size_t data_out_len = 0;
			FILE *fh = open_memstream(&data_out, &data_out_len);
			if (!fh) {
				delete pvf;
				log(LL_ERR, "open_memstream returns failure");
				break;
			}

			write_PNG_file(fh, pvf->get_w(), pvf->get_h(), pvf->get_data(E_RGB));

			fclose(fh);

			delete prev_frame;
			prev_frame = pvf;

			// send
			constexpr char term[] = "\r\n";

			if (first) {
				first = false;

				std::string reply_headers = myformat(
					"HTTP/1.0 200 OK\r\n"
					"Cache-Control: no-cache\r\n"
					"Pragma: no-cache\r\n"
					"Server: " NAME " " VERSION "\r\n"
					"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
					"Last-Modified: %s\r\n"
					"Date: %s\r\n"
					"Connection: close\r\n"
					"%s"
					"Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
					"\r\n", date_header(prev).c_str(), date_header(0).c_str(), cookie.c_str());

				if (WRITE_SSL(hh, reply_headers.c_str(), reply_headers.size()) <= 0) {
					log(LL_DEBUG, "short write on response header");
					free(data_out);
					break;
				}

				if (!get) {
					free(data_out);
					break;
				}
			}
			else if (WRITE_SSL(hh, term, strlen(term)) <= 0) {
				log(LL_DEBUG, "short write on terminating cr/lf");
				free(data_out);
				break;
			}

			char img_h[4096] = { 0 };
			snprintf(img_h, sizeof img_h, 
					"--myboundary\r\n"
					"Content-Type: image/png\r\n"
					"Content-Length: %d\r\n"
					"\r\n", (int)data_out_len);

			if (WRITE_SSL(hh, img_h, strlen(img_h)) <= 0) {
				log(LL_DEBUG, "short write on boundary header");
				free(data_out);
				break;
			}

			if (WRITE_SSL(hh, data_out, data_out_len) <= 0) {
				log(LL_DEBUG, "short write on img data");
				free(data_out);
				break;
			}

			st->track_bw(strlen(img_h) + data_out_len);

			free(data_out);
			data_out = nullptr;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;
}

void http_server::send_png_frame(h_handle_t & hh, source *s, bool get, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie)
{
	std::string reply_headers = myformat(
		"HTTP/1.0 200 OK\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
		"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
		"Last-Modified: %s\r\n"
		"Date: %s\r\n"
		"Connection: close\r\n"
		"Content-Type: image/png\r\n"
		"%s"
		"\r\n", date_header(s->get_current_ts()).c_str(), date_header(0).c_str(), cookie.c_str());

	if (WRITE_SSL(hh, reply_headers.c_str(), reply_headers.size()) <= 0) {
		log(LL_DEBUG, "short write on response header");
		CLOSE_SSL(hh);
		return;
	}

	st->track_bw(reply_headers.size());

	if (!get) {
		CLOSE_SSL(hh);
		return;
	}

	set_no_delay(hh.fd, true);

	video_frame * pvf = s -> get_frame(handle_failure, get_us());

	if (pvf) {
		source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
		instance *inst = find_instance_by_interface(cfg, cur_s);

		if (resize_h != -1 || resize_w != -1) {
			video_frame *temp = pvf->do_resize(r, resize_w, resize_h);
			delete pvf;
			pvf = temp;
		}

		if (filters && !filters->empty()) {
			video_frame *temp = pvf->apply_filtering(inst, s, nullptr, filters, nullptr);
			delete pvf;
			pvf = temp;
		}

		char *data_out = nullptr;
		size_t data_out_len = 0;

		FILE *fh = open_memstream(&data_out, &data_out_len);
		if (!fh)
			error_exit(true, "open_memstream() failed");

		write_PNG_file(fh, pvf->get_w(), pvf->get_h(), pvf->get_data(E_RGB));

		delete pvf;

		fclose(fh);

		if (WRITE_SSL(hh, data_out, data_out_len) <= 0)
			log(LL_DEBUG, "short write on img data");

		st->track_bw(data_out_len);

		free(data_out);
	}
}

void http_server::send_jpg_frame(h_handle_t & hh, source *s, bool get, int quality, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie)
{
	std::string reply_headers = myformat(
		"HTTP/1.0 200 OK\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
		"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
		"Last-Modified: %s\r\n"
		"Date: %s\r\n"
		"Connection: close\r\n"
		"Content-Type: image/jpeg\r\n"
		"%s"
		"\r\n", date_header(s->get_current_ts()).c_str(), date_header(0).c_str(), cookie.c_str());

	if (WRITE_SSL(hh, reply_headers.c_str(), reply_headers.size()) <= 0) {
		log(LL_DEBUG, "short write on response header");
		CLOSE_SSL(hh);
		return;
	}

	st->track_bw(reply_headers.size());

	if (!get) {
		CLOSE_SSL(hh);
		return;
	}

	set_no_delay(hh.fd, true);

	video_frame *pvf = s -> get_frame(handle_failure, get_us());

	if (pvf) {
		source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
		instance *inst = find_instance_by_interface(cfg, cur_s);

		if (resize_h != -1 || resize_w != -1) {
			video_frame *temp = pvf->do_resize(r, resize_w, resize_h);
			delete pvf;
			pvf = temp;
		}

		if (filters && !filters->empty()) {
			video_frame *temp = pvf->apply_filtering(inst, s, nullptr, filters, nullptr);
			delete pvf;
			pvf = temp;
		}

		auto img = pvf->get_data_and_len(E_JPEG);

		if (WRITE_SSL(hh, (const char *)std::get<0>(img), std::get<1>(img)) <= 0)
			log(LL_DEBUG, "short write on img data");
		else
			st->track_bw(std::get<1>(img));

		delete pvf;
	}
}

bool http_server::send_file(h_handle_t & hh, const std::string & path, const char *const name, const bool dl, stats_tracker *const st, const std::string & cookie, const bool path_is_valid)
{
	std::string complete_path = path_is_valid ? name : (path + "/" + name);

	std::string type = "text/html";
	if (complete_path.size() >= 3) {
		std::string ext = complete_path.substr(complete_path.size() - 3);

		if (ext == "avi")
			type = "video/x-msvideo";
		else if (ext == "mp4")
			type = "video/mp4";
		else if (ext == "png")
			type = "image/png";
		else if (ext == "jpg")
			type = "image/jpeg";
		else if (ext == "css")
			type = "text/css";
		else if (ext == "flv")
			type = "video/x-flv";
		else if (ext == "ico")
			type = "image/x-icon";
		else if (ext == "ebm")
			type = "video/webm";
		else if (ext == "css")
			type = "text/css";
		else if (ext == "ogg" || ext == "ogv" || ext == "ogm")
			type = "video/ogg";
	}

	log(LL_INFO, "Sending file %s of type %s", complete_path.c_str(), type.c_str());

	FILE *fh = fopen(complete_path.c_str(), "r");
	if (!fh) {
		log(LL_WARNING, "Cannot access %s: %s", complete_path.c_str(), strerror(errno));

		std::string headers = "HTTP/1.0 404 na\r\nServer: " NAME " " VERSION "\r\n\r\n";

		(void)WRITE_SSL(hh, headers.c_str(), headers.size());

		return false;
	}

	struct stat sb;
	if (fstat(fileno(fh), &sb) == -1) {
		log(LL_ERR, "fstat on %s failed: %s\n", complete_path.c_str(), strerror(errno));
		fclose(fh);
		return false;
	}

	bool rc = true;

	std::string name_header = dl ? myformat("Content-Disposition: attachment; filename=\"%s\"\r\n", name) : "";
	std::string headers = "HTTP/1.0 200 OK\r\nServer: " NAME " " VERSION "\r\n" + name_header + "Content-Type: " + type + "\r\nDate: " + date_header(0) + "\r\nLast-Modified:" + date_header(sb.st_mtime * 1000ll * 1000ll) + "\r\n" + cookie + "\r\n";
	if (WRITE_SSL(hh, headers.c_str(), headers.size()) <= 0)
		rc = false;

	st->track_bw(headers.size());

	while(!feof(fh) && rc) {
		char buffer[4096];

		int rc = fread(buffer, 1, sizeof(buffer), fh);
		if (rc <= 0) {
			rc = false;
			break;
		}

		if (WRITE_SSL(hh, buffer, rc) <= 0) {
			log(LL_INFO, "Short write");
			rc = false;
			break;
		}

		st->track_bw(rc);
	}

	fclose(fh);

	return rc;
}

bool http_server::get_fs_db_pars(const std::map<std::string, std::string> & pars, std::string *const name, int *const width, int *const height, int *const nw, int *const nh, int *const feed_w, int *const feed_h)
{
	auto db_it = pars.find("dashboard");
	if (db_it == pars.end())
		return false;

	*name = db_it->second;

	auto width_it = pars.find("width");
	if (width_it == pars.end())
		return false;

	*width = atoi(width_it->second.c_str());

	auto height_it = pars.find("height");
	if (height_it == pars.end())
		return false;

	*height = atoi(height_it->second.c_str());

	auto nw_it = pars.find("nw");
	if (nw_it == pars.end())
		return false;

	*nw = atoi(nw_it->second.c_str());

	auto nh_it = pars.find("nh");
	if (nh_it == pars.end())
		return false;

	*nh = atoi(nh_it->second.c_str());

	auto feed_w_it = pars.find("feed_w");
	if (feed_w_it == pars.end())
		return false;

	*feed_w = atoi(feed_w_it->second.c_str());

	auto feed_h_it = pars.find("feed_h");
	if (feed_h_it == pars.end())
		return false;

	*feed_h = atoi(feed_h_it->second.c_str());

	return true;
}

void http_server::send_fs_db_mjpeg(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars, const std::string & cookie, int quality)
{
	std::string dashboard_name;
	int width = -1, height = -1, nw = -1, nh = -1, feed_w = -1, feed_h = -1;
	if (!get_fs_db_pars(pars, &dashboard_name, &width, &height, &nw, &nh, &feed_w, &feed_h))
		return;

	std::vector<view_src_t> sources;
	auto views = get_db()->get_dashboard_ids(username, dashboard_name);

	for(auto v : views) {
		if (v.type == DE_SOURCE) {
			instance *inst = nullptr;
			interface *i = nullptr;
			find_by_id(cfg, v.id, &inst, &i);

			if (i)
				sources.push_back({ i->get_id(), none });
		}
		else if (v.type == DE_VIEW) {
			view *sv = find_view(this->views, v.id);

			if (sv)
				sources.push_back({ sv->get_id(), none });
		}
	}

	log(LL_INFO, "Requesting stream at resolution %dx%d (%dx%d, %dx%d each)", width, height, nw, nh, feed_w, feed_h);

	view_all va(cfg, "fs-db_"+ username, "full screen dashboard", width, height, nw, nh, sources, -1, nullptr, quality);
	va.start();

	bool first = true;

	uint64_t prev = 0;

	for(;!local_stop_flag;) {
		video_frame *pvf = va.get_frame(true, prev);

		if (pvf) {
			// send header
			constexpr char term[] = "\r\n";

			if (first) {
				first = false;

				std::string reply_headers = myformat(
					"HTTP/1.0 200 OK\r\n"
					"Cache-Control: no-cache\r\n"
					"Pragma: no-cache\r\n"
					"Server: " NAME " " VERSION "\r\n"
					"Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
					"Last-Modified: %s\r\n"
					"Date: %s\r\n"
					"Connection: close\r\n"
					"%s"
					"Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
					"\r\n", date_header(prev).c_str(), date_header(0).c_str(), cookie.c_str());

				if (WRITE_SSL(ct->hh, reply_headers.c_str(), reply_headers.size()) <= 0) {
					log(LL_DEBUG, "short write on response header");
					delete pvf;
					break;
				}
			}
			else if (WRITE_SSL(ct->hh, term, strlen(term)) <= 0)
			{
				log(LL_DEBUG, "short write on terminating cr/lf: %s", strerror(errno));
				delete pvf;
				break;
			}

			ct->st.track_bw(2);

			auto img = pvf->get_data_and_len(E_JPEG);

			char img_h[4096] = { 0 };
			int len = snprintf(img_h, sizeof img_h, 
				"--myboundary\r\n"
				"Content-Type: image/jpeg\r\n"
				"Content-Length: %zu\r\n"
				"\r\n", std::get<1>(img));

			if (WRITE_SSL(ct->hh, img_h, len) <= 0)
			{
				log(LL_DEBUG, "short write on boundary header: %s", strerror(errno));
				delete pvf;
				break;
			}

			if (WRITE_SSL(ct->hh, (const char *)std::get<0>(img), std::get<1>(img)) <= 0)
			{
				log(LL_DEBUG, "short write on img data: %s", strerror(errno));
				delete pvf;
				break;
			}

			ct->st.track_bw(len + std::get<1>(img));

			delete pvf;
		}

		ct->st.track_cpu_usage();
	}

	va.stop();
}

std::string http_server::get_motd(const std::string & motd_file)
{
	std::string reply;

	if (!motd_file.empty()) {
		reply += "<section><h2>message of the day</h2>\n";

		FILE *fh = fopen(motd_file.c_str(), "rb");
		if (fh) {
			reply += "<p>";

			char buffer[4096];
			int n = 0;
			if ((n = fread(buffer, 1, sizeof buffer - 1, fh)) <= 0)
				strcpy(buffer, "(read error)");
			else
				buffer[n] = 0x00;

			reply += buffer;

			reply += "</p>";

			fclose(fh);
		}
		else {
			reply += "<p><strong>Problem opening motd file.</strong></p>";
			log(LL_WARNING, "Failed accessing motd file (%s)", motd_file.c_str());
		}

		reply += "</section>\n";
	}

	return reply;
}
