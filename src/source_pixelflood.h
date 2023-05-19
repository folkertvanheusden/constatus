// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "source.h"
#include "pixelflood.h"
#include "utils.h"

class source_pixelflood : public source
{
private:
	pthread_t th_client;
	const listen_adapter_t la;
	const int pixel_size;
	const pixelflood_protocol_t pp;

	uint8_t *frame_buffer;

public:
	source_pixelflood(const std::string & id, const std::string & descr, const std::string & exec_failure, const listen_adapter_t & la, const int pixel_size, const pixelflood_protocol_t pp, const double max_fps, const int w, const int h, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio);
	~source_pixelflood();

	void operator()() override;
};
