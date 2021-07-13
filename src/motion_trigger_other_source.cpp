// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <atomic>
#include <chrono>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/time.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "picio.h"
#include "target.h"
#include "filter.h"
#include "log.h"
#include "exec.h"
#include "parameters.h"
#include "motion_trigger.h"
#include "motion_trigger_other_source.h"
#include "schedule.h"

motion_trigger_other_source::motion_trigger_other_source(const std::string & id, const std::string & descr, source *const hr_s, instance *const lr_motion_trigger, std::vector<target *> *const targets, const std::map<std::string, parameter *> & detection_parameters, schedule *const sched) : motion_trigger(id, descr, detection_parameters, targets, sched), hr_s(hr_s), lr_motion_trigger(lr_motion_trigger)
{
	ct = CT_MOTIONTRIGGER;

	for(auto mi : find_motion_triggers(lr_motion_trigger))
		static_cast<motion_trigger *>(mi)->register_notifier(this);
}

motion_trigger_other_source::~motion_trigger_other_source()
{
	for(auto mi : find_motion_triggers(lr_motion_trigger))
		static_cast<motion_trigger *>(mi)->unregister_notifier(this);

	stop();

	for(target *t : *targets)
		delete t;
	delete targets;
}

void motion_trigger_other_source::operator()()
{
	set_thread_name("m-otr-src");

	hr_s -> start();

	bool recording = false;

	std::vector<video_frame *> prerecord;

	uint64_t prev_ts = get_us();

	for(;!local_stop_flag;) {
		bool allow_trigger = sched == nullptr || (sched && sched->is_on());

		size_t pre_duration = parameter::get_value_int(parameters, "pre-motion-record-duration");

		st->track_fps();

		if (pre_duration > 0) {
			video_frame *f = hr_s -> get_frame(true, prev_ts);
			if (!f)
				continue;

			if (prerecord.size() >= pre_duration) {
				delete prerecord.at(0);

				prerecord.erase(prerecord.begin() + 0);
			}

			prerecord.push_back(f);

			if (cv_event_notified.exchange(false) && !recording && allow_trigger) {
				recording = true;

				log(id, LL_INFO, "starting (%zu pre-record frames)", prerecord.size());

				std::vector<video_frame *> empty;

				for(size_t i=0; i<targets -> size(); i++) {
					targets -> at(i) -> set_on_demand(true);

					targets -> at(i) -> start(i == 0 ? prerecord : empty, -1);
				}

				recording = true;

				prerecord.clear();
			}
		}
		// when not recording any frames before the motion starts, we can just sit idle and
		// wait for a signal from the linked source
		else {
			std::unique_lock<std::mutex> lk(m_event);

			using namespace std::chrono_literals;

			if (cv_event.wait_for(lk, 100ms) == std::cv_status::no_timeout) {
				lk.unlock();

				if (!recording && allow_trigger) {
					log(id, LL_INFO, "starting");

					std::vector<video_frame *> empty;
					for(size_t i=0; i<targets -> size(); i++) {
						targets -> at(i) -> set_on_demand(true);

						targets -> at(i) -> start(empty, -1);
					}

					recording = true;
				}
			}
		}

		if (recording) {
			bool still_recording = false;

			for(auto mi : find_motion_triggers(lr_motion_trigger))
				still_recording |= static_cast<motion_trigger *>(mi)->check_motion();

			if (!still_recording) {
				log(id, LL_INFO, "stopping recording");

				for(target *t : *targets)
					t -> stop();

				recording = false;
			}
		}

		st->track_cpu_usage();
	}

	hr_s -> stop();

	while(!prerecord.empty()) {
		delete prerecord.at(0);

		prerecord.erase(prerecord.begin() + 0);
	}

	register_thread_end("motion_trigger_other_source");
}
