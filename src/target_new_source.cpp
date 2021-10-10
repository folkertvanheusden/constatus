// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <unistd.h>

#include "target_new_source.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "controls.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "schedule.h"

target_new_source::target_new_source(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const std::string & new_s_id, const std::string & new_s_descr, schedule *const sched) : target(id, descr, s, "", "", "", -1, interval, filters, "", "", "", -1, cfg, false, false, sched), new_s_id(new_s_id), new_s_descr(new_s_descr)
{
}

target_new_source::~target_new_source()
{
	stop();
}

source * target_new_source::get_new_source()
{
	new_source_lock.lock();

	if (!new_source) {
		log(id, LL_INFO, "Instantiating new source %s", new_s_id.c_str());

		new_source = new source(new_s_id, new_s_descr, "", -1.0, nullptr, -1, -1, LL_DEBUG, 0.1, nullptr, default_failure(), new controls(), 100);
	}

	new_source_lock.unlock();

	return new_source;
}

void target_new_source::operator()()
{
	log(id, LL_INFO, "\"as-a-new-source\" thread started");

	set_thread_name("newsrc" + new_s_id);

	const double fps = 1.0 / interval;

	video_frame *prev_frame = nullptr;
	uint64_t prev_ts = 0;

	s -> start();

        for(;!local_stop_flag;) {
                pauseCheck();
		st->track_fps();

                uint64_t before_ts = get_us();

		if (new_source) {
			video_frame *pvf = s->get_frame(false, prev_ts);

			if (pvf) {
				prev_ts = pvf->get_ts();

				if (filters && !filters->empty()) {
					instance *inst = find_instance_by_interface(cfg, s);

					video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
					delete pvf;
					pvf = temp;
				}

				const bool allow_store = sched == nullptr || (sched && sched->is_on());

				if (allow_store) {
					auto img = pvf->get_data_and_len(E_JPEG);
					new_source->set_frame(E_RGB, std::get<0>(img), std::get<1>(img));
				}

				delete prev_frame;
				prev_frame = pvf;
			}
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;

	s -> stop();

	log(id, LL_INFO, "\"as-a-new-source\" thread terminating");
}
