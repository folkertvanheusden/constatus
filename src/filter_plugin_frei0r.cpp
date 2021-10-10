// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_FREI0R == 1
#include <assert.h>
#include <dlfcn.h>
#include <string>

#include "gen.h"
#include "error.h"
#include "log.h"
#include "filter_plugin_frei0r.h"
#include "utils.h"
#include "source.h"
#include "resize.h"

typedef int (*my_f0r_init)();
typedef int (*my_f0r_deinit)();
typedef void (*my_f0r_get_plugin_info)(f0r_plugin_info_t *info);
typedef void (*my_f0r_get_param_info)(f0r_param_info_t *info, int param_index);
typedef f0r_instance_t (*my_f0r_construct)(unsigned int width, unsigned int height);
typedef void (*my_f0r_destruct)(f0r_instance_t instance);
typedef void (*my_f0r_set_param_value)(f0r_instance_t instance, f0r_param_t param, int param_index);

void to_frei0r(const uint8_t *const rgb, const int w, const int h, const int color_model, uint8_t **const frei0r)
{
	*frei0r = (uint8_t *)malloc(w * h * 4);

	for(int y=0; y<h; y++) {
		int oo = y * w * 4;
		int oi = y * w * 3;

		if (color_model == F0R_COLOR_MODEL_RGBA8888 || color_model == F0R_COLOR_MODEL_PACKED32) {
			for(int x=0; x<w; x++) {
				(*frei0r)[oo + x * 4 + 0] = rgb[oi + x * 3 + 0];
				(*frei0r)[oo + x * 4 + 1] = rgb[oi + x * 3 + 1];
				(*frei0r)[oo + x * 4 + 2] = rgb[oi + x * 3 + 2];
				(*frei0r)[oo + x * 4 + 3] = 255;
			}
		}
		else if (color_model == F0R_COLOR_MODEL_BGRA8888) {
			for(int x=0; x<w; x++) {
				(*frei0r)[oo + x * 4 + 2] = rgb[oi + x * 3 + 0];
				(*frei0r)[oo + x * 4 + 1] = rgb[oi + x * 3 + 1];
				(*frei0r)[oo + x * 4 + 0] = rgb[oi + x * 3 + 2];
				(*frei0r)[oo + x * 4 + 3] = 255;
			}
		}
	}
}

void from_frei0r(const uint8_t *const frei0r, const int w, const int h, const int color_model, uint8_t *const rgb)
{
	for(int y=0; y<h; y++) {
		int oo = y * w * 3;
		int oi = y * w * 4;

		if (color_model == F0R_COLOR_MODEL_RGBA8888 || color_model == F0R_COLOR_MODEL_PACKED32) {
			for(int x=0; x<w; x++) {
				rgb[oo + x * 3 + 0] = frei0r[oi + x * 4 + 0];
				rgb[oo + x * 3 + 1] = frei0r[oi + x * 4 + 1];
				rgb[oo + x * 3 + 2] = frei0r[oi + x * 4 + 2];
			}
		}
		else if (color_model == F0R_COLOR_MODEL_BGRA8888) {
			for(int x=0; x<w; x++) {
				rgb[oo + x * 3 + 2] = frei0r[oi + x * 4 + 0];
				rgb[oo + x * 3 + 1] = frei0r[oi + x * 4 + 1];
				rgb[oo + x * 3 + 0] = frei0r[oi + x * 4 + 2];
			}
		}
	}
}

filter_plugin_frei0r::filter_plugin_frei0r(const std::string & filter_filename, const std::string & pars, const std::vector<source *> & other_sources, resize *const r) : filter_filename(filter_filename), pars(pars), other_sources(other_sources), r(r)
{
	library = load_library(filter_filename.c_str());

	((my_f0r_init)find_symbol(library, filter_filename, "f0r_init"))();

	((my_f0r_get_plugin_info)find_symbol(library, filter_filename, "f0r_get_plugin_info"))(&fpi);

	color_model = fpi.color_model;
	plugin_type = fpi.plugin_type;

	log(id, LL_INFO, "%s: name: %s", filter_filename.c_str(), fpi.name);
	log(id, LL_INFO, "%s: author: %s", filter_filename.c_str(), fpi.author);
	log(id, LL_INFO, "%s: frei0r version: %d", filter_filename.c_str(), fpi.frei0r_version);
	log(id, LL_INFO, "%s: plugin version: %d.%d", filter_filename.c_str(), fpi.major_version, fpi.minor_version);
	log(id, LL_INFO, "%s: explanation: %s", filter_filename.c_str(), fpi.explanation);

	if (fpi.plugin_type == F0R_PLUGIN_TYPE_FILTER)
		log(id, LL_INFO, "%s: type: filter", filter_filename.c_str());
	else if (fpi.plugin_type == F0R_PLUGIN_TYPE_SOURCE)
		log(id, LL_INFO, "%s: type: source", filter_filename.c_str());
	else if (fpi.plugin_type == F0R_PLUGIN_TYPE_MIXER2) {
		log(id, LL_INFO, "%s: type: mixer (1 extra video-source required)", filter_filename.c_str());

		if (other_sources.size() != 1)
			error_exit(false, "%s: please select one other source in the configuration file", filter_filename.c_str());
	}
	else if (fpi.plugin_type == F0R_PLUGIN_TYPE_MIXER3) {
		log(id, LL_INFO, "%s: type: mixer (2 extra video-sources required)", filter_filename.c_str());

		if (other_sources.size() != 2)
			error_exit(false, "%s: please select two other sources in the configuration file", filter_filename.c_str());
	}
	else {
		error_exit(false, "This frei0r plugin is not supported (%d)", fpi.plugin_type);
	}

	my_f0r_update_i = (my_f0r_update)find_symbol(library, filter_filename, "f0r_update");

	my_f0r_update_i_2_3 = (my_f0r_update2_3)find_symbol(library, filter_filename, "f0r_update2", false);

	for(auto s : other_sources)
		s->start();
}

filter_plugin_frei0r::~filter_plugin_frei0r()
{
	if (f_inst) {
		my_f0r_destruct my_f0r_destruct_i = (my_f0r_destruct)find_symbol(library, filter_filename, "f0r_destruct");
		my_f0r_destruct_i(f_inst);
	}

	((my_f0r_init)find_symbol(library, filter_filename, "f0r_deinit"))();

	dlclose(library);

	// unfortunately we cannot unregister the usage of a source here
	// as it is already cleaned-up by the time we get here
}

void filter_plugin_frei0r::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	if (!f_inst) {
		f_inst = ((my_f0r_construct)find_symbol(library, filter_filename, "f0r_construct"))(w, h);

		my_f0r_get_param_info my_f0r_get_param_info_i = (my_f0r_get_param_info)find_symbol(library, filter_filename, "f0r_get_param_info");
		my_f0r_set_param_value my_f0r_set_param_value_i = (my_f0r_set_param_value)find_symbol(library, filter_filename, "f0r_set_param_value");

		std::vector<std::string> *parts = split(pars, " ");

		std::map<std::string, std::string> kv;
		for(auto s : *parts) {
			size_t is = s.find("=");
			
			if (is != std::string::npos)
				kv.insert(std::pair<std::string, std::string>(s.substr(0, is), s.substr(is + 1)));
			else
				kv.insert(std::pair<std::string, std::string>(s, std::string("true")));
		}

		delete parts;

		log(id, LL_INFO, "%s: %d parameters:", filter_filename.c_str());

		for(int i=0; i<fpi.num_params; i++) {
			f0r_param_info_t info{ 0 };
			my_f0r_get_param_info_i(&info, i);

			const char *const frei0r_type_str[] = { "bool", "float", "color", "position", "string" };
			log(id, LL_INFO, "%s: par %d: %s / %s / %s", filter_filename.c_str(), i, info.name, frei0r_type_str[info.type], info.explanation);

			auto it = kv.find(info.name);

			if (it == kv.end())
				continue;

			if (info.type == F0R_PARAM_BOOL) {
				bool v = it->second == "true" || it->second == "y" || it->second == "1";
				my_f0r_set_param_value_i(f_inst, &v, i);
			}
			else if (info.type == F0R_PARAM_DOUBLE) {
				double v = atof(it->second.c_str());
				my_f0r_set_param_value_i(f_inst, &v, i);
			}
			else if (info.type == F0R_PARAM_COLOR) {
				float r = 0, g = 0, b = 0;
				sscanf(it->second.c_str(), "%f/%f/%f", &r, &g, &b);

				f0r_param_color_t v{ float(r / 100.0), float(g / 100.0), float(b / 100.0) };

				my_f0r_set_param_value_i(f_inst, &v, i);
			}
			else if (info.type == F0R_PARAM_POSITION) {
				float x = 0, y = 0;
				sscanf(it->second.c_str(), "%f/%f", &x, &y);

				f0r_param_position_t v{ x / 100.0, y / 100.0 };

				my_f0r_set_param_value_i(f_inst, &v, i);
			}
		}
	}

	std::vector<uint8_t *> frei0r_from;

	uint8_t *from = nullptr;

	if (plugin_type != F0R_PLUGIN_TYPE_SOURCE) {
		to_frei0r(in, w, h, color_model, &from);
		frei0r_from.push_back(from);
	}

	for(auto s : other_sources) {
		video_frame *pvf = s->get_frame(true, ts);

                if (!pvf) {
			size_t n_bytes = w * h * 3;
			uint8_t *data = (uint8_t *)malloc(n_bytes);

			pvf = new video_frame(nullptr, 100, ts, w, h, data, n_bytes, E_RGB);
		}

		if (pvf->get_w() != w || pvf->get_h() != h) {
			video_frame *temp = pvf->do_resize(r, w, h);
			delete pvf;
			pvf = temp;
		}

		to_frei0r(pvf->get_data(E_RGB), pvf->get_w(), pvf->get_h(), color_model, &from);
		frei0r_from.push_back(from);

		delete pvf;
	}

	uint8_t *const frei0r_to = (uint8_t *)malloc(w * h * 4);

	if (plugin_type == F0R_PLUGIN_TYPE_SOURCE || plugin_type == F0R_PLUGIN_TYPE_FILTER)
		my_f0r_update_i(f_inst, ts / 1000000.0, (const uint32_t *)frei0r_from.at(0), (uint32_t *)frei0r_to);
	else
		my_f0r_update_i_2_3(f_inst, ts / 1000000.0, (const uint32_t *)frei0r_from.at(0), (const uint32_t *)frei0r_from.at(1), frei0r_from.size() == 3 ? (const uint32_t *)frei0r_from.at(2) : nullptr, (uint32_t *)frei0r_to);

	for(auto p : frei0r_from)
		free(p);

	from_frei0r(frei0r_to, w, h, color_model, out);
	free(frei0r_to);
}
#endif
