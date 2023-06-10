// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "source.h"
#include "gen.h"

typedef struct {
	std::string id;
	pos_t pos; // currently only for PIP views
	int perc;  // shrink percentage (PIP views)
} view_src_t;

class view : public source
{
protected:
	configuration_t              *const cfg;
	const std::vector<view_src_t> sources;

public:
	view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, std::vector<filter *> *const filters, const int jpeg_quality);
	view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<view_src_t> & sources, const int jpeg_quality);
	virtual ~view();

	virtual std::string get_html(const std::map<std::string, std::string> & pars) const = 0;

	virtual video_frame * get_frame(const bool handle_failure, const uint64_t after) override { return { }; }

	virtual source *get_current_source();

	const std::vector<view_src_t> get_sources() const { return sources; }

	std::optional<double> get_fps() override;

	virtual void operator()() override;
};
