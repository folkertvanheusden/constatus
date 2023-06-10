// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <unistd.h>

#include "target_jpeg.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "schedule.h"

target_jpeg::target_jpeg(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : target(id, descr, s, store_path, prefix, fmt, max_time, interval, filters, exec_start, exec_cycle, exec_end, -1, cfg, is_view_proxy, handle_failure, sched), quality(quality)
{
	if (this -> descr == "")
		this -> descr = store_path + "/" + prefix;
}

target_jpeg::~target_jpeg()
{
	stop();
}

std::string target_jpeg::write_frame(video_frame *put_f)
{
	std::string name = gen_filename(s, fmt, store_path, prefix, "jpg", get_us(), f_nr++);
	create_path(name);

	if (!exec_start.empty() && is_start) {
		if (check_thread(&exec_start_th))
			exec_start_th = exec(exec_start, name);

		is_start = false;
	}

	log(id, LL_DEBUG_VERBOSE, "Write frame to %s", name.c_str());
	FILE *fh = fopen(name.c_str(), "wb");
	if (!fh)
		error_exit(true, "Cannot create file %s", name.c_str());

	auto img = put_f->get_data_and_len(E_JPEG);

	fwrite(std::get<0>(img), std::get<1>(img), 1, fh);

	delete put_f;

	fclose(fh);

	return name;
}

void target_jpeg::operator()()
{
	set_thread_name("storej_" + prefix);

	uint64_t prev_ts = 0;
	std::string name;

	const double fps = 1.0 / interval;

	f_nr = 0;
	is_start = true;

	s -> start();

	video_frame *prev_frame = nullptr;

	for(;!local_stop_flag;) {
		pauseCheck();
		st->track_fps();

		uint64_t before_ts = get_us();

		video_frame *pvf = s -> get_frame(handle_failure, prev_ts);

		if (pvf) {
			prev_ts = pvf->get_ts();

			if (!filters || filters -> empty()) {
				pre_record.push_back(pvf);
			}
			else {
				source *cur_s = is_view_proxy ? ((view *)s) -> get_current_source() : s;
				instance *inst = find_instance_by_interface(cfg, cur_s);

                                video_frame *temp = pvf->apply_filtering(inst, cur_s, prev_frame, filters, nullptr);
				pre_record.push_back(temp);

				delete prev_frame;
				prev_frame = temp->duplicate({ });
			}

			video_frame * put_f = pre_record.front();
			pre_record.erase(pre_record.begin());

			const bool allow_store = sched == nullptr || (sched && sched->is_on());

			if (allow_store)
				write_frame(put_f);
		}

		st->track_cpu_usage();

		handle_fps(&local_stop_flag, fps, before_ts);
	}

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, name);

		join_thread(&exec_end_th, id, "exec-end");
	}

	const bool allow_store = sched == nullptr || (sched && sched->is_on());

	for(auto f : pre_record) {
		if (allow_store)
			write_frame(f);
		else
			delete f;
	}

	pre_record.clear();

	delete prev_frame;

	s -> stop();
}
