// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "target.h"
#include "pixelflood.h"

class schedule;

class target_pixelflood : public target
{
private:
	const int quality;
	const std::string host;
	const int port;
	const int pw, ph;
	const pixelflood_protocol_t pp;
	const int xoff, yoff;

	int fd;
	struct sockaddr_in servaddr { 0 };

	bool send_frame(const uint8_t *const data, const int w, const int h);

public:
	target_pixelflood(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const std::string & ip_addr, const int port, const int pfw, const int pfh, const int quality, const pixelflood_protocol_t pp, const int xoff, const int yoff, const bool handle_failure, schedule *const sched);
	virtual ~target_pixelflood();

	void operator()() override;
};
