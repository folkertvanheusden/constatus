// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <stddef.h>
#include <cstring>
#include <sys/time.h>

#include "gen.h"
#include "view_ss.h"
#include "http_server.h"
#include "utils.h"
#include "picio.h"
#include "log.h"
#include "target.h"
#include "filter.h"
#include "db.h"
#include "resize.h"

view_ss::view_ss(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const bool auto_shrink, const std::vector<view_src_t> & sources, const double switch_interval, std::vector<filter *> *const filters, const int jpeg_quality) : view(cfg, id, descr, width, height, sources, filters, jpeg_quality), auto_shrink(auto_shrink), switch_interval(switch_interval)
{
	cur_source_index = 0;
        latest_switch = 0;
	cur_source = this;
}

view_ss::~view_ss()
{
	get_db()->register_event_end(event_nr);

	delete prev_frame;
}

void view_ss::operator()()
{
}

std::string view_ss::get_html(const std::map<std::string, std::string> & pars) const
{
	return "";
}

void view_ss::start()
{
	for(auto s : sources) {
		instance *inst = nullptr;
		source *source = nullptr;
		find_by_id(cfg, s.id, &inst, (interface **)&source);

		if (source)
			source -> start();
	}

	interface::start();
}

void view_ss::stop()
{
	log(id, LL_INFO, "view_ss::stop");

	for(auto s : sources) {
		instance *inst = nullptr;
		source *source = nullptr;
		find_by_id(cfg, s.id, &inst, (interface **)&source);

		if (source)
			source -> stop();
	}

	interface::stop();
}

source *view_ss::get_current_source()
{
	return cur_source;
}

video_frame * view_ss::get_frame(const bool handle_failure, const uint64_t after)
{
	instance *inst = nullptr;
	find_by_id(cfg, sources.at(cur_source_index).id, &inst, (interface **)&cur_source);

	uint64_t si = switch_interval * 1000000.0;
	if (switch_interval < 1.0)
		si = 1000000;

	if (latest_switch == 0)
		latest_switch = get_us();

	uint64_t now = get_us();
	if (now - latest_switch >= si) {
		for(size_t i=0; i<sources.size(); i++) {
			if (++cur_source_index >= sources.size())
				cur_source_index = 0;

			instance *next_inst = nullptr;
			interface *next_source = nullptr;
			find_by_id(cfg, sources.at(cur_source_index).id, &next_inst, &next_source);

			if (!next_source)
				log(id, LL_ERR, "source selection %s in view %s is not known", sources.at(cur_source_index).id.c_str(), id.c_str());
			else if (next_source->is_paused() == false && next_source->is_running() == true)
				break;
		}

		latest_switch = now;
	}

	if (!cur_source) {
		log(id, LL_INFO, "no source found?!");
		return { };
	}

	video_frame *pvf = cur_source->get_frame(handle_failure, after);

	if (!pvf) {
		log(id, LL_INFO, "source \"%s\" returned no image", cur_source->get_id().c_str());
		return { };
	}

	if (width != pvf->get_w() || height != pvf->get_h()) {
		video_frame *temp = pvf->do_resize(r, width, height);
		delete pvf;
		pvf = temp;
	}

	if (filters && !filters->empty()) {
		instance *inst = find_instance_by_interface(cfg, cur_source);

		video_frame *temp = pvf->apply_filtering(inst, cur_source, prev_frame, filters, nullptr);
		delete pvf;
		pvf = temp;

		delete prev_frame;
		prev_frame = pvf->duplicate(E_RGB);
	}

	return pvf;
}
