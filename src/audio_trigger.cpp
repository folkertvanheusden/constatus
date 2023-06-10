// (C) 2017-2023 by folkert van heusden, released under the MIT license

#include <algorithm>

#include "audio_trigger.h"
#include "audio.h"
#include "source.h"
#include "log.h"
#include "utils.h"

audio_trigger::audio_trigger(const std::string & id, const std::string & descr, instance *const i, source *const s, const int threshold, const size_t min_n_triggers, const std::vector<std::string> & trigger_targets) : interface(id, descr), i(i), s(s), a(s->get_audio()), threshold(threshold), min_n_triggers(min_n_triggers), trigger_targets(trigger_targets)
{
}

audio_trigger::~audio_trigger()
{
}

void audio_trigger::register_notifiers()
{
	auto motion_triggers = find_motion_triggers(i);

	for(auto mt : motion_triggers) {
		auto it = std::find(trigger_targets.begin(), trigger_targets.end(), mt->get_id());
		
		if (it != trigger_targets.end()) {
			log(id, LL_INFO, "Will notify %s", mt->get_id().c_str());
			event_clients.push_back(mt);
		}
	}

}

void audio_trigger::unregister_notifiers()
{
	event_clients.clear();

	log(id, LL_ERR, "audio_trigger::unregister_notifier: interface not found");
}

void audio_trigger::operator()()
{
	if (!a) {
		set_error(myformat("Cannot use audio-trigger: %s does not have audio capabilities", s->get_id().c_str()), true);
		return;
	}

	register_notifiers();

	int sample_rate = a->get_samplerate();

	for(;!local_stop_flag;) {
		pauseCheck();

		auto data = a->get_audio_mono(sample_rate / 10);

		int max_ = -1, min_ = 65536, avg = 0;

		size_t n_triggers = 0;
		for(size_t i=0; i<std::get<1>(data); i++) {
			int v = abs(std::get<0>(data)[i]);

			n_triggers += v >= threshold;

			min_ = std::min(min_, v);
			max_ = std::max(max_, v);

			avg += v;
		}

		avg /= std::get<1>(data);

		if (n_triggers >= min_n_triggers) {
			log(id, LL_INFO, "Audio treshold reached (%zu events, min value %d, max value %d, average value %d)", n_triggers, min_, max_, avg);

			for(auto ec : event_clients)
				ec->notify_thread_of_event(s->get_id());
		}
	}

	unregister_notifiers();

	register_thread_end("audio_trigger");
}
