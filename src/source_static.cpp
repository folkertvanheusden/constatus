// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stdio.h>
#include <cstring>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_static.h"
#include "filter_add_text.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

source_static::source_static(const std::string & id, const std::string & descr, const int width, const int height, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, "", width, height, nullptr, c, jpeg_quality, text_feeds, keep_aspectratio)
{
}

source_static::~source_static()
{
	delete c;
}

void source_static::operator()()
{
}

video_frame * source_static::get_frame(const bool handle_failure, const uint64_t after)
{
	size_t len = IMS(this->width, this->height, 3);
	uint8_t *p = (uint8_t *)malloc(len);

	struct timespec tv;
        clock_gettime(CLOCK_REALTIME, &tv);
        uint64_t ts = uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);

	memset(p, 0x20, len);

	filter_add_text fat1("Hello, world!", { center_center, -1, -1 }, text_feeds);
	fat1.apply(nullptr, this, ts, this->width, this->height, nullptr, p);

	filter_add_text fat2("%c", { upper_center, -1, -1 }, text_feeds);
	fat2.apply(nullptr, this, ts, this->width, this->height, nullptr, p);

	return new video_frame(get_meta(), jpeg_quality, ts, this->width, this->height, p, len, E_RGB);
}
