// (C) 2017 by folkert van heusden, this file is released under the AGPL v3.0 license.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/des.h>
#include <sys/time.h>

#include "../source.h"
#include "../log.h"
#include "../utils.h"

static int get_card16(unsigned char *p)
{
	uint16_t *P = (uint16_t *)p;

	return ntohs(*P);
}

static int get_card32(unsigned char *p)
{
	uint32_t *P = (uint32_t *)p;

	return ntohl(*P);
}

static std::string get_errno_string()
{
	return strerror(errno);
}

static void reconnect(const std::string & host, const int port, const std::string & vnc_password, std::atomic_bool *const stop_flag, int *const fd, int *const vnc_width, int *const vnc_height, bool *const swap_rgb)
{
	std::string host_details = myformat("[%s]:%d", host.c_str(), port);

	log(LL_INFO, "(Re-)connect to %s", host_details.c_str());

	*fd = -1;

	for(;;) {
		if (*fd != -1) {
			usleep(751000);
			close(*fd);
		}

		*fd = connect_to(host, port, stop_flag);
		if (*fd == -1) {
			log(LL_INFO, "Cannot connect to %s (%s)", host_details.c_str(), get_errno_string().c_str());
			continue;
		}

		// receiver VNC-server protocol version
		char ProtocolVersion[12 + 1];
		if (READ(*fd, ProtocolVersion, 12) != 12) {
			log(LL_WARNING, host_details + ": protocol version handshake short read (" + get_errno_string() + ")");
			continue;
		}
		ProtocolVersion[12] = 0;

		log(LL_DEBUG, host_details + ": protocol version: " + ProtocolVersion);

		// send VNC-client protocol version
		char my_prot_version[] = "RFB 003.003\n";
		if (WRITE(*fd, my_prot_version, 12) != 12) {
			log(LL_WARNING, host_details + ": protocol version handshake short write (" + get_errno_string() + ")");
			continue;
		}

		char auth[4];
		if (READ(*fd, auth, 4) != 4) {
			log(LL_WARNING, host_details + ": auth method short read (" + get_errno_string() + ")");
			continue;
		}

		if (auth[3] == 0) { // CONNECTION FAILED
			std::string reason;

			for(;;) {
				unsigned char auth_err_len[4];
				if (READ(*fd, reinterpret_cast<char *>(auth_err_len), 4) != 4)
					break;

				int len = get_card16(auth_err_len);
				if (len <= 0)
					break;

				char *msg = new char[len + 1];
				if (READ(*fd, msg, len) != len) {
					delete [] msg;
					break;
				}

				msg[len] = 0x00;

				reason = msg;
				delete [] msg;

				break;
			}

			log(LL_WARNING, host_details + ": connection failed (" + reason + ")");

			continue;
		}
		else if (auth[3] == 1) { // NO AUTHENTICATION
			// do nothing
			log(LL_INFO, host_details + ": log-on without password");
		}
		else if (auth[3] == 2) // AUTHENTICATION
		{
			unsigned char challenge[16 + 1];
			if (READ(*fd, reinterpret_cast<char *>(challenge), 16) != 16)
			{
				log(LL_ERR, host_details + ": challenge short read (" + get_errno_string() + ")");
				continue;
			}

			// retrieve first 8 characters of password and pad with 0x00
			const_DES_cblock password_padded;
			memset(password_padded, 0x00, sizeof password_padded);
			memcpy(password_padded, vnc_password.c_str(), std::min(8, int(vnc_password.size())));
			// then vnc swaps the bits in the password
			for(int index=0; index<8; index++)
			{ 
				int cur = password_padded[index];
				password_padded[index] = static_cast<unsigned char>(((cur & 0x01)<<7) + ((cur & 0x02)<<5) + ((cur & 0x04)<<3) + ((cur & 0x08)<<1) + ((cur & 0x10)>>1) + ((cur & 0x20)>>3) + ((cur & 0x40)>>5) + ((cur & 0x80)>>7));
			}
			// setup key schedule
			DES_key_schedule key_schedule;
			DES_set_key_unchecked(&password_padded, &key_schedule);

			// encrypt
			unsigned char result[16];
			const_DES_cblock ch1, ch2;
			memcpy(ch1, &challenge[0], 8);
			memcpy(ch2, &challenge[8], 8);
			DES_cblock r1, r2;
			DES_ecb_encrypt(&ch1, &r1, &key_schedule, DES_ENCRYPT);
			DES_ecb_encrypt(&ch2, &r2, &key_schedule, DES_ENCRYPT);
			memcpy(&result[0], r1, 8);
			memcpy(&result[8], r2, 8);
			// transmit result to server
			if (WRITE(*fd, reinterpret_cast<char *>(result), 16) != 16)
			{
				log(LL_ERR, host_details + ": auth response short write (" + get_errno_string() + ")");
				continue;
			}

			char auth_answer[4];
			if (READ(*fd, auth_answer, 4) != 4)
			{
				log(LL_ERR, host_details + ": auth response short read (" + get_errno_string() + ")");
				continue;
			}

			if (auth_answer[3] != 0)
			{
				log(LL_ERR, host_details + ": authentication failed");
				continue;
			}

			log(LL_INFO, host_details + ": authentication succeeded");
		}
		else
		{
			log(LL_ERR, host_details + ": connection failed, unknown authentication");
			continue;
		}

		char do_shared = 1;
		if (WRITE(*fd, &do_shared, 1) != 1) {
			log(LL_ERR, host_details + ": enable shared failed (" + get_errno_string() + ")");
			continue;
		}

		unsigned char parameters[2 + 2 + 16 + 4];
		if (READ(*fd, reinterpret_cast<char *>(parameters), 24) != 24) {
			log(LL_ERR, host_details + ": parameters short read (" + get_errno_string() + ")");
			break;
		}

		*swap_rgb = parameters[6] != 0;
		*vnc_width = get_card16(&parameters[0]);
		*vnc_height = get_card16(&parameters[2]);

		log(LL_DEBUG, "%s: w/h: %dx%d", host_details.c_str(), *vnc_width, *vnc_height);

		char *vnc_name = new char[parameters[23] + 1];
		if (READ(*fd, vnc_name, parameters[23]) != parameters[23]) {
			delete [] vnc_name;
			log(LL_ERR, host_details + ": name short read (" + get_errno_string() + ")");
			continue;
		}

		vnc_name[parameters[23]] = 0x00;
		log(LL_DEBUG, host_details + ": session name: " + std::string(vnc_name));
		delete [] vnc_name;

		log(LL_DEBUG, "bits per pixel: %d, depth: %d, r/g/b shift: %d/%d/%d", parameters[4], parameters[5], parameters[14], parameters[15], parameters[16]);

		// printf("%d, %d, %d\n", get_card16(&parameters[4+4]), get_card16(&parameters[6+4]), get_card16(&parameters[8+4]));
		if (parameters[4] != 32) {
			log(LL_ERR, host_details + ": only 32 bit/pixel supported");
			continue;
		}

		// set encoding
		// char encoding[] = { 2, 0, 0, 5, 0, 0, 0, 5, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 0 };
		char encoding[] = { 2, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 0 };
		if (WRITE(*fd, encoding, sizeof encoding) != sizeof encoding) {
			log(LL_ERR, host_details + ": short write for encoding message (" + get_errno_string() + ")");
			continue;
		}

		log(LL_INFO, host_details + ": session started");

		break;
	}
}

bool draw_vnc(const int cur_fd, const unsigned int vnc_width, const unsigned int vnc_height, uint8_t *const fb)
{
	unsigned char header[4];
	unsigned char bg_color[4], fg_color[4];

	if (READ(cur_fd, reinterpret_cast<char *>(header), 1) != 1)
		return false;

	if (header[0] == 2) { // BELL
		log(LL_DEBUG, "VNC: bell");
		return true;
	}

	if (header[0] != 0) {
		log(LL_DEBUG, "Unknown VNC server command %d\n", header[0]);
		return true; // don't know how to handle but just continue, maybe it works
	}

	if (READ(cur_fd, reinterpret_cast<char *>(&header[1]), 3) != 3)
		return false;

	unsigned int n_rec = get_card16(&header[2]);

	//printf("# rectangles: %d\n", n_rec);
	char *pixels = (char *)fb;
	for(unsigned int nr=0; nr<n_rec; nr++) {
		unsigned char rectangle_header[12];

		if (READ(cur_fd, reinterpret_cast<char *>(rectangle_header), 12) != 12)
			return false;

		unsigned int cur_xo= get_card16(&rectangle_header[0]);
		unsigned int cur_yo= get_card16(&rectangle_header[2]);
		unsigned int cur_w = get_card16(&rectangle_header[4]);
		unsigned int cur_h = get_card16(&rectangle_header[6]);

		int encoding = get_card32(&rectangle_header[8]);
		//printf("encoding: %d\n", encoding);
		if (encoding == 0) // RAW
		{
			//printf("%d,%d %dx%d\n", cur_xo, cur_yo, cur_w, cur_h);

			int n_bytes = cur_w * 4;
			for(unsigned int y=cur_yo; y<(cur_yo + cur_h); y++)
			{
				if (READ(cur_fd, &pixels[y * vnc_width * 4 + cur_xo * 4], n_bytes) != n_bytes)
					break;
			}
			//printf(" updated\n");
		}
		else if (encoding == 1) // copy rect
		{
			unsigned char src[4];
			if (READ(cur_fd, reinterpret_cast<char *>(src), 4) != 4)
				return false;

			unsigned int src_x = std::min(vnc_width, (unsigned int)get_card16(&src[0]));
			unsigned int src_y = std::min(vnc_height, (unsigned int)get_card16(&src[2]));
			//printf("%d,%d %dx%d from %d,%d\n", cur_xo, cur_yo, cur_w, cur_h, src_x, src_y);

			for(unsigned int y=0; y<cur_h; y++)
			{
				memmove(&pixels[(y + cur_yo) * vnc_width * 4 + cur_xo * 4], &pixels[(y + src_y) * vnc_width * 4 + src_x * 4], cur_w * 4);
			}
			// printf(" updated\n");
		}
		else if (encoding == 2) // RRE
		{
			unsigned char sub_header[4];
			if (READ(cur_fd, reinterpret_cast<char *>(sub_header), 4) != 4)
				return false;
			if (READ(cur_fd, reinterpret_cast<char *>(bg_color), 4) != 4)
				return false;

			unsigned int n_subrect = get_card32(sub_header);
			// printf("# sub rectangles: %d\n", n_subrect);

			for(unsigned int y=cur_yo; y<(cur_yo + cur_h); y++)
			{
				for(unsigned int x=cur_xo; x<(cur_xo + cur_w); x++)
					memcpy(&pixels[y * vnc_width * 4 + x * 4], bg_color, 4);
			}

			for(unsigned int rnr=0; rnr<n_subrect; rnr++)
			{
				if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
					return false;
				unsigned char subrect[8];
				if (READ(cur_fd, reinterpret_cast<char *>(subrect), 8) != 8)
					return false;
				unsigned int x_pos = get_card16(&subrect[0]) + cur_xo;
				unsigned int y_pos = get_card16(&subrect[2]) + cur_yo;
				unsigned int scur_w = get_card16(&subrect[4]);
				unsigned int scur_h = get_card16(&subrect[6]);

				for(unsigned int y=y_pos; y<(y_pos + scur_h); y++)
				{
					for(unsigned int x=x_pos; x<(x_pos + scur_w); x++)
						memcpy(&pixels[y * vnc_width * 4 + x * 4], fg_color, 4);
				}
			}
			// printf(" updated\n");
		}
		else if (encoding == 4) // CORRE
		{
			unsigned char sub_header[4];
			if (READ(cur_fd, reinterpret_cast<char *>(sub_header), 4) != 4)
				return false;
			if (READ(cur_fd, reinterpret_cast<char *>(bg_color), 4) != 4)
				return false;

			unsigned int n_subrect = get_card32(sub_header);
			// printf("# sub rectangles: %d\n", n_subrect);

			for(unsigned int y=cur_yo; y<(cur_yo + cur_h); y++)
			{
				for(unsigned int x=cur_xo; x<(cur_xo + cur_w); x++)
					memcpy(&pixels[y * vnc_width * 4 + x * 4], bg_color, 4);
			}

			for(unsigned int rnr=0; rnr<n_subrect; rnr++)
			{
				if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
					return false;
				unsigned char subrect[4];
				if (READ(cur_fd, reinterpret_cast<char *>(subrect), 4) != 4)
					return false;
				unsigned int x_pos = subrect[0] + cur_xo;
				unsigned int y_pos = subrect[1] + cur_yo;
				unsigned int scur_w = subrect[2];
				unsigned int scur_h = subrect[3];

				for(unsigned int y=y_pos; y<(y_pos + scur_h); y++)
				{
					for(unsigned int x=x_pos; x<(x_pos + scur_w); x++)
						memcpy(&pixels[y * vnc_width * 4 + x * 4], fg_color, 4);
				}
			}
			// printf(" updated\n");
		}
		else if (encoding == 5) // HEXTILE
		{
			unsigned int ht_w = cur_w / 16 + (cur_w & 15 ? 1 : 0);
			unsigned int ht_h = cur_h / 16 + (cur_h & 15 ? 1 : 0);

			for(unsigned int ht_y=0; ht_y<ht_h; ht_y++)
			{
				for(unsigned int ht_x=0; ht_x<ht_w; ht_x++)
				{
					unsigned char subencoding = 0;
					if (READ(cur_fd, reinterpret_cast<char *>(&subencoding), 1) != 1)
						return false;

					if (subencoding & 1) // RAW, just read bytes & set
					{
						char pixelscols[16 * 16 * 4];
						if (READ(cur_fd, pixelscols, sizeof pixelscols) != sizeof pixelscols)
							return false;

						int pixel_index = 0;
						for(unsigned int y=cur_yo + ht_y * 16; y<std::min(cur_yo + ht_y * 16 + 16, vnc_height); y++)
						{
							for(unsigned int x=cur_xo + ht_x * 16; x<std::min(cur_xo + ht_x * 16 + 16, vnc_width); x++)
							{
								memcpy(&pixels[y * vnc_width * 4 + x * 4], &pixelscols[pixel_index], 4);
								pixel_index += 4;
							}
						}
						memcpy(fg_color, &pixelscols[pixel_index - 4], 4);

						continue;
					}
					if (subencoding & 2) // background
					{
						if (READ(cur_fd, reinterpret_cast<char *>(bg_color), 4) != 4)
							return false;
					}
					if (subencoding & 4) // foreground
					{
						if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
							return false;
					}
					bool any_subrects = (subencoding & 8) ? true : false;
					bool subrects_color = (subencoding & 16) ? true : false;

					if (!any_subrects) // ! / *
					{
						for(unsigned int y=cur_yo + ht_y * 16; y<std::min(cur_yo + ht_y * 16 + 16, vnc_height); y++)
						{
							for(unsigned int x=cur_xo + ht_x * 16; x<std::min(cur_xo + ht_x * 16 + 16, vnc_width); x++)
								memcpy(&pixels[y * vnc_width * 4 + x * 4], bg_color, 4);
						}

						continue;
					}

					unsigned char subsubrects = 0;
					if (READ(cur_fd, reinterpret_cast<char *>(&subsubrects), 1) != 1)
						return false;
					for(unsigned int ss_index=0; ss_index<subsubrects; ss_index++)
					{
						if (subrects_color) // T / T
						{
							if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
								return false;
						}

						unsigned char xywh[2];
						if (READ(cur_fd, reinterpret_cast<char *>(xywh), 2) != 2)
							return false;

						unsigned int ssx = xywh[0] >> 4;
						unsigned int ssy = xywh[0] & 15;
						unsigned int ssw = (xywh[1] >> 4) + 1;
						unsigned int ssh = (xywh[1] & 15) + 1;

						for(unsigned int y=cur_yo + ht_y * 16 + ssy; y<std::min(cur_yo + ht_y * 16 + ssy + ssh, vnc_height); y++)
						{
							for(unsigned int x=cur_xo + ht_x * 16 + ssx; x<std::min(cur_xo + ht_x * 16 + ssx + ssw, vnc_width); x++)
								memcpy(&pixels[y * vnc_width * 4 + x * 4], fg_color, 4);
						}
					}
				}
			}
			printf(" updated\n");
		}
	}

	return true;
}

/// FINISHED vvvvv

bool request_update(const int cur_fd, const int vnc_width, const int vnc_height)
{
	// request update
	//printf("request update %dx%d\n", vnc_width, vnc_height);
	char upd_request[10];
	upd_request[0] = 3;
	upd_request[1] = 0; // incremental == 1
	upd_request[2] = upd_request[3] = 0; // x
	upd_request[4] = upd_request[5] = 0; // y
	upd_request[6] = char(vnc_width >> 8);
	upd_request[7] = char(vnc_width & 255);
	upd_request[8] = char(vnc_height >> 8);
	upd_request[9] = char(vnc_height & 255);

	if (WRITE(cur_fd, upd_request, 10) != 10)
		return false;

	return true;
}

typedef struct
{
	source *s;
	std::string host, password;
	int port;
	std::atomic_bool stop_flag;
	double fps;
	pthread_t th;
} my_data_t;

void * thread(void *arg)
{
	log(LL_INFO, "source plugin thread started");

	my_data_t *md = (my_data_t *)arg;

	std::string host_details = myformat("[%s]:%d", md -> host.c_str(), md -> port);

	while(!md -> stop_flag) {
		usleep(10000);

		set_thread_name("vnc-client");

		int fd = -1, width = 0, height = 0;

		uint8_t *fb = NULL, *fb3 = NULL;
		size_t bytes = 0;

		bool swap_rgb = false;
		reconnect(md -> host, md -> port, md -> password, &md -> stop_flag, &fd, &width, &height, &swap_rgb);

		bytes = width * height * 4;
		fb = (uint8_t *)valloc(bytes);

		fb3 = (uint8_t *)valloc(width * height * 3);

		md -> s -> set_size(width, height);

		bool upd_req = true;
		bool got_update = false;

		for(;!md -> stop_flag;)
		{
			if (upd_req)
			{
				upd_req = false;

				if (!request_update(fd, width, height))
					break;
			}

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 100000;

			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);

			if (select(fd + 1, &rfds, NULL, NULL, &tv) == -1)
			{
				if (errno == EINTR || errno == EAGAIN)
					continue;

				log(LL_ERR, host_details + ": connection problems (" + get_errno_string() + ")");

				break;
			}

			if (FD_ISSET(fd, &rfds)) {
				if (!draw_vnc(fd, width, height, fb))
					break;

				got_update = true;
			}

			if (got_update) {
				got_update = false;

				upd_req = true;

				if (swap_rgb) {
					for(int i=0; i<width*height; i++) {
						int oi = i * 4;
						int oo = i * 3;
						fb3[oo + 0] = fb[oi + 0];
						fb3[oo + 1] = fb[oi + 1];
						fb3[oo + 2] = fb[oi + 2];
					}
				}
				else {
					for(int i=0; i<width*height; i++) {
						int oi = i * 4;
						int oo = i * 3;
						fb3[oo + 2] = fb[oi + 0];
						fb3[oo + 1] = fb[oi + 1];
						fb3[oo + 0] = fb[oi + 2];
					}
				}

				md -> s -> set_frame(E_RGB, fb3, bytes);

			}

			if (md -> fps > 0.0)
				mysleep(1.0 / md -> fps, &md -> stop_flag, NULL);
		}

		close(fd);

		free(fb);
		fb = NULL;

		free(fb3);
		fb3 = NULL;
	}

	log(LL_INFO, "source plugin for VNC client thread ending");

	return NULL;
}

extern "C" void *init_plugin(source *const s, const char *const argument)
{
	my_data_t *md = new my_data_t;
	md -> s = s;
	md -> stop_flag = false;

	md -> host = "localhost";
	md -> port = 5901;
	md -> password = "";

	if (argument) {
		char *temp = strdup(argument);

		char *sp = strchr(temp, ' ');
		if (sp) {
			*sp = 0x00;

			md -> port = atoi(sp + 1);

			char *sp2 = strchr(sp + 1, ' ');
			if (sp2)
				md -> password = sp2 + 1;
		}

		md -> host = temp;

		free(temp);
	}

	pthread_create(&md -> th, NULL, thread, md);

	return md;
}

extern "C" void uninit_plugin(void *arg)
{
	my_data_t *md = (my_data_t *)arg;

	md -> stop_flag = true;

	pthread_join(md -> th, NULL);

	delete md;
}
