// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stdio.h>
#include <string.h>
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

void source_other::crop(video_frame *const in, video_frame **const out, const cut_t & cut)
{
	auto data = in->get_data_and_len(E_RGB);

	size_t bytes = cut.w * cut.h * 3;
	uint8_t *temp = (uint8_t *)malloc(bytes);

	for(int y=0; y<cut.h; y++)
		memcpy(&temp[y * cut.w * 3], &std::get<0>(data)[y * in->get_w() * 3 + cut.x * 3], cut.w * 3);

	*out = new video_frame(get_meta(), jpeg_quality, in->get_ts(), cut.w, cut.h, temp, bytes, E_RGB);
}

source_other::source_other(const std::string & id, const std::string & descr, source *const other, const std::string & exec_failure, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, resize *const r, const int resize_w, const int resize_h, const std::optional<cut_t> & cut) : source(id, descr, exec_failure, -1, r, resize_w, resize_h, loglevel, 86400, filters, failure, c, jpeg_quality), other(other), cut(cut)
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
				if (cut.has_value()) {
					video_frame *vf_new = nullptr;

					crop(vf, &vf_new, cut.value());

					set_size(vf_new->get_w(), vf_new->get_h());

					auto data = vf_new->get_data_and_len(E_RGB);

					set_frame(E_RGB, std::get<0>(data), std::get<1>(data));

					delete vf_new;
				}
				else {
					auto data = vf->get_data_and_len(E_RGB);

					set_size(vf->get_w(), vf->get_h());

					if (resize_w != -1 && resize_h != -1)
						set_scaled_frame(std::get<0>(data), vf->get_w(), vf->get_h());
					else
						set_frame(E_RGB, std::get<0>(data), std::get<1>(data));
				}
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
