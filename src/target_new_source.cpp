// (C) 2017-2023 by folkert van heusden, released under the MIT license
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

target_new_source::target_new_source(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const std::string & new_s_id, const std::string & new_s_descr, schedule *const sched, const bool rot90, const std::map<std::string, feed *> & text_feeds) : target(id, descr, s, "", "", "", -1, interval, filters, "", "", "", -1, cfg, false, false, sched), new_s_id(new_s_id), new_s_descr(new_s_descr), rot90(rot90), text_feeds(text_feeds)
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

		new_source = new source(new_s_id, new_s_descr, "", -1.0, nullptr, -1, -1, LL_DEBUG, 0.1, nullptr, default_failure(), new controls(), 100, text_feeds, false);
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

				const bool allow_store = sched == nullptr || sched->is_on();

				if (allow_store) {
					auto img = pvf->get_data_and_len(E_RGB);
					const uint8_t *data = std::get<0>(img);
					const size_t len = std::get<1>(img);
					const int w = pvf->get_w();
					const int h = pvf->get_h();

					if (rot90) {
						// rotate
						uint8_t *new_ = (uint8_t *)malloc(len);

						for(int y=0; y<h; y++) {
							for(int x=0; x<w; x++) {
								new_[x * h * 3 + y * 3 + 0] = data[y * w * 3 + x * 3 + 0];
								new_[x * h * 3 + y * 3 + 1] = data[y * w * 3 + x * 3 + 1];
								new_[x * h * 3 + y * 3 + 2] = data[y * w * 3 + x * 3 + 2];
							}
						}

						new_source->set_size(h, w);
						new_source->set_frame(E_RGB, new_, len);

						free(new_);
					}
					else {
						new_source->set_size(w, h);
						new_source->set_frame(E_RGB, data, len);
					}
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
