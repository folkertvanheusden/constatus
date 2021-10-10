// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_FREI0R == 1
#include <frei0r.h>
#include <string>

#include "filter_plugin.h"

typedef void (*my_f0r_update)(f0r_instance_t instance, double time, const uint32_t* inframe, uint32_t* outframe);
typedef void (*my_f0r_update2_3)(f0r_instance_t instance, double time, const uint32_t* inframe1, const uint32_t* inframe2, const uint32_t* inframe3, uint32_t* outframe);
;

class filter_plugin_frei0r : public filter_plugin
{
private:
	const std::string filter_filename, pars;
	const std::vector<source *> other_sources;
	resize *const r;

	my_f0r_update my_f0r_update_i{ nullptr };
	my_f0r_update2_3 my_f0r_update_i_2_3{ nullptr };
	f0r_instance_t f_inst{ nullptr };
	int color_model{ -1 };
	int plugin_type{ 0 };
	f0r_plugin_info_t fpi{ 0 };

public:
	filter_plugin_frei0r(const std::string & filter_filename, const std::string & pars, const std::vector<source *> & other_sources, resize *const r);
	~filter_plugin_frei0r();

	bool uses_in_out() const override { return true; }
	void apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out) override;
};
#endif
