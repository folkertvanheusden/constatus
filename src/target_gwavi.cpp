// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"

#if HAVE_LIBGWAVI == 1
#include <unistd.h>

#include "target_gwavi.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "filter.h"
#include "schedule.h"

target_gwavi::target_gwavi(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : target(id, descr, s, store_path, prefix, fmt, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps, cfg, is_view_proxy, handle_failure, sched), quality(quality)
{
}

target_gwavi::~target_gwavi()
{
	stop();
}

void target_gwavi::put_frame(video_frame *const f)
{
	auto frame = f->get_data_and_len(E_JPEG);

	gwavi_add_frame(gwavi, (unsigned char *)std::get<0>(frame), std::get<1>(frame));
}

void target_gwavi::open_file()
{
	name = gen_filename(s, fmt, store_path, prefix, "", get_us(), f_nr++);

	gwavi = gwavi_open((char *)name.c_str(), s->get_width(), s->get_height(), (char *)"MJPG", override_fps >= 1 ? override_fps : fps, NULL);
	if (!gwavi)
		error_exit(true, "Cannot create %s", name.c_str());
}

void target_gwavi::close_file()
{
	if (gwavi) {
		gwavi_close(gwavi);

		gwavi = nullptr;
	}

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, name);

		join_thread(&exec_end_th, id, "exec-end");
	}
}

void target_gwavi::operator()()
{
	log(id, LL_INFO, "\"target-gwavi\" thread started");

	set_thread_name("tgt-gwavi");

	video_frame *prev_frame = nullptr;
	uint64_t ts = 0;

	s->start();

	fps = interval > 0 ? 1.0 / interval : 25;

	time_t next_file = max_time > 0 ? time(nullptr) + max_time : 0;

        for(;!local_stop_flag;) {
                pauseCheck();
		st->track_fps();

                uint64_t before_ts = get_us();

		const bool allow_store = sched == nullptr || (sched && sched->is_on());

		video_frame *pvf = s->get_frame(false, ts);

		if (pvf) {
			ts = pvf->get_ts();

			if (filters && !filters->empty()) {
				instance *inst = find_instance_by_interface(cfg, s);

				video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;

				delete prev_frame;
				prev_frame = pvf->duplicate(E_RGB);
			}

			pre_record.push_back(pvf);

                        // get one
                        video_frame *put_f = pre_record.front();
                        pre_record.erase(pre_record.begin());

			time_t now = time(nullptr);
			if ((now >= next_file && next_file != 0 && gwavi) || allow_store == false) {
				close_file();

				next_file = now + max_time;
			}

			if (gwavi == nullptr)
				open_file();

			if (allow_store)
				put_frame(put_f);

			delete put_f;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;

	s->stop();

	if (pre_record.empty()) {
		if (gwavi)
			close_file();
	}
	else {
		log(id, LL_INFO, "Pushing buffers to avi file...");

		if (!gwavi)
			open_file();

		const bool allow_store = sched == nullptr || (sched && sched->is_on());

		for(auto f : pre_record) {
			if (allow_store)
				put_frame(f);

			delete f;
		}

		pre_record.clear();

		close_file();
	}

	log(id, LL_INFO, "\"target-gwavi\" thread terminating");
}
#endif
