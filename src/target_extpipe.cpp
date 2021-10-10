// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <errno.h>
#include <signal.h>
#include <cstring>
#include <unistd.h>

#include "target_extpipe.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "filter.h"
#include "source.h"
#include "view.h"
#include "schedule.h"

target_extpipe::target_extpipe(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const std::string & cmd, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : target(id, descr, s, store_path, prefix, fmt, max_time, interval, filters, exec_start, exec_cycle, exec_end, -1.0, cfg, is_view_proxy, handle_failure, sched), quality(quality), cmd(cmd)
{
	if (this -> descr == "")
		this -> descr = store_path + "/" + prefix;

	th = myjpeg::allocate_transformer();
}

target_extpipe::~target_extpipe()
{
	stop();

	myjpeg::free_transformer(th);
}

static void put_frame(transformer_t th, FILE *fh, const uint8_t *const in, const int w, const int h)
{
	uint8_t *temp { nullptr };
	my_jpeg.rgb_to_i420(th, in, w, h, &temp);

	size_t n = w * h + w * h / 2;
	fwrite(temp, 1, n, fh);

	free(temp);
}

void target_extpipe::store_frame(video_frame *const put_f, FILE *const p_fd)
{
	put_frame(th, p_fd, put_f->get_data(E_RGB), put_f->get_w(), put_f->get_h());
}

void target_extpipe::operator()()
{
	set_thread_name("storee_" + prefix);

	s -> start();

	const double fps = 1.0 / interval;

	time_t cut_ts = time(nullptr) + max_time;

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;
	unsigned f_nr = 0;

	FILE *p_fd = nullptr;

	video_frame *prev_frame = nullptr;

	bool first = true;
	std::string workCmd = cmd;

	for(;!local_stop_flag;) {
		pauseCheck();
		st->track_fps();

		uint64_t before_ts = get_us();

		const bool allow_store = sched == nullptr || (sched && sched->is_on());

		video_frame *pvf = s -> get_frame(handle_failure, prev_ts);

		if (pvf) {
			prev_ts = pvf->get_ts();

			if (max_time > 0 && time(nullptr) >= cut_ts) {
				log(id, LL_DEBUG, "new file");

				if (p_fd) {
					pclose(p_fd);
					p_fd = nullptr;
				}

				cut_ts = time(nullptr) + max_time;
			}

			if (p_fd == nullptr) {
				name = gen_filename(s, fmt, store_path, prefix, "", get_us(), f_nr++);
				create_path(name);
				register_file(name);

				if (!exec_start.empty() && is_start) {
					if (check_thread(&exec_start_th))
						exec_start_th = exec(exec_start, name);

					is_start = false;
				}
				else if (!exec_cycle.empty()) {
					if (check_thread(&exec_end_th))
						exec_end_th = exec(exec_cycle, name);
				}

				int fps = interval <= 0 ? 25 : std::max(1, int(1.0 / interval));

				if (first) {
					first = false;

					char fps_str[16];
					snprintf(fps_str, sizeof fps_str, "%d", fps);

					workCmd = search_replace(workCmd, "%fps", fps_str);

					char w_str[16], h_str[16];
					snprintf(w_str, sizeof w_str, "%d", pvf->get_w());
					snprintf(h_str, sizeof h_str, "%d", pvf->get_h());
					workCmd = search_replace(workCmd, "%w", w_str);
					workCmd = search_replace(workCmd, "%h", h_str);

					workCmd = search_replace(workCmd, "%f", name);

					log(id, LL_DEBUG, "Will invoke %s", workCmd.c_str());
				}

				p_fd = exec(workCmd);
			}

			log(id, LL_DEBUG_VERBOSE, "Write frame");

			if (filters && !filters->empty()) {
				source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
				instance *inst = find_instance_by_interface(cfg, cur_s);

				video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
				delete pvf;
				pvf = temp;
			}

			// put one
			pre_record.push_back(pvf);

			// get one
			video_frame *put_f = pre_record.front();
			pre_record.erase(pre_record.begin());

			if (allow_store)
				store_frame(put_f, p_fd);

			prev_frame = put_f;
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	delete prev_frame;

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, name);

		join_thread(&exec_end_th, id, "exec-end");
	}

	const bool allow_store = sched == nullptr || (sched && sched->is_on());

	for(auto f : pre_record) {
		if (p_fd && allow_store)
			store_frame(f, p_fd);

		delete f;
	}

	pre_record.clear();

	if (p_fd != nullptr)
		pclose(p_fd);

	s -> stop();
}
