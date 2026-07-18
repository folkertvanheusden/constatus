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

target_rtsp::target_rtsp(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int port, const int quality, const bool handle_failure, schedule *const sched) :
	target(id, descr, s, "", "", "", max_time, interval, filters, "", "", "", override_fps, cfg, false, handle_failure, sched),
	quality(quality),
	port(port)
{
	printf("HIER\n");
}

target_rtsp::~target_rtsp()
{
	stop();
}

std::string target_rtsp::gen_payload_string()
{
	return  "m=video 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 JPEG/90000\r\n";
}

void target_rtsp::rtsp_session(const int fd)
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
				payload = gen_payload_string();
				reply = "RTSP/1.0 200 OK\r\nContent-Base: " + url + "\r\nContent-Type: application/sdp\r\nContent-Length: " + std::to_string(payload.size()) + "\r\n";
			}
			else if (line.substr(0, 5) == "SETUP") {
				setup = true;
			}
			else if (line.substr(0, 10) == "Transport:") {
				setup_transport = line.substr(11);
			}
		}

		if (setup) {
			if (setup_transport.empty())
				break;

			int port1 = 0;
			int port2 = 0;

			auto parts = split(setup_transport, ";");
			for(auto & str : *parts) {
				if (str.substr(0, 12) == "client_port=") {
					auto dash = str.find("-", 12);
					if (dash == std::string::npos)
						break;
					port1 = std::stoi(str.substr(12, dash));
					port2 = std::stoi(str.substr(dash + 1));
				}
			}
			delete parts;

			if (port1 == 0 || port2 == 0)
				break;

			reply = "RTSP/1.0 200 OK\r\nTransport: RTP/AVP;unicast;client_port=" + myformat("%d-%d", port1, port2) + ";server_port=" + myformat("%d-%d", sport1.second, sport2.second) + ";ssrc=" + myformat("%08x", ssrc) + "\r\nSession: " + session + "\r\n";
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
	}

	close(sport2.first);
	close(sport1.first);
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

		int client_fd = accept(fd, nullptr, nullptr);
		if (client_fd == -1)
			continue;

		std::thread th([&] { 
				rtsp_session(client_fd);
				close(client_fd);
				});
		th.detach();
	}

	log(id, LL_WARNING, "stopping rtsp handler");
}
