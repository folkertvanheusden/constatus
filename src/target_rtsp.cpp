// (C) 2026 by folkert van heusden, released under the MIT license
#include "config.h"
#include <cstring>
#include <math.h>
#include <poll.h>
#include <unistd.h>

#include "target_rtsp.h"
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

target_rtsp::target_rtsp(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int port, const int quality, const bool handle_failure, schedule *const sched, const bool is_jpeg, const bool follow_rfc) :
	target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", override_fps, cfg, false, handle_failure, sched),
	quality(quality),
	port(port),
	is_jpeg(is_jpeg),
	follow_rfc(follow_rfc)
{
	printf("HIER %f\n", interval);
}

target_rtsp::~target_rtsp()
{
	stop();
}

std::string target_rtsp::gen_sdp_payload_string(const std::string & session, const std::string & local_ip_addr)
{
	return  "v=0\r\n"
		"s=" + session + "\r\n" +
		"t=0 0\r\n" +
		"o=constatus 0 0 IN IP4 " + local_ip_addr + "\r\n" +
		"c=IN IP4 0.0.0.0\r\n" +
		"m=video 30000 RTP/AVP 112\r\n" +
		(is_jpeg ? "a=rtpmap:112 JPEG/90000\r\n" : "a=rtpmap:112 RAW/90000\r\n") +
		(is_jpeg ? "" : myformat("a=fmtp:112 sampling=rgb; colorimetry=BT709-2; interlace=0; width=%d; height=%d; depth=8;\r\n", s->get_width(), s->get_height()));
}

bool target_rtsp::send_frame_via_jpeg_rtp(video_frame *const pvf, const std::pair<int, int> local_fd_port, const sockaddr_in remote, const socklen_t remote_len, const uint32_t ssrc, uint32_t *const seq_nr, uint32_t *const timestamp)
{
	bool rc = true;
	const auto rgb = pvf->get_data_and_len(E_JPEG);
	const int h = pvf->get_h();
	const int w = pvf->get_w();

	constexpr const size_t max_pl_len = 1300;
	uint8_t *const buffer = new uint8_t[16 + 8 + max_pl_len];

	// find sos marker
	std::optional<size_t> sos_marker;
	for(size_t i=0; i<std::get<1>(rgb) - 1; i++) {
		if (std::get<0>(rgb)[i] == 0xff && std::get<0>(rgb)[i + 1] == 0xda) {
			sos_marker = i;
			break;
		}
	}
	if (sos_marker.has_value() == false) {
		printf("JPEG has no SOS-marker\n");
		return false;
	}

	size_t sos_offset    = sos_marker.value();
	size_t sos_length    = (std::get<0>(rgb)[sos_offset + 2] << 8) | std::get<0>(rgb)[sos_offset + 3];
	size_t entropy_start = sos_offset + sos_length;
	size_t entropy_len   = std::get<1>(rgb) - entropy_start;
	size_t offset        = entropy_start;

	if (follow_rfc == false) {
		offset        = 0;
		entropy_start = 0;  // uggly hack
		entropy_len   = std::get<1>(rgb);  // also uggly hack
	}

	while(offset < std::get<1>(rgb)) {
		size_t cur_len         = std::min(max_pl_len, std::get<1>(rgb) - offset);
		size_t fragment_offset = offset - entropy_start;
		bool   end_of_frame    = fragment_offset + cur_len == entropy_len;

		buffer[0] = 128;  // v2
		buffer[1] = 112 | (end_of_frame ? 128 : 0);  // schema id
		buffer[2] = *seq_nr >> 8;
		buffer[3] = *seq_nr;
		buffer[4] = *timestamp >> 24;
		buffer[5] = *timestamp >> 16;
		buffer[6] = *timestamp >>  8;
		buffer[7] = *timestamp;
		buffer[8] = ssrc >> 24;
		buffer[9] = ssrc >> 16;
		buffer[10] = ssrc >>  8;
		buffer[11] = ssrc;
		buffer[12] = 0;  // ?
		buffer[13] = fragment_offset >> 16;
		buffer[14] = fragment_offset >>  8;
		buffer[15] = fragment_offset;
		buffer[16] = 1;  // 4:2:0 jpeg, see picio.cpp
		buffer[17] = quality;  // should be 0...127, so this is probably not correct FIXME
		buffer[18] = (w + 7) / 8;
		buffer[19] = (h + 7) / 8;

		memcpy(&buffer[20], &std::get<0>(rgb)[offset], cur_len);

		if (sendto(local_fd_port.first, buffer, cur_len + 20, 0, (sockaddr *)&remote, remote_len) != cur_len + 20) {
			printf("%d %d | %d | %s\n", local_fd_port.first, local_fd_port.second, remote.sin_family, strerror(errno));
			rc = false;
			break;
		}

		(*seq_nr)++;
		offset += cur_len;
	}

	(*timestamp) += 90000 / interval;  // 90000 = clock

	if (!rc)
		printf("frame NOT sent!\n");

	return rc;
}

bool target_rtsp::send_frame_via_raw_rtp(video_frame *const pvf, const std::pair<int, int> local_fd_port, const sockaddr_in remote, const socklen_t remote_len, const uint32_t ssrc, uint32_t *const seq_nr, uint32_t *const timestamp)
{
	bool rc = true;
	const uint8_t *const rgb = pvf->get_data(E_RGB);
	const int h = pvf->get_h();
	const int w = pvf->get_w();

	uint8_t *const buffer = new uint8_t[w * 3 + 20];
	memset(buffer, 0x00, 20);

	buffer[0] = 128;  // v2
	buffer[4] = *timestamp >> 24;
	buffer[5] = *timestamp >> 16;
	buffer[6] = *timestamp >>  8;
	buffer[7] = *timestamp;
	buffer[8] = ssrc >> 24;
	buffer[9] = ssrc >> 16;
	buffer[10] = ssrc >>  8;
	buffer[11] = ssrc;
	int bytes = w * 3;
	buffer[14] = bytes >> 8;
	buffer[15] = bytes;

	for(int y=0; y<h; y++) {
		buffer[1] = 112 | (y == h - 1 ? 128 : 0);  // schema id
		buffer[2] = *seq_nr >> 8;
		buffer[3] = *seq_nr;
		buffer[12] = *seq_nr >> 24;
		buffer[13] = *seq_nr >> 16;
		buffer[16] = y >> 8;
		buffer[17] = y;

		memcpy(&buffer[20], &rgb[y * bytes], bytes);

		if (sendto(local_fd_port.first, buffer, bytes + 20, 0, (sockaddr *)&remote, remote_len) != bytes + 20) {
			printf("%d %d | %d | %s\n", local_fd_port.first, local_fd_port.second, remote.sin_family, strerror(errno));
			rc = false;
			break;
		}

		(*seq_nr)++;
	}

	(*timestamp) += 90000 / interval;  // 90000 = clock

	printf("frame sent: %d\n", rc);

	return rc;
}

void target_rtsp::rtsp_session(const int fd, sockaddr remote_addr, socklen_t remote_addr_len)
{
	pollfd fds[] { { fd, POLLIN, 0 } };

	constexpr const size_t rnd_bin_len = 8;
	uint8_t *rnd_bin = gen_random(rnd_bin_len);
	std::string session = bin_to_hex(rnd_bin, rnd_bin_len);
	free(rnd_bin);

	uint8_t *ssrc_bin = gen_random(sizeof(uint32_t));
	uint32_t ssrc = *reinterpret_cast<uint32_t *>(ssrc_bin);
	free(ssrc_bin);

	auto sport1 = allocate_udp_listener();  // fd, port nr
	auto sport2 = allocate_udp_listener();
	bool play   = false;
	int cport1 = 0;
	int cport2 = 0;

	std::string local_name = "127.0.0.1";  // FIXME get_socket_name(sport1.first);

	std::string session_buffer;
	while(!local_stop_flag) {
		int rc = poll(fds, 1, 100);
		if (rc == -1)
			break;
		if (rc == 0)
			continue;

		char buffer[2048 + 1];
		int rrc = read(fd, buffer, sizeof buffer - 1);
		if (rrc <= 0)
			break;
		buffer[rrc] = 0x00;

		printf("%s\n", buffer);

		session_buffer += buffer;
		auto term = session_buffer.find("\r\n\r\n");
		if (term == std::string::npos) {
			if (session_buffer.size() > 8192)
				break;
			continue;
		}

		std::string current_request = session_buffer.substr(0, term);
		session_buffer.erase(0, term + 4);

		std::optional<int> cseq;
		std::string reply;
		std::string payload;
		bool setup = false;
		std::string setup_transport;
		auto parts = split(current_request, "\r\n");
		for(auto & line: *parts) {
			if (line.substr(0, 7) == "OPTIONS")
				reply = "RTSP/1.0 200 OK\r\nPublic: DESCRIBE, SETUP, PLAY\r\n";
			else if (line.substr(0, 5) == "CSeq:")
				cseq = std::stoi(line.substr(6));
			else if (line.substr(0, 8) == "DESCRIBE") {
				auto url = line.substr(9);
				auto space = url.find(" ");
				if (space != std::string::npos)
					url = url.substr(0, space);
				payload = gen_sdp_payload_string(session, local_name);
				reply = "RTSP/1.0 200 OK\r\nContent-Base: " + url + "\r\nContent-Type: application/sdp\r\nContent-Length: " + std::to_string(payload.size()) + "\r\n";
			}
			else if (line.substr(0, 5) == "SETUP") {
				setup = true;
			}
			else if (line.substr(0, 10) == "Transport:") {
				setup_transport = line.substr(11);
			}
			else if (line.substr(0, 4) == "PLAY") {
				play = true;
			}
		}

		if (setup) {
			if (setup_transport.empty())
				break;

			auto parts = split(setup_transport, ";");
			for(auto & str : *parts) {
				if (str.substr(0, 12) == "client_port=") {
					auto dash = str.find("-", 12);
					if (dash == std::string::npos)
						break;
					cport1 = std::stoi(str.substr(12, dash));
					cport2 = std::stoi(str.substr(dash + 1));
				}
			}
			delete parts;

			if (cport1 == 0 || cport2 == 0)
				break;

			reply = "RTSP/1.0 200 OK\r\nTransport: RTP/AVP;unicast;client_port=" + myformat("%d-%d", cport1, cport2) + ";server_port=" + myformat("%d-%d", sport1.second, sport2.second) + ";ssrc=" + myformat("%08x", ssrc) + "\r\nSession: " + session + "\r\n";
		}

		if (play) {
			reply = "RTSP/1.0 200 OK\r\nSession: " + session + "\r\n";
		}

		if (reply.empty() == false) {
			if (cseq.has_value())
				reply += myformat("CSeq: %d\r\n", cseq.value());
			reply += "\r\n";
			if (payload.empty() == false)
				reply += payload;
			if (WRITE(fd, reply.c_str(), reply.size()) != reply.size()) {
				delete parts;
				break;
			}
			printf("%s\n", reply.c_str());
		}

		delete parts;

		if (play)
			break;
	}

	if (play) {
		printf("START RTP STREAM\n");
		uint64_t prev_ts = 0;
		const double fps = 1.0 / interval;

		s -> start();

		video_frame *prev_frame = nullptr;

		((sockaddr_in *)&remote_addr)->sin_port = htons(cport1);

		uint32_t seq_nr    = 0;
		uint32_t timestamp = 0;

		// RTP
		while(!local_stop_flag) {
			pauseCheck();
			st->track_fps();

			uint64_t before_ts = get_us();

			video_frame *pvf = s -> get_frame(handle_failure, prev_ts);

			if (pvf) {
				prev_ts = pvf->get_ts();

				if (filters && filters->empty() == false) {
					source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
					instance *inst = find_instance_by_interface(cfg, cur_s);

					video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
					delete pvf;
					pvf = temp;

					delete prev_frame;
					prev_frame = temp->duplicate({ });
				}

				// stream
				if (is_jpeg) {
					if (send_frame_via_jpeg_rtp(pvf, sport1, *(sockaddr_in *)&remote_addr, sizeof remote_addr, ssrc, &seq_nr, &timestamp) == false) {
						delete pvf;
						break;
					}
				}
				else {
					if (send_frame_via_raw_rtp(pvf, sport1, *(sockaddr_in *)&remote_addr, sizeof remote_addr, ssrc, &seq_nr, &timestamp) == false) {
						delete pvf;
						break;
					}
				}
			}

			delete pvf;

			st->track_cpu_usage();

			handle_fps(&local_stop_flag, fps, before_ts);
		}

		delete prev_frame;

		s->stop();
	}

	close(sport2.first);
	close(sport1.first);
	close(fd);
}

void target_rtsp::operator()()
{
	printf("DAAR\n");
	set_thread_name("rtsp_" + prefix);

	listen_adapter_t la { };
	la.adapter = "0.0.0.0";
	la.port = port;
        la.listen_queue_size = 1;
        la.dgram = false;
	int fd = start_listen(la);

	pollfd fds[] { { fd, POLLIN, 0 } };

	while(!local_stop_flag) {
		int rc = poll(fds, 1, 100);
		if (rc == -1)
			break;
		if (rc == 0)
			continue;

		sockaddr addr { };
		socklen_t addr_len { sizeof addr };
		int client_fd = accept(fd, &addr, &addr_len);
		if (client_fd == -1)
			continue;

		std::thread th([&] { 
				rtsp_session(client_fd, addr, addr_len);
				close(client_fd);
				});
		th.detach();
	}

	log(id, LL_WARNING, "stopping rtsp handler");
}
