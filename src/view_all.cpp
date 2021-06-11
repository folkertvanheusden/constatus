// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <cstring>
#include "gen.h"
#include "view_all.h"
#include "http_server.h"
#include "utils.h"
#include "picio.h"
#include "log.h"
#include "resize.h"
#include "filter.h"

view_all::view_all(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const int gwidth, const int gheight, const std::vector<view_src_t> & sources, const double switch_interval, std::vector<filter *> *const filters, const int jpeg_quality) : view_ss(cfg, id, descr, width, height, false, sources, switch_interval, filters, jpeg_quality), grid_width(gwidth), grid_height(gheight)
{
	ts_list = new uint64_t[grid_width * grid_height]();

	size_t work_len = IMS(grid_width * width, grid_height * height, 3);
	work = (uint8_t *)allocate_0x00(work_len);
}

view_all::~view_all()
{
	free(work);
	delete [] ts_list;
}

std::string view_all::get_html(const std::map<std::string, std::string> & pars) const
{
	return "";
}

video_frame * view_all::get_frame(const bool handle_failure, const uint64_t after)
{
	bool rc = false;
	const int n_sources = std::min(int(sources.size()), grid_width * grid_height);

	uint64_t si = switch_interval * 1000000.0;
	if (switch_interval < 1.0)
		si = 1000000;

	if (latest_switch == 0)
		latest_switch = get_us();

	uint64_t now = get_us();
	if (now - latest_switch >= si) {
		cur_source_index += n_sources;
		cur_source_index %= sources.size();

		latest_switch = now;

		delete [] ts_list;
		ts_list = new uint64_t[grid_width * grid_height]();
	}

	size_t frame_out_len = IMS(grid_width * width, grid_height * height, 3);

	std::vector<std::thread *> threads;

	uint64_t ts = 0;

	for(int i=0; i<n_sources; i++) {
		threads.push_back(new std::thread([&, i] {
			set_thread_name("GF:" + id);

			size_t offset = (cur_source_index + i) % sources.size();

			instance *next_inst = nullptr;
			source *next_source = nullptr;
			find_by_id(cfg, sources.at(offset).id, &next_inst, (interface **)&next_source);

			if (!next_source)
				log(id, LL_ERR, "source selection %s in view %s is not known", sources.at(offset).id.c_str(), id.c_str());
			else if (uint64_t s_ts = next_source->get_current_ts(); next_source->is_paused() == false && next_source->is_running() == true && (s_ts > ts_list[i] || ts_list[i] == 0)) {

				video_frame *pvf = next_source->get_frame(handle_failure, get_us());

				int cur_grid_x = i % grid_width;
				int cur_grid_y = i / grid_width;

				if (pvf) {
					ts_list[i] = ts = std::max(ts, pvf->get_ts());

					if (pvf->get_w() != width || pvf->get_h() != height) {
						video_frame *temp = pvf->do_resize(r, width, height);
						delete pvf;
						pvf = temp;
					}

					uint8_t *data = pvf->get_data(E_RGB);

					for(int y=0; y<height; y++) {
						int offset = y * width * grid_width * 3 + width * cur_grid_x  * 3 + height * width * 3 * grid_width * cur_grid_y;
						memcpy(&work[offset], &data[y * width * 3], width * 3);
					}

					delete pvf;
				}
			}
			else {
				log(id, LL_DEBUG, "source selection %s in view %s is not running/paused", sources.at(offset).id.c_str(), id.c_str());
			}
		}));
	}

	for(auto th : threads) {
		th->join();
		delete th;
	}

	uint8_t *frame_out = (uint8_t *)duplicate(work, frame_out_len);

	return new video_frame(get_meta(), jpeg_quality, ts, width * grid_width, height * grid_height, frame_out, frame_out_len, E_RGB);
}
