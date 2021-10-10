// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <atomic>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/time.h>

#include <libconfig.h++>
#include <turbojpeg.h>
#include <png.h>
#include <fontconfig/fontconfig.h>
#include <unicode/uversion.h>

#if HAVE_LIBSDL2 == 1
#include <SDL2/SDL.h>
#endif

#if HAVE_MYSQL == 1
#include <cppconn/version_info.h>
#endif

#if HAVE_OPENSSL == 1
#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#if HAVE_JANSSON == 1
#include <jansson.h>
#endif

#if HAVE_EXIV2 == 1
#include <exiv2/exiv2.hpp>
#endif

#if HAVE_LIBCURL == 1
#include <curl/curl.h>
#endif

#if HAVE_GSTREAMER == 1
#include <gst/gst.h>
#endif

#if HAVE_FREI0R == 1
#include <frei0r.h>
#endif

#if HAVE_WEBSOCKETPP == 1
#include <websocketpp/version.hpp>
#endif

#if HAVE_ALSA == 1
#include <alsa/version.h>
#endif

#if HAVE_PIPEWIRE == 1
#include <pipewire/pipewire.h>
#endif

#include "gen.h"
#include "db.h"
#include "log.h"
#include "target.h"
#include "error.h"
#include "draw_text.h"
#include "cleaner.h"
#include "resize.h"
#include "http_server.h"
#include "exec.h"
#include "announce_upnp.h"
#include "instance.h"
#include "utils.h"

std::atomic_bool terminate { false };

uint32_t crc32b(const uint8_t *const in, const size_t n)
{
	uint32_t crc = 0xFFFFFFFF;

	for(size_t i=0; i<n; i++) {
		crc ^= in[i];

		for(int j=0; j<8; j++) {
			uint32_t mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}

	return ~crc;
}

void run_nrpe_server(configuration_t *const cfg, std::atomic_bool *const local_stop_flag)
{
	log(LL_INFO, "Will listen for NRPE on %s.%d", cfg->nrpe.adapter.c_str(), cfg->nrpe.port);

	int fd = start_listen(cfg->nrpe);

	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	while(!*local_stop_flag) {
		if (poll(fds, 1, 500) <= 0)
			continue;

		int cfd = accept(fd, nullptr, nullptr);

		uint8_t recv_buffer[16];
		if (READ(cfd, (char *)recv_buffer, 16) != 16) {
			log(LL_WARNING, "Short read on NRPE request header");
			close(cfd);
			continue;
		}

		int version = (recv_buffer[0] << 8) | recv_buffer[1];
		log(LL_DEBUG_VERBOSE, "NRPE protocol version %d", version);
		if (version != 4 && version != 3) {
			log(LL_ERR, "Expected version 3 or 4, got %d", version);
			close(cfd);
			continue;
		}

		int type = (recv_buffer[2] << 8) | recv_buffer[3];
		log(LL_DEBUG_VERBOSE, "NRPE protocol type %d", type);

		if (recv_buffer[10] || recv_buffer[11]) {
			log(LL_ERR, "Alignment field should be 0");
			close(cfd);
			continue;
		}

		int len = (recv_buffer[12] << 24) | (recv_buffer[13] << 16) | (recv_buffer[14] << 8) | recv_buffer[15];
		if (len > 1020) {
			log(LL_WARNING, "Request too large: %d", len);
			len = 1020;
		}

		char request[1021] { 0 };
		if (READ(cfd, request, len) != len) {
			log(LL_WARNING, "Short read on NRPE request name");
			close(cfd);
			continue;
		}

		request[len] = 0x00;

		log(LL_DEBUG, "NRPE request for \"%s\" (by %s)", request, get_endpoint_name(cfd).c_str());

		std::string error = "internal error";
		int code = 3; // unknown

//		std::unique_lock<std::timed_mutex> lock(cfg->lock);

		if (request[0] && strcasecmp(request, "%ALL%") != 0) {
			interface *i = find_interface_by_id(cfg, request);
			if (i) {
				std::optional<error_state_t> o_error = i->get_last_error();

				if (o_error.has_value()) {
					error = o_error.value().msg;
					code = o_error.value().critical ? 2 : 1;
				}
				else {
					error = "OK";
					code = 0;
				}
			}
			else {
				error = myformat("UNKNOWN ID: %s", request);
			}
		}
		else {
			auto rc = find_errors(cfg);

			if (rc.has_value()) {
				std::string id = rc.value().first;
				error_state_t o_error = rc.value().second;

				error = id + ": " + o_error.msg;
				code = o_error.critical ? 2 : 1;
			}
			else {
				error = "all fine";
				code = 0;
			}
		}

//		lock.unlock();

		struct rusage ru { 0 };
		getrusage(RUSAGE_SELF, &ru);
		error += myformat("|cpu-usage=%f%%; rss=%lukB;", cfg->st->get_cpu_usage() * 100.0, ru.ru_maxrss);

		if (error.size() > 1020)
			error = error.substr(0, 1020);

		uint8_t response[1021 + 16] { 0 };
		response[0] = recv_buffer[0];
		response[1] = recv_buffer[1]; // version

		response[2] = 0;
		response[3] = 2; // response

		response[4] = response[5] = response[6] = response[7] = 0; // CRC32

		response[8] = 0;
		response[9] = code;

		response[10] = 0;
		response[11] = 0; // alignment

		int str_len = error.size();
		response[12] = str_len >> 24;
		response[13] = str_len >> 16;
		response[14] = str_len >> 8;
		response[15] = str_len;
		memcpy(&response[16], error.c_str(), str_len);

		size_t out_len = 16 + str_len;

		uint32_t crc = crc32b(response, out_len + (version == 3 ? 3 : 0));
		response[4] = crc >> 24;
		response[5] = crc >> 16;
		response[6] = crc >> 8;
		response[7] = crc;

		WRITE(cfd, (char *)response, out_len);

		close(cfd);
	}

	close(fd);

	log(LL_INFO, "NRPE thread terminating");
}

void sigh(int sig)
{
	terminate = true;
}

void write_pid_file(const char *const pid_file)
{
	if (pid_file) {
		FILE *fh = fopen(pid_file, "w");
		if (!fh)
			error_exit(true, "Cannot create %s", pid_file);

		fprintf(fh, "%d", getpid());

		fclose(fh);
	}
}

void version()
{
	printf(NAME " " VERSION "\n");
	printf("(C) 2017-2021 by Folkert van Heusden\n\n");
}

void help()
{
	printf("-c x   select configuration file\n");
	printf("-f     fork into the background.\n");
	printf("       when not forking, then the program can be stopped by pressing enter.\n");
	printf("-p x   file to write PID to\n");
	printf("-v     enable verbose mode\n");
	printf("-V     show version & exit\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	const char *pid_file = NULL;
	bool do_fork = false, verbose = false;
	int ll = LL_INFO;

#if HAVE_PIPEWIRE == 1
	pw_init(&argc, &argv);
#endif

	configuration_t *cfg = new configuration_t;

	cfg->cfg_file = "constatus.cfg";
	cfg->search_path = "./";

	int c = -1;
	while((c = getopt(argc, argv, "c:p:fhvV")) != -1) {
		switch(c) {
			case 'c': {
				cfg->cfg_file = optarg;

				size_t last_slash = cfg->cfg_file.rfind('/'); // windows users may want to replace this by a backslash
				if (last_slash != std::string::npos) // no path
					cfg->search_path = cfg->cfg_file.substr(0, last_slash);

				break;
			}

			case 'p':
				pid_file = optarg;
				break;

			case 'f':
				do_fork = true;
				break;

			case 'h':
				help();
				return 0;

			case 'v':
				verbose = true;
				ll++;
				break;

			case 'V':
				version();
				return 0;

			default:
				help();
				return 1;
		}
	}

	cfg->st = new stats_tracker("main", true);
	cfg->st->start();

	log(LL_DEBUG, "Search path: %s", cfg->search_path.c_str());

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	signal(SIGTERM, sigh);
	signal(SIGINT, sigh);

	curl_global_init(CURL_GLOBAL_ALL);

	draw_text::init_fonts();

#if HAVE_GSTREAMER == 1
	gst_init(0, nullptr);
#endif

	log(LL_INFO, "With libconfig %d.%d.%d", LIBCONFIGXX_VER_MAJOR, LIBCONFIGXX_VER_MINOR, LIBCONFIGXX_VER_REVISION);

	if (!load_configuration(cfg, verbose, ll))
		return 1;

	log(LL_INFO, "With libturbojpeg");

	log(LL_INFO, "With libpng " PNG_HEADER_VERSION_STRING);

	log(LL_INFO, "With fontconfig %d.%d.%d", FC_MAJOR, FC_MINOR, FC_REVISION);

	log(LL_INFO, "With libcurl " LIBCURL_VERSION);

#if HAVE_RYGEL == 1
	log(LL_INFO, "With rygel");
#endif

	UVersionInfo uvi;
	u_getVersion(uvi);
	char icu_buffer[U_MAX_VERSION_STRING_LENGTH];
	u_versionToString(uvi, icu_buffer);
	log(LL_INFO, "With icu %s", icu_buffer);

#if HAVE_GSTREAMER == 1
	log(LL_INFO, "With gstreamer %s", gst_version_string());
#else
	log(LL_INFO, "Without gstreamer");
#endif

#if HAVE_FFMPEG == 1
	log(LL_INFO, "With ffmpeg");
#else
	log(LL_INFO, "Without ffmpeg");
#endif

#if HAVE_JANSSON == 1
	log(LL_INFO, "With libjansson " JANSSON_VERSION);
#else
	log(LL_INFO, "Without libjansson");
#endif

#if HAVE_EXIV2 == 1
	log(LL_INFO, "With EXIV2 %s", Exiv2::versionString().c_str());
#else
	log(LL_INFO, "Without EXIV2");
#endif

#if HAVE_MYSQL == 1
	log(LL_INFO, "With MySQL " MYCPPCONN_DM_VERSION);
#else
	log(LL_INFO, "Without SMySQL");
#endif

#if HAVE_NETPBM == 1
	log(LL_INFO, "With NetPBM");
#else
	log(LL_INFO, "Without NetPBM");
#endif

#if HAVE_LIBPAM == 1
	log(LL_INFO, "With PAM");
#else
	log(LL_INFO, "Without PAM");
#endif

#if HAVE_FREI0R == 1
	log(LL_INFO, "With Frei0r %d.%d", FREI0R_MAJOR_VERSION, FREI0R_MINOR_VERSION);
#else
	log(LL_INFO, "Without Frei0r");
#endif

#if HAVE_LIBV4L2 == 1
	log(LL_INFO, "With Video4Linux2");
#else
	log(LL_INFO, "Without Video4Linux2");
#endif

#if HAVE_OPENSSL == 1
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();

	log(LL_INFO, "With OpenSSL %s", OPENSSL_VERSION_TEXT);
#else
	log(LL_INFO, "Without OpenSSL");
#endif

#if HAVE_LIBSDL2 == 1
	log(LL_INFO, "With SDL2 v%d.%d.%d", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#else
	log(LL_INFO, "Without SDL2");
#endif

#if HAVE_WEBSOCKETPP == 1
	log(LL_INFO, "With websocket++ %d.%d.%d", websocketpp::major_version, websocketpp::minor_version, websocketpp::patch_version);
#else
	log(LL_INFO, "Without websocket++");
#endif

#if HAVE_ALSA == 1
	log(LL_INFO, "With ALSA " SND_LIB_VERSION_STR);
#else
	log(LL_INFO, "Without ALSA");
#endif

#if HAVE_PIPEWIRE == 1
	log(LL_INFO, "With pipewire");
#else
	log(LL_INFO, "Without pipewire");
#endif

	if (do_fork && daemon(0, 0) == -1)
		error_exit(true, "daemon() failed");

	write_pid_file(pid_file);

	log(LL_INFO, "Starting threads");

	std::map<std::string, unsigned long> startup_events;

	for(instance * i : cfg->instances) {
		log(LL_DEBUG, "Instance %s", i->name.c_str());

		unsigned long int event_nr = 0;

		for(interface *t : i -> interfaces) {
			log(LL_DEBUG, "Starting %s", t->get_id().c_str());

			if (t->get_class_type() == CT_SOURCE) {
				event_nr = t->get_db() -> register_event(t->get_id(), EVT_GENERIC, "regular startup");
				startup_events.insert({ t->get_id(), event_nr });
			}

			if (t->get_on_demand())
				continue;

			if (t->get_class_type() == CT_TARGET) {
				std::vector<video_frame *> empty;
				((target *)t)->start(empty, event_nr);
			}
			else {
				t->start();
			}
		}
	}

	for(interface *t : cfg->global_http_servers) {
		if (!t->get_on_demand())
			t -> start();
	}

	for(interface *t : cfg->guis) {
		if (!t->get_on_demand())
			t -> start();
	}

	if (cfg->clnr) {
		log(LL_INFO, "Starting cleaner");
		cfg->clnr -> start();
	}

	http_server::global_st.start();

	std::atomic_bool local_stop_flag { false };

	std::thread *global_st_thread = new std::thread([&cfg, &local_stop_flag]() {
			set_thread_name("globstats");

			while(!local_stop_flag) {
				int bw = 0, conn_count = 0;
				calc_global_http_stats(cfg, &bw, &conn_count);

				http_server::global_st.track_bw(bw);
				http_server::global_st.track_cc(conn_count);

				sleep(1);
			}
		});

	std::thread *nrpe_server = cfg->nrpe.port == -1 ? nullptr : new std::thread([&cfg, &local_stop_flag]() {
			set_thread_name("nrpe server");

			run_nrpe_server(cfg, &local_stop_flag);
		});

	for(interface *t : cfg->announcers)
		t -> start();

	log(LL_INFO, "System started");

	if (do_fork) {
		// not sure if it is safe to use condition variables
		// in a signal handler
		while(!terminate)
			usleep(101000);
	}
	else {
		printf("Press enter to terminate\n");
		fflush(nullptr);

		struct pollfd fds[] = { { 0, POLLIN, 0 } };

		while(!terminate) {
			cfg->st->track_cpu_usage();

			if (poll(fds, 1, 100) == 1) {
				char c = 0;
				if (read(0, &c, 1) == 1 && (c == 13 || c == 10))
					break;
			}
		}
	}

	log(LL_INFO, "Terminating");

	local_stop_flag = true;

	log(LL_INFO, "Waiting for \"global statistics thread\" to terminate...");
	global_st_thread->join();
	delete global_st_thread;

	cfg->lock.lock();

	for(interface *t : cfg->announcers)
		t -> announce_stop();

	for(interface *t : cfg->global_http_servers)
		t -> announce_stop();

	for(interface *t : cfg->guis)
		t -> announce_stop();

	for(instance * i : cfg->instances) {
		for(interface *t : i -> interfaces)
			t -> announce_stop();
	}

	for(interface *t : cfg->announcers)
		t -> stop();

	for(interface *t : cfg->global_http_servers)
		t -> stop();

	for(interface *t : cfg->guis)
		t -> stop();

	for(instance * i : cfg->instances) {
		for(interface *t : i -> interfaces)
			t -> stop();
	}

	if (cfg->clnr)
		cfg->clnr -> stop();

	log(LL_INFO, "Waiting for threads to terminate...");

	if (nrpe_server) {
		nrpe_server->join();
		delete nrpe_server;
	}

	for(interface *t : cfg->global_http_servers)
		delete t;

	for(interface *t : cfg->guis)
		delete t;

	for(interface *t : cfg->announcers)
		delete t;

	for(instance * i : cfg->instances) {
		for(size_t l=0; l<i->interfaces.size();) {
			interface *t = i->interfaces.at(l);

			if (t->get_class_type() == CT_SOURCE) {
				t->get_db()->register_event(t->get_id(), EVT_GENERIC, "shutdown");

				auto evt = startup_events.find(t->get_id());
				if (evt != startup_events.end())
					t->get_db()->register_event_end(evt->second);
			}

			if (t->get_class_type() != CT_SOURCE && t->get_class_type() != CT_VIEW) {
				i->interfaces.erase(i->interfaces.begin() + l);
				delete t;
			}
			else {
				l++;
			}
		}
	}

	for(instance * i : cfg->instances) {
		for(size_t l=0; l<i->interfaces.size();) {
			interface *t = i->interfaces.at(l);

			if (t->get_class_type() == CT_VIEW) {
				i->interfaces.erase(i->interfaces.begin() + l);
				delete t;
			}
			else {
				l++;
			}
		}
	}

	for(instance * i : cfg->instances) {
		for(size_t l=0; l<i->interfaces.size();) {
			interface *t = i->interfaces.at(l);

			if (t->get_class_type() == CT_SOURCE) {
				i->interfaces.erase(i->interfaces.begin() + l);
				delete t;
			}
			else {
				l++;
			}
		}

		delete i;
	}

	delete cfg->r;

	delete cfg->clnr;

	delete cfg->st;

	draw_text::uninit_fonts();

#if HAVE_LIBCURL == 1
	curl_global_cleanup();
#endif

#if HAVE_GSTREAMER == 1
	gst_deinit();
#endif

#if HAVE_OPENSSL == 1
	EVP_cleanup();
#endif

	if (pid_file)
		unlink(pid_file);

	delete cfg;

#if HAVE_PIPEWIRE == 1
	pw_deinit();
#endif

	log(LL_INFO, "Bye bye");

	return 0;
}
