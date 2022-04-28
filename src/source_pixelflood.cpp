// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <ctype.h>
#include <poll.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "error.h"
#include "source.h"
#include "source_pixelflood.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"
#include "exec.h"

typedef struct {
	pthread_t th_client;
	std::atomic_bool is_terminated;
	std::string id;
	uint8_t *fb;
	int width, height;
	int fd;
	listen_adapter_t la;
	std::atomic_bool *local_stop_flag;
	stats_tracker *st;
} pf_s_parameter_t;

int fromhex(int c)
{
	c = toupper(c);

	if (c >= 'A')
		return c - 'A' + 10;

	return c - '0';
}

bool handle_command(pf_s_parameter_t *pp, char *const line)
{
	if (strncmp(line, "SIZE", 4) == 0) {
		char out[32];
		int len = snprintf(out, sizeof out, "SIZE %d %d\n", pp -> width, pp -> height);

		return WRITE(pp->fd, out, len);
	}

	// PX 20 30 ff8800
	if (line[0] != 'P' || line[1] != 'X') {
		if (line[0])
			printf("invalid header: %d %d\n", line[0], line[1]);

		return false;
	}

	int state = 0;
	int tx = 0, ty = 0;
	int r = 0, g = 0, b = 0;

	char *p = line;
	while(*p) {
		if (state == 0) {
			if (*p == 'P') {
				p++;
				state++;
			}
			else {
				break;
			}
		}
		else if (state == 1) {
			if (*p == 'X') {
				p++;
				state++;
			}
			else {
				break;
			}
		}
		else if (state == 2) {
			if (*p != ' ')
				state++;
			else
				p++;
		}
		else if (state == 3) {
			if (*p >= '0' and *p <= '9') {
				tx *= 10;
				tx += *p - '0';
				p++;
			}
			else {
				state++;
			}
		}
		else if (state == 4) {
			if (*p != ' ')
				state++;
			else
				p++;
		}
		else if (state == 5) {
			if (*p >= '0' and *p <= '9') {
				ty *= 10;
				ty += *p - '0';
				p++;
			}
			else {
				state++;
			}
		}
		else if (state == 6) {
			if (*p != ' ')
				state++;
			else
				p++;
		}
		else if (state == 7) {
			int off = ty * pp -> width * 3 + tx * 3;

			if (*p == '\n') {
				// requesting the current pixel value

				int r = pp -> fb[off + 0], g = pp -> fb[off + 1], b = pp -> fb[off + 2];

				char buffer[64];
				int len = snprintf(buffer, sizeof buffer, "PX %d %d %02x%02x%02x\n", tx, ty, r, g, b);

				return WRITE(pp->fd, buffer, len);
			}

			r = fromhex(*p) << 4;
			p++;
			// from here it should've been state 8...12 :-)
			r |= fromhex(*p);
			p++;

			g = fromhex(*p) << 4;
			p++;
			g |= fromhex(*p);
			p++;

			b = fromhex(*p) << 4;
			p++;
			b |= fromhex(*p);
			p++;

			pp -> fb[off + 0] = r;
			pp -> fb[off + 1] = g;
			pp -> fb[off + 2] = b;

			break;
		}
	}

	return true;
}

void * tcp_receive_thread(void *p)
{
	pf_s_parameter_t *pp = (pf_s_parameter_t *)p;

	set_thread_name("src_pfCtcp");

	log(pp -> id, LL_INFO, "source pixelflood tcp handle thread started (%d)", pp->fd);

	char buffer[4096];
	int o = 0;

	struct pollfd fds[] { { pp->fd, POLLIN, 0 } };

	for(;!*pp->local_stop_flag;) {
		int rc = poll(fds, 1, 500);
		if (rc == 0)
			continue;

		if (o == sizeof buffer) {
			printf("buffer overflow\n");
			break;
		}

		int n = read(pp->fd, &buffer[o], sizeof buffer - 1 - o);
		if (n <= 0) {
			// printf("fd %d read error (%d): %d (%d)\n", pp->fd, n, errno, o);
			break;
		}

		pp -> st -> track_bw(n);

		buffer[o + n] = 0x00;
		// printf("received: |%s|\n", &buffer[o]);

		int old_o = o;
		o += n;

		// printf("before: |%s|\n", buffer);

		for(;;) {
			char *lf = strchr(&buffer[old_o], '\n');
			if (!lf)
				break;

			*lf = 0x00;

			// printf("command: %s\n", buffer);
			if (!handle_command(pp, buffer)) {
				printf("invalid PX\n");
				goto fail;
			}

			int offset_lf = lf - buffer;
			int bytes_left = o - (offset_lf + 1);

			if (bytes_left == 0) {
				o = 0;
				break;
			}

			if (bytes_left < 0) {
				printf("%d bytes left\n", bytes_left);
				goto fail;
			}

			memmove(buffer, lf + 1, bytes_left);
			o = bytes_left;
			buffer[o] = 0x00;
		}
	}

fail:
	close(pp->fd);

	log(pp -> id, LL_INFO, "source pixelflood tcp handle thread terminating (%d)", pp -> fd);

	pp->is_terminated = true;

	return nullptr;
}

void * udp_connection_thread_bin(void *p)
{
	pf_s_parameter_t *pp = (pf_s_parameter_t *)p;

	set_thread_name("src_pfSudp");

	log(pp -> id, LL_INFO, "source pixelflood udp listen thread started (port %d)", pp->la.port);

	int sfd = start_listen(pp->la);

	struct pollfd fds[] { { sfd, POLLIN, 0 } };

	for(;!*pp->local_stop_flag;) {
		uint8_t pkt[1500];

		if (poll(fds, 1, 250) != 1)
			continue;

		int rc = recv(sfd, pkt, sizeof pkt, 0);

		if (rc < 2)
			continue;

		if (pkt[0]) // only protocol version 0 is supported
			continue;

		if (pkt[1]) // only rgb is supported, not alpha
			continue;

		for(int i=2; i<rc; i += 7) {
			int tx = pkt[i] + (pkt[i + 1] << 8);
			int ty = pkt[i + 2] + (pkt[i + 3] << 8);

			int off = ty * pp -> width * 3 + tx * 3;

			if (tx < pp -> width && ty < pp -> height) {
				pp -> fb[off + 0] = pkt[i + 4];
				pp -> fb[off + 1] = pkt[i + 5];
				pp -> fb[off + 2] = pkt[i + 6];
			}
		}
	}

	close(sfd);

	log(pp -> id, LL_INFO, "source pixelflood udp listen thread terminating");

	delete pp;

	return nullptr;
}

void * udp_connection_thread_txt(void *p)
{
	pf_s_parameter_t *pp = (pf_s_parameter_t *)p;

	set_thread_name("src_pfCudp");

	log(pp -> id, LL_INFO, "source pixelflood udp handle thread started (%d)", pp->fd);

	int sfd = start_listen(pp->la);

	struct pollfd fds[] { { sfd, POLLIN, 0 } };

	for(;!*pp->local_stop_flag;) {
		char pkt[9000 + 1];

		if (poll(fds, 1, 250) != 1)
			continue;

		int rc = recv(sfd, pkt, sizeof pkt - 1, 0);
		if (rc == -1)
			continue;
		pkt[rc] = 0x00;

		pp -> st -> track_bw(rc);

		char *p = pkt;

		for(;;) {
			char *lf = strchr(p, '\n');
			if (!lf)
				break;

			*lf = 0x00;

			// printf("command: %s\n", pkt);
			if (!handle_command(pp, p)) {
				printf("invalid PX\n");
				goto fail;
			}
			
			p = lf + 1;
		}
	}

fail:
	close(pp->fd);

	log(pp -> id, LL_INFO, "source pixelflood tcp handle thread terminating (%d)", pp -> fd);

	pp->is_terminated = true;

	return nullptr;
}

void * tcp_connection_thread(void *p)
{
	pf_s_parameter_t *pp = (pf_s_parameter_t *)p;

	set_thread_name("src_pfStcp");

	log(pp -> id, LL_INFO, "source pixelflood tcp listen thread started (port %d)", pp->la.port);

	int sfd = start_listen(pp->la);

	struct pollfd fds[] { { sfd, POLLIN, 0 } };

	std::vector<pf_s_parameter_t *> handles;

	for(;!*pp->local_stop_flag;) {
		for(size_t i=0; i<handles.size();) {
			if (handles.at(i)->is_terminated) {
				pthread_join(handles.at(i)->th_client, NULL);

				delete handles.at(i);
				handles.erase(handles.begin() + i);
			}
			else {
				i++;
			}
		}

		if (poll(fds, 1, 500) == 0)
			continue;

		int cfd = accept(sfd, NULL, 0);
		if (cfd == -1)
			continue;

		log(pp -> id, LL_INFO, "source pixelflood tcp listen thread connection on fd %d", cfd);

		pf_s_parameter_t *ct = new pf_s_parameter_t;
		ct -> is_terminated = false;
		ct -> id = pp -> id;
		ct -> fd = cfd;
		ct -> fb = pp -> fb;
		ct -> width = pp -> width;
		ct -> height = pp -> height;
		ct -> local_stop_flag = pp -> local_stop_flag;
		ct -> la.port = -1; // not applicable for thread
		ct -> st = pp -> st;

		int rc = -1;
		if ((rc = pthread_create(&ct->th_client, nullptr, tcp_receive_thread, ct)) != 0) {
			errno = rc;
			error_exit(true, "pthread_create failed (tcp pixelflood client thread)");
		}

		handles.push_back(ct);
	}

	for(auto ct : handles) {
		*ct->local_stop_flag = true;

		pthread_join(ct->th_client, nullptr);

		delete ct;
	}

	close(sfd);

	log(pp -> id, LL_INFO, "source pixelflood tcp listen thread terminating");

	delete pp;

	return nullptr;
}

source_pixelflood::source_pixelflood(const std::string & id, const std::string & descr, const std::string & exec_failure, const listen_adapter_t & la, const int pixel_size_in, const pixelflood_protocol_t pp, const double max_fps, const int w_in, const int h_in, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, exec_failure, max_fps, w_in * pixel_size_in, h_in * pixel_size_in, loglevel, filters, failure, c, jpeg_quality, text_feeds, keep_aspectratio), la(la), pixel_size(pixel_size_in), pp(pp)
{
	int size = width * height * 3;

	frame_buffer = (uint8_t *)malloc(size);
	memset(frame_buffer, 0x11, size);
}

source_pixelflood::~source_pixelflood()
{
	stop();

	free(frame_buffer);

	delete c;
}

void source_pixelflood::operator()()
{
	log(id, LL_INFO, "source pixelflood thread started");

	set_thread_name("src_pixelflood");

	pf_s_parameter_t *ct = new pf_s_parameter_t;

	ct -> id = id;
	ct -> fd = -1; // not applicable here
	ct -> fb = frame_buffer;
	ct -> width = width;
	ct -> height = height;
	ct -> local_stop_flag = &local_stop_flag;
	ct -> la = la;
	ct -> st = st;

	if (pp == PP_TCP_TXT) {
		int rc = -1;
		if ((rc = pthread_create(&th_client, NULL, tcp_connection_thread, ct)) != 0) {
			errno = rc;
			error_exit(true, "pthread_create failed (tcp pixelflood server thread)");
		}
	}
	else if (pp == PP_UDP_BIN) {
		int rc = -1;
		if ((rc = pthread_create(&th_client, NULL, udp_connection_thread_bin, ct)) != 0) {
			errno = rc;
			error_exit(true, "pthread_create failed (udp pixelflood (bin) server thread)");
		}
	}
	else if (pp == PP_UDP_TXT) {
		int rc = -1;
		if ((rc = pthread_create(&th_client, NULL, udp_connection_thread_txt, ct)) != 0) {
			errno = rc;
			error_exit(true, "pthread_create failed (udp pixelflood (txt) server thread)");
		}
	}
	else {
		log(id, LL_ERR, "Internal error");
		delete ct;
		return;
	}

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	for(;!local_stop_flag;)
	{
		uint64_t start_ts = get_us();

		if (work_required() && !is_paused()) {
			if (pixel_size == 1) {
				set_frame(E_RGB, frame_buffer, IMS(width, height, 3));
			}
			else {
				uint8_t *temp_fb = (uint8_t *)malloc(IMS(width, height, 3 * pixel_size * pixel_size));

				for(int y=0; y<height; y++) {
					for(int x=0; x<width; x++) {
						int o_in = y * width * 3 + x * 3;

						uint8_t r = frame_buffer[o_in + 0];
						uint8_t g = frame_buffer[o_in + 1];
						uint8_t b = frame_buffer[o_in + 2];

						for(int yy=0; yy<pixel_size; yy++) {
							int o_out = y * width * 3 * pixel_size + yy * width * 3 + x * 3 * pixel_size;

							for(int xx=0; xx<pixel_size; x++) {
								temp_fb[o_out + xx * 3 + 0] = r;
								temp_fb[o_out + xx * 3 + 1] = g;
								temp_fb[o_out + xx * 3 + 2] = b;
							}
						}
					}
				}

				set_frame(E_RGB, temp_fb, IMS(width, height, 3 * pixel_size * pixel_size));

				free(temp_fb);
			}
		}

		st->track_cpu_usage();

		uint64_t end_ts = get_us();
		int64_t left = interval - (end_ts - start_ts);

		if (interval > 0 && left > 0)
			myusleep(left);
	}

	register_thread_end("source pixelflood");
}
