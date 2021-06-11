// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "view_ss.h"

typedef enum { view_3d_sidebyside, view_3d_topbottom, view_3d_lines, view_3d_columns, view_3d_checkerboard, view_3d_framesequence, view_3d_redgreen, view_3d_redblue } view_3d_mode_t;

class view_3d : public view_ss
{
private:
	const view_3d_mode_t v3m;

	std::mutex prev_frame_lock;
	uint8_t *l_prev_frame { nullptr };
	uint8_t *r_prev_frame { nullptr };

	bool left { true };

public:
	view_3d(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, std::vector<filter *> *const filters, const view_3d_mode_t v3m, const int jpeg_quality);
	virtual ~view_3d();

	std::string get_html(const std::map<std::string, std::string> & pars) const override;

	virtual video_frame * get_frame(const bool handle_failure, const uint64_t after) override;
};
