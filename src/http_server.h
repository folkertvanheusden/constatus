// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <atomic>
#include <vector>

#include "config.h"
#include "cfg.h"
#include <map>
#if HAVE_OPENSSL == 1
#include <openssl/ssl.h>
#endif
#include "stats_tracker.h"
#include "interface.h"
#include "webservices.h"
#include "utils.h"

class http_auth;
class filter;
class meta;
class source;
class view;

typedef struct
{
#if HAVE_OPENSSL == 1
	SSL *sh;
#endif
	int fd;
} h_handle_t;

class ws_server;
class http_cookies;
class http_server;

typedef struct {
	std::thread *th;
	h_handle_t hh;
	std::atomic_bool is_terminated;
	stats_tracker st { "st:http-server", false };
	std::string peer_name, base_url;
	bool is_stream;
} http_thread_t;

class http_server : public interface
{
private:
	const listen_adapter_t la;
	const double fps;
	const int quality, time_limit;
	const std::vector<filter *> *const filters;
	const int resize_w, resize_h;
	source *const motion_compatible;
	const bool allow_admin, archive_acces;
	configuration_t *const cfg;
	http_auth *const auth;
	const std::string snapshot_dir;
	const bool with_subdirs, is_rest;
	instance *const views, *const limit_to;
	const bool handle_failure;
	const std::string stylesheet, websocket_url;
	const int max_cookie_age;
	const std::string motd_file;
	const bool ws_privacy;

	http_cookies *c { nullptr };

#if HAVE_OPENSSL == 1
	ssl_pars_t sp;
	SSL_CTX *ssl_ctx { nullptr };
#endif

#if HAVE_WEBSOCKETPP == 1
	ws_server *ws { nullptr };
#else
	void *ws { nullptr };
#endif

	int fd;

	mutable std::mutex data_lock;
	std::vector<http_thread_t *> data;

	std::string get_websocket_js();

	void purge_threads();
	void handle_http_client_thread(http_thread_t *const ct);
	void handle_http_client(http_thread_t *const ct);

	void send_mjpeg_stream(h_handle_t & hh, source *s, double fps, int quality, bool get, int time_limit, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie);
	void send_theora_stream(h_handle_t & hh, source *s, double fps, int quality, bool get, int time_limit, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie);
	void send_mpng_stream(h_handle_t & hh, source *s, double fps, bool get, const int time_limit, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie);
	void send_png_frame(h_handle_t & hh, source *s, bool get, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie);
	void send_jpg_frame(h_handle_t & hh, source *s, bool get, int quality, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, stats_tracker *const st, const std::string & cookie);
	bool send_file(h_handle_t & hh, const std::string & path, const char *const name, const bool dl, stats_tracker *const st, const std::string & cookie, const bool path_is_valid);
	bool get_fs_db_pars(const std::map<std::string, std::string> & pars, std::string *const name, int *const width, int *const height, int *const nw, int *const nh, int *const feed_w, int *const feed_h);
	void send_fs_db_mjpeg(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars, const std::string & cookie, int quality);
	std::string get_motd(const std::string & motd_file);

	void send_404(http_thread_t *const ct, const std::string & path, const std::map<std::string, std::string> & pars, const std::string & cookie);
	void add_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars);
	void add_to_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars);
	void remove_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars);
	void remove_from_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars);
	void send_dashboard(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars, const std::string & cookie);
	void file_menu_db(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie);
	void file_menu(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, const std::string & cookie);
	void send_fs_db_html(http_thread_t *const ct, const std::string & username, const std::map<std::string, std::string> & pars, const std::string & cookie);
	void register_peer(const bool is_register, const std::string & peer_name);
	void send_stylesheet(http_thread_t *const ct, const std::string & cookie);
	void send_copypaste(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie);
	void send_gstats(http_thread_t *const ct, const std::string & cookie);
	void send_stats(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie);
	bool send_snapshot(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie, const bool db);
	void view_view(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie);
	void view_menu(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & cookie, const std::string & username);
	void view_file(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, const std::string & cookie);
	void pause_module(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, const std::string & path, const std::string & cookie);
	void start_stop_module(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, const std::string & path, const std::string & cookie);
	void restart_module(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, const std::string & cookie);
	void take_snapshot(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, source *const s, const std::string & cookie);
	void video_snapshot(http_thread_t *const ct, const std::map<std::string, std::string> & pars, const std::string & page_header, instance *const inst, source *const s, const bool is_view_proxy, const std::string & cookie);
	void authorize(http_thread_t *const ct, const std::vector<std::string> & header_lines, bool *const auth_ok, std::string *const username, std::string *const cookie);
	void send_stream_html(http_thread_t *const ct, const std::string & page_header, const std::string & iup, source *const s, const bool view_proxy, const std::string & cookie, const bool ogg);
	void do_auth(http_thread_t *const ct, const std::vector<std::string> & header_lines);
	void logout(http_thread_t *const ct, const std::string & username);
	void send_auth_html(http_thread_t *const ct, const std::string & cookie, const std::vector<std::string> & header_lines, const std::map<std::string, std::string> & pars);
	void send_redirect_auth_html(http_thread_t *const ct);
	void send_index_html(http_thread_t *const ct, const std::string & page_header, const std::string & iup, instance *const inst, source *const s, const std::string & username, const std::string & cookie);
	void send_favicon_ico(http_thread_t *const ct, const std::string & cookie);
	void send_icon_jpg(http_thread_t *const ct, const std::string & cookie);
	std::optional<std::string> get_headers(http_thread_t *const ct);
	void register_trigger_notifier(instance *const inst, const bool is_register, http_server *const me);
	void publish_motion_detected(void *ws, const std::string & subject);

public:
	http_server(configuration_t *const cfg, http_auth *const auth, instance *const limit_to, const std::string & id, const std::string & descr, const listen_adapter_t & la, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, const int resize_w, const int resize_h, source *const motion_compatible, const bool allow_admin, const bool archive_access, const std::string & snapshot_dir, const bool with_subdirs, const bool is_rest, instance *const views, const bool handle_failure, const ssl_pars_t *const sp, const std::string & stylesheet, const int websocket_port, const std::string & websocket_url, const int max_cookie_age, const std::string & motd_file, const bool ws_privacy);
	virtual ~http_server();

	static void mjpeg_stream_url(configuration_t *const cfg, const std::string & id, std::string *const img_url, std::string *const page_url);

	double get_cpu_usage() const override;
	int get_bw() const override;
	size_t get_connection_count() const;
	std::vector<std::string> get_active_connections() const;

	std::pair<std::string, std::string> gen_video_url(instance *const i);
	std::vector<std::pair<std::string, std::string> > get_motion_video_urls();

	std::pair<std::string, std::string> gen_still_images_url(instance *const i);
	std::vector<std::pair<std::string, std::string> > get_motion_still_images_urls();

	void notify_thread_of_event(const std::string & subject) override;

	std::string get_base_url();

	void operator()() override;

	static stats_tracker global_st;
};

std::string date_header(const uint64_t ts);
void calc_global_http_stats(const configuration_t *const cfg, int *const bw, int *const conn_count);
