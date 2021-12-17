// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_other.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

source_other::source_other(const std::string & id, const std::string & descr, source *const other, const std::vector<filter *> *const filters) : source(id, descr, "", -1, -1, nullptr, nullptr, 100), other(other)
{
}

source_other::~source_other()
{
	stop();
	delete c;
}

void source_other::operator()()
{
	log(id, LL_INFO, "source other started");

	set_thread_name("src_other");

	other->start();

	bool first = true, resize = resize_h != -1 || resize_w != -1;

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;
	long int backoff = 101000;

	uint64_t after = 0;

	for(;!local_stop_flag;)
	{
		uint64_t start_ts = get_us();

		if (work_required()) {
			video_frame *vf = other->get_frame(true, after);
			after = vf->get_ts();

			if (!is_paused()) {
				auto data = vf->get_data_and_len(E_RGB);

				set_size(vf->get_w(), vf->get_h());
				set_frame(E_RGB, std::get<0>(data), std::get<1>(data));
			}

			delete vf;
		}

		st->track_cpu_usage();

		uint64_t end_ts = get_us();
		int64_t left = interval <= 0 ? 100000 : (interval - std::max(uint64_t(1), end_ts - start_ts));

		if (left > 0)
			myusleep(left);
	}

	other->stop();

	register_thread_end("source other source");
}
