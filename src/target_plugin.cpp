// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <unistd.h>

#include "target_plugin.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "schedule.h"

target_plugin::target_plugin(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, stream_plugin_t *const sp, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : target(id, descr, s, store_path, prefix, fmt, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps, cfg, is_view_proxy, handle_failure, sched), quality(quality), sp(sp)
{
	if (this -> descr == "")
		this -> descr = store_path + "/" + prefix;
}

target_plugin::~target_plugin()
{
	stop();
}

void target_plugin::open_plugin()
{
	if (!is_open) {
		double fps = interval <= 0 ? 25.0 : (1.0 / interval);

		sp -> open_file(sp -> arg, (store_path + prefix).c_str(), override_fps > 0 ? override_fps : fps, quality);
		is_open = true;

		if (!exec_start.empty() && is_start) {
			if (check_thread(&exec_start_th))
				exec_start_th = exec(exec_start, "");

			is_start = false;
		}
		else if (!exec_cycle.empty()) {
			if (check_thread(&exec_end_th))
				exec_end_th = exec(exec_cycle, "");
		}
	}
}

void target_plugin::operator()()
{
	set_thread_name("storep_" + prefix);

	const double fps = 1.0 / interval;

	sp -> arg = sp -> init_plugin(sp -> par.c_str());

	s -> start();

	time_t cut_ts = time(nullptr) + max_time;

	uint64_t prev_ts = 0;
	is_start = true;
	is_open = false;

	video_frame *prev_frame = nullptr;

	for(;!local_stop_flag;) {
		pauseCheck();
		st->track_fps();

		uint64_t before_ts = get_us();

		video_frame *pvf = s->get_frame(handle_failure, prev_ts);

		if (pvf) {
			prev_ts = pvf->get_ts();

			if (max_time > 0 && time(nullptr) >= cut_ts) {
				log(id, LL_DEBUG, "new file");

				sp -> close_file(sp -> arg);
				is_open = false;

				cut_ts = time(nullptr) + max_time;
			}

			open_plugin();

			if (filters && !filters->empty()) {
				source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
				instance *inst = find_instance_by_interface(cfg, cur_s);

				video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;
			}

			// put
			pre_record.push_back(pvf);

			// get
                        video_frame *put_f = pre_record.front();
			pre_record.erase(pre_record.begin());

			const bool allow_store = sched == nullptr || (sched && sched->is_on());

			if (allow_store)
				sp->write_frame(sp->arg, prev_ts, put_f->get_w(), put_f->get_h(), prev_frame ? prev_frame->get_data(E_RGB) : nullptr, pvf->get_data(E_RGB));

			delete prev_frame;
			prev_frame = pvf;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	open_plugin();

	if (is_open) {
		const bool allow_store = sched == nullptr || (sched && sched->is_on());

		for(auto f : pre_record) {
			if (allow_store)
				sp -> write_frame(sp -> arg, f->get_ts(), f->get_w(), f->get_h(), prev_frame->get_data(E_RGB), f->get_data(E_RGB));

			delete f;
		}

		pre_record.clear();

		sp->close_file(sp -> arg);
	}

	delete prev_frame;

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, "");

		join_thread(&exec_end_th, id, "exec-end");
	}

	sp -> uninit_plugin(sp -> arg);

	s -> stop();
}
