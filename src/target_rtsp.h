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
	const int quality {  85 };
	const int port    { 554 };

	std::string gen_sdp_payload_string();
	void        rtsp_session(const int fd);

public:
	target_rtsp(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int port, const int quality, const bool handle_failure, schedule *const sched);
	virtual ~target_rtsp();

	void operator()() override;
};
