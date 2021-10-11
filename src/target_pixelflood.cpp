// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <math.h>
#include <cstring>
#include <unistd.h>

#include "target_pixelflood.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "resize.h"
#include "schedule.h"

target_pixelflood::target_pixelflood(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const std::string & host, const int port, const int pfw, const int pfh, const int quality, const pixelflood_protocol_t pp, const int xoff, const int yoff, const bool handle_failure, schedule *const sched) : target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", override_fps, cfg, false, handle_failure, sched), quality(quality), host(host), port(port), pw(pfw), ph(pfh), pp(pp), xoff(xoff), yoff(yoff)
{
	if (this -> descr == "")
		this -> descr = store_path + "/" + prefix;

	if (pp == PP_TCP_TXT) {
		fd = connect_to(host, port, nullptr);

		if (fd == -1)
			error_exit(true, "Connect to %s:%d failed", host.c_str(), port);
	}
	else if (pp == PP_UDP_BIN || pp == PP_UDP_TXT) {
		fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd == -1)
			error_exit(true, "Cannot create socket");

		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);
		servaddr.sin_addr.s_addr = inet_addr(host.c_str());
	}
	else {
		log(id, LL_ERR, "No protocol selected");
	}
}

target_pixelflood::~target_pixelflood()
{
	stop();

	close(fd);
}

bool target_pixelflood::send_frame(const uint8_t *const data, const int w, const int h)
{
	bool rc = true;
	char buffer[65507];
	int o = 0;

	if (pp == PP_UDP_BIN) {
		buffer[0] = 0; // protocol version 0
		buffer[1] = 0;
		o = 2;
	}

	for(int y=0; y<ph - yoff; y++) {
		for(int x=0; x<pw - xoff; x++) {
			const uint8_t *const p = &data[y * w * 3 + x * 3];

			int cx = x + xoff;
			int cy = y + yoff;

			if (pp == PP_UDP_BIN) {
				buffer[o++] = cx;
				buffer[o++] = cx >> 8;
				buffer[o++] = cy;
				buffer[o++] = cy >> 8;
				buffer[o++] = p[0];
				buffer[o++] = p[1];
				buffer[o++] = p[2];

				if (o >= 1122 - 6) {
					if (sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
						log(id, LL_INFO, "Cannot send pixelflood packet (%s)", strerror(errno));

					st->track_bw(o);

					o = 2;
				}
			}
			else if (pp == PP_TCP_TXT) {
				o += sprintf(&buffer[o], "PX %d %d %02x%02x%02x\n", cx, cy, p[0], p[1], p[2]);

				if (o >= sizeof(buffer) - 100) {
					if (WRITE(fd, buffer, o) != o) {
						log(id, LL_INFO, "Cannot send pixelflood packet (%s)", strerror(errno));
						rc = false;
					}

					st->track_bw(o);

					o = 0;
				}
			}
			else if (pp == PP_UDP_TXT) {
				o += sprintf(&buffer[o], "PX %d %d %02x%02x%02x\n", cx, cy, p[0], p[1], p[2]);

				if (o >= 1400) {
					if (sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
						log(id, LL_INFO, "Cannot send pixelflood packet (%s)", strerror(errno));

					st->track_bw(o);

					o = 0;
				}
			}
		}
	}

	if (pp == PP_UDP_BIN) {
		if (o > 2) {
			st->track_bw(o);

			if (sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
				log(id, LL_INFO, "Cannot send pixelflood packet (%s)", strerror(errno));
		}
	}
	else if (pp == PP_TCP_TXT) {
		if (o > 0) {
			st->track_bw(o);

			if (WRITE(fd, buffer, o) != o) {
				log(id, LL_INFO, "Cannot send pixelflood packet (%s)", strerror(errno));
				rc = false;
			}
		}
	}
	else if (pp == PP_UDP_TXT) {
		if (o >= 0) {
			if (sendto(fd, buffer, o, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
				log(id, LL_INFO, "Cannot send pixelflood packet (%s)", strerror(errno));
			else
				st->track_bw(o);
		}
	}

	return rc;
}

void target_pixelflood::operator()()
{
	set_thread_name("pixelflood_" + prefix);

	s -> start();

	const double fps = 1.0 / interval;

	time_t cut_ts = time(nullptr) + max_time;

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;
	unsigned f_nr = 0;

	video_frame *prev_frame = nullptr;

	for(;!local_stop_flag;) {
		pauseCheck();
		st->track_fps();

		uint64_t before_ts = get_us();

		video_frame *pvf = s->get_frame(handle_failure, prev_ts);

		if (pvf) {
			prev_ts = pvf->get_ts();

			if (filters && !filters->empty()) {
				source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
				instance *inst = find_instance_by_interface(cfg, cur_s);

				video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;
			}

			if (pvf->get_w() != pw || pvf->get_h() != ph) {
				video_frame *temp = pvf->do_resize(cfg->r, pw, ph);
				delete pvf;
				pvf = temp;
			}

			const bool allow_store = sched == nullptr || (sched && sched->is_on());

			if (allow_store) {
				bool rc = send_frame(pvf->get_data(E_RGB), pvf->get_w(), pvf->get_h());

				if (!rc) {
					close(fd);

					fd = connect_to(host, port, nullptr);
				}
			}

			delete prev_frame;
			prev_frame = pvf;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;

	s -> stop();

	log(id, LL_INFO, "stopping");
}
