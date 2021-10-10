// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <cstring>
#include <stddef.h>
#include "gen.h"
#include "view_3d.h"
#include "http_server.h"
#include "utils.h"
#include "picio.h"
#include "log.h"
#include "filter.h"
#include "resize.h"

static void to_gray(uint8_t *const in_out, const int n_pixels)
{
        for(int i=0, o=0; o<n_pixels;) {
                int r = in_out[i++];
                int g = in_out[i++];
                int b = in_out[i++];

                in_out[o++] = (r + g + b) / 3;
        }
}

view_3d::view_3d(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, std::vector<filter *> *const filters, const view_3d_mode_t v3m, const int jpeg_quality) : view_ss(cfg, id, descr, width, height, false, sources, 0.0, filters, jpeg_quality), v3m(v3m)
{
}

view_3d::~view_3d()
{
	free(l_prev_frame);
	free(r_prev_frame);
}

std::string view_3d::get_html(const std::map<std::string, std::string> & pars) const
{
	return "";
}

video_frame * view_3d::get_frame(const bool handle_failure, const uint64_t after)
{
	// get left

	instance *left_inst = nullptr;
	source *left_source = nullptr;
	find_by_id(cfg, sources.at(0).id, &left_inst, (interface **)&left_source);

	if (!left_source)
		error_exit(false, "Source %s not found", sources.at(0).id.c_str());

	if (left_source->get_class_type() != CT_SOURCE)
		error_exit(false, "Object with id \"%s\" is not of type source", sources.at(0).id.c_str());

	video_frame *left = left_source -> get_frame(handle_failure, after);
	if (!left)
		return nullptr;

	// get right

	instance *right_inst = nullptr;
	source *right_source = nullptr;
	find_by_id(cfg, sources.at(1).id, &right_inst, (interface **)&right_source);

	if (!right_source)
		error_exit(false, "Source %s not found", sources.at(1).id.c_str());

	if (right_source->get_class_type() != CT_SOURCE)
		error_exit(false, "Object with id \"%s\" is not of type source", sources.at(1).id.c_str());

	video_frame *right = right_source -> get_frame(handle_failure, after);
	if (!right) {
		delete left;
		return nullptr;
	}

	int left_width = left->get_w();
	int left_height = left->get_h();
	int right_width = right->get_w();
	int right_height = right->get_h();
	uint64_t left_ts = left->get_ts();
	uint64_t right_ts = right->get_ts();
	uint8_t *left_frame = left->get_data(E_RGB);
	uint8_t *right_frame = right->get_data(E_RGB);

	// make 'prev' frames

	prev_frame_lock.lock();
	if (!l_prev_frame)
		l_prev_frame = (uint8_t *)malloc(IMS(left_width, left_height, 3));
	if (!r_prev_frame)
		r_prev_frame = (uint8_t *)malloc(IMS(right_width, right_height, 3));
	prev_frame_lock.unlock();

	// apply filters

	if (filters) {
		instance *inst = find_instance_by_interface(cfg, this);
		apply_filters(inst, this, filters, l_prev_frame, left_frame, left_ts, left_width, left_height);
		memcpy(l_prev_frame, left_frame, IMS(left_width, left_height, 3));

		inst = find_instance_by_interface(cfg, this);
		apply_filters(inst, this, filters, r_prev_frame, right_frame, right_ts, right_width, right_height);
		memcpy(r_prev_frame, right_frame, IMS(right_width, right_height, 3));
	}

	// combine

	int new_width = -1, new_height = -1;

	if (v3m == view_3d_sidebyside || v3m == view_3d_columns || v3m == view_3d_checkerboard)
		new_width = left_width + right_width, new_height = left_height;
	else if (v3m == view_3d_topbottom || v3m == view_3d_lines)
		new_width = left_width, new_height = left_height + right_height;
	else if (v3m == view_3d_redgreen || v3m == view_3d_redblue)
		new_width = left_width, new_height = left_height;
	else if (v3m == view_3d_framesequence)
		new_width = left_width, new_height = left_height;

	const size_t new_frame_out_len = IMS(new_width, new_height, 3);
	uint8_t *new_frame = (uint8_t *)malloc(new_frame_out_len);

	this->width = new_width;
	this->height = new_height;

	if (v3m == view_3d_sidebyside) {
		for(int y=0; y<new_height; y++) {
			int o1 = y * new_width * 3;
			int o2 = o1 + left_width * 3;

			memcpy(&new_frame[o1], &left_frame[y * left_width * 3], left_width * 3);
			memcpy(&new_frame[o2], &right_frame[y * right_width * 3], right_width * 3);
		}
	}
	else if (v3m == view_3d_lines) {
		for(int y=0; y<left_height; y++) {
			int o1 = (y * 2 + 0) * new_width * 3;
			int o2 = (y * 2 + 1) * new_width * 3;

			memcpy(&new_frame[o1], &left_frame[y * left_width * 3], left_width * 3);
			memcpy(&new_frame[o2], &right_frame[y * right_width * 3], right_width * 3);
		}
	}
	else if (v3m == view_3d_topbottom) {
		int left_size = IMS(left_width, left_height, 3);
		memcpy(&new_frame[0], &left_frame[0], left_size);
		memcpy(&new_frame[left_size], &right_frame[0], IMS(right_width, right_height, 3));
	}
	else if (v3m == view_3d_columns) {
		for(int y=0; y<new_height; y++) {
			int o = y * new_width * 3;

			for(int x=0; x<left_width; x++) {
				new_frame[o + x * 2 * 3 + 0] = left_frame[y * left_width * 3 + x * 3 + 0];
				new_frame[o + x * 2 * 3 + 1] = left_frame[y * left_width * 3 + x * 3 + 1];
				new_frame[o + x * 2 * 3 + 2] = left_frame[y * left_width * 3 + x * 3 + 2];
				new_frame[o + x * 2 * 3 + 3] = right_frame[y * right_width * 3 + x * 3 + 0];
				new_frame[o + x * 2 * 3 + 4] = right_frame[y * right_width * 3 + x * 3 + 1];
				new_frame[o + x * 2 * 3 + 5] = right_frame[y * right_width * 3 + x * 3 + 2];
			}
		}
	}
	else if (v3m == view_3d_checkerboard) {
		for(int y=0; y<new_height; y++) {
			int o = y * new_width * 3;

			if (y & 1) {
				for(int x=0; x<left_width; x++) {
					new_frame[o + x * 2 * 3 + 0] = right_frame[y * right_width * 3 + x * 3 + 0];
					new_frame[o + x * 2 * 3 + 1] = right_frame[y * right_width * 3 + x * 3 + 1];
					new_frame[o + x * 2 * 3 + 2] = right_frame[y * right_width * 3 + x * 3 + 2];
					new_frame[o + x * 2 * 3 + 3] = left_frame[y * left_width * 3 + x * 3 + 0];
					new_frame[o + x * 2 * 3 + 4] = left_frame[y * left_width * 3 + x * 3 + 1];
					new_frame[o + x * 2 * 3 + 5] = left_frame[y * left_width * 3 + x * 3 + 2];
				}
			}
			else {
				for(int x=0; x<left_width; x++) {
					new_frame[o + x * 2 * 3 + 0] = left_frame[y * left_width * 3 + x * 3 + 0];
					new_frame[o + x * 2 * 3 + 1] = left_frame[y * left_width * 3 + x * 3 + 1];
					new_frame[o + x * 2 * 3 + 2] = left_frame[y * left_width * 3 + x * 3 + 2];
					new_frame[o + x * 2 * 3 + 3] = right_frame[y * right_width * 3 + x * 3 + 0];
					new_frame[o + x * 2 * 3 + 4] = right_frame[y * right_width * 3 + x * 3 + 1];
					new_frame[o + x * 2 * 3 + 5] = right_frame[y * right_width * 3 + x * 3 + 2];
				}
			}
		}
	}
	else if (v3m == view_3d_redgreen || v3m == view_3d_redblue) {
		to_gray(left_frame, left_width * left_height);
		to_gray(right_frame, right_width * right_height);

		for(int y=0; y<left_height; y++) {
			int o = y * new_width * 3;

			if (v3m == view_3d_redgreen) {
				for(int x=0; x<left_width; x++) {
					int o2 = o + x * 3;

					new_frame[o2 + 0] = left_frame[y * left_width + x];
					new_frame[o2 + 1] = right_frame[y * right_width + x];
					new_frame[o2 + 2] = 0x00;
				}
			}
			else {
				for(int x=0; x<left_width; x++) {
					int o2 = o + x * 3;

					new_frame[o2 + 0] = left_frame[y * left_width + x];
					new_frame[o2 + 1] = 0x00;
					new_frame[o2 + 2] = right_frame[y * right_width + x];
				}
			}
		}
	}
	else if (v3m == view_3d_framesequence) {
		if (this->left)
			memcpy(new_frame, left_frame, IMS(left_width, left_height, 3));
		else
			memcpy(new_frame, right_frame, IMS(right_width, right_height, 3));

		this->left = !this->left;
	}

	free(right_frame);
	free(left_frame);

	// export

	return new video_frame(get_meta(), jpeg_quality, std::max(left->get_ts(), right->get_ts()), new_width, new_height, new_frame, new_frame_out_len, E_RGB);
}
