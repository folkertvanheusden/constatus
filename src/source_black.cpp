// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stdio.h>
#include <cstring>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_black.h"
#include "filter_add_text.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

source_black::source_black(const std::string & id, const std::string & descr, const int width, const int height, controls *const c, const std::map<std::string, feed *> & text_feeds) : source(id, descr, "", width, height, nullptr, c, 1, text_feeds, false)
{
}

source_black::~source_black()
{
	delete c;
}

void source_black::operator()()
{
}

video_frame * source_black::get_frame(const bool handle_failure, const uint64_t after)
{
	size_t len = IMS(this->width, this->height, 3);

	uint8_t *p = (uint8_t *)allocate_0x00(len);

	return new video_frame(get_meta(), jpeg_quality, get_us(), width, height, p, len, E_RGB);
}
