// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <cstring>
#include <unistd.h>

#include "source_delay.h"
#include "log.h"
#include "utils.h"
#include "picio.h"
#include "filter.h"
#include "controls.h"

source_delay::source_delay(const std::string & id, const std::string & descr, const std::string & exec_failure, source *const s, const int jpeg_quality, const int n_frames, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int ll, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, ll, timeout, filters, failure, c, jpeg_quality), s(s), n_frames(n_frames), max_fps(max_fps)
{
}

source_delay::~source_delay()
{
	delete c;
}

void source_delay::operator()()
{
	set_thread_name("source_delay");

	log(id, LL_INFO, "source_delay thread started");

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	s -> start();

	for(;!local_stop_flag;)
	{
		uint64_t start_ts = get_us();

		video_frame *f = s -> get_frame(true, start_ts);

		if (f) {
			std::lock_guard lck(frames_lock);

			while(frames.size() >= n_frames) {
				delete prev;
				prev = frames.at(0);

				frames.erase(frames.begin() + 0);
			}

			frames.push_back(f);

			last_ts = f->get_ts();
		}

		if (interval) {
			uint64_t end_ts = get_us();
			int64_t left = interval - (end_ts - start_ts);

			if (left > 0)
				myusleep(left);
		}

		st->track_cpu_usage();
	}

	for(auto f : frames)
		delete f;

	delete prev;
	prev = nullptr;

	s -> stop();

	register_thread_end("source_delay");
}

video_frame * source_delay::get_frame(const bool handle_failure, const uint64_t after)
{
	std::shared_lock lck(frames_lock);

	if (frames.empty()) {
		lck.unlock();

		do_exec_failure();

		return nullptr;
	}

	video_frame *vf = nullptr;

	std::shared_lock clck(controls_lock);
	bool filtering = (filters && !filters->empty()) || (c != nullptr && c->requires_apply());

	if (filtering) {
		vf = frames.at(0)->apply_filtering(nullptr, this, prev, filters, c);
		lck.unlock();

		if (c != nullptr && c->requires_apply()) {
			c->apply(vf->get_data(E_RGB), vf->get_w(), vf->get_h());
			vf->keep_only_format(E_RGB);
		}
	}
	else {
		vf = frames.at(0)->duplicate({ });
	}
	clck.unlock();

	vf->set_ts(last_ts);

	return vf;
}

uint64_t source_delay::get_current_ts() const
{
	std::shared_lock lck(frames_lock);

	return last_ts;
}
