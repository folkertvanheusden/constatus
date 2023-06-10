// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <cstring>
#include <stddef.h>
#include "gen.h"
#include "view_pip.h"
#include "http_server.h"
#include "utils.h"
#include "picio.h"
#include "log.h"
#include "filter.h"
#include "resize.h"

view_pip::view_pip(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, std::vector<filter *> *const filters, const int jpeg_quality) : view_ss(cfg, id, descr, width, height, false, sources, 0.0, filters, jpeg_quality)
{
	frames.resize(sources.size());
}

view_pip::~view_pip()
{
	delete prev_frame;

	for(auto f : frames)
		delete f;
}

std::string view_pip::get_html(const std::map<std::string, std::string> & pars) const
{
	return "";
}

void view_pip::operator()()
{
        log(id, LL_INFO, "Starting view_pip thread");

	view_ss::operator()();

	uint64_t after_ts = get_us();

	new_ts = 0;

	size_t i = 0;
	while(!local_stop_flag) {
		instance *main_inst = nullptr;
		source *main_source = nullptr;
		find_by_id(cfg, sources.at(i).id, &main_inst, (interface **)&main_source);

		if (!main_source)
			error_exit(false, "Source %s not found", sources.at(i).id.c_str());

		if (main_source->get_class_type() != CT_SOURCE)
			error_exit(false, "Object with id \"%s\" is not of type source", sources.at(i).id.c_str());

		video_frame *pvf = main_source -> get_frame_to(true, after_ts, 100000);

		if (pvf) {
			if (i) {
				int perc = sources.at(i).perc;
				video_frame *temp = pvf->do_resize(r, pvf->get_w() * perc / 100, pvf->get_h() * perc / 100);
				delete pvf;
				pvf = temp;
			}

			const std::lock_guard<std::mutex> lck(frames_lock);

			delete frames.at(i);
			frames.at(i) = pvf;

			if (new_ts <= after_ts)
				new_ts = pvf->get_ts();

			frames_cv.notify_all();
		}

		i++;

		if (i == frames.size()) {
			i = 0;
			const std::lock_guard<std::mutex> lck(frames_lock);
			after_ts = new_ts;
		}
	}

        log(id, LL_INFO, "Stopping view_pip thread");
}

video_frame * view_pip::get_frame(const bool handle_failure, const uint64_t after)
{
	std::unique_lock<std::mutex> lck(frames_lock);

	while(new_ts <= after && !local_stop_flag)
		frames_cv.wait(lck);

	int lsf = local_stop_flag;

	if (!frames.at(0))
		return nullptr;

	video_frame *main_frame = frames.at(0)->duplicate(E_RGB);

	int main_width = main_frame->get_w();
	int main_height = main_frame->get_h();

	uint8_t *work = main_frame->get_data(E_RGB);

	for(size_t i=1; i<sources.size(); i++) {
		video_frame *pip = frames.at(i);

		if (pip == nullptr)
			continue;

		picture_in_picture(work, main_width, main_height, pip->get_data(E_RGB), pip->get_w(), pip->get_h(), sources.at(i).pos);
	}

	lck.unlock();

	if (filters && !filters->empty()) {
		instance *inst = nullptr; //find_instance_by_interface(cfg, main_source);

		const std::lock_guard<std::mutex> pflck(prev_frame_lock);

		video_frame *temp = main_frame->apply_filtering(inst, nullptr, prev_frame, filters, nullptr);

		delete main_frame;
		main_frame = temp;

		delete prev_frame;
		prev_frame = temp->duplicate(E_RGB);
	}

	return main_frame;
}
