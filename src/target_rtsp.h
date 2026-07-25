// (C) 2026 by folkert van heusden, released under the MIT license
#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "target.h"

class schedule;

class target_rtsp : public target
{
private:
	const int  quality    {  85   };
	const int  port       { 554   };
	const bool is_jpeg    { false };
	const bool follow_rfc { true  };

	bool        send_frame_via_jpeg_rtp(video_frame *const pvf,
			const std::pair<int, int> local_fd_port, const sockaddr_in remote, const socklen_t remote_len,
			const uint32_t ssrc, uint32_t *const seq_nr, uint32_t *const timestamp);
	bool        send_frame_via_raw_rtp(video_frame *const pvf,
			const std::pair<int, int> local_fd_port, const sockaddr_in remote, const socklen_t remote_len,
			const uint32_t ssrc, uint32_t *const seq_nr, uint32_t *const timestamp);
	std::string gen_sdp_payload_string(const std::string & session, const std::string & local_ip_addr);
	void        rtp_stream  (const std::pair<int, int> & sport1, const int cport1, const uint32_t ssrc,
			sockaddr remote_addr, socklen_t remote_addr_len, std::atomic_bool *const rtp_stop_flag);
	void        rtsp_session(const int fd, sockaddr remote_addr, socklen_t remote_addr_len);

public:
	target_rtsp(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int port, const int quality, const bool handle_failure, schedule *const sched, const bool is_jpeg, const bool follow_rfc);
	virtual ~target_rtsp();

	void operator()() override;
};
