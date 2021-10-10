// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "cfg.h"
#include "interface.h"

class gui : public interface
{
protected:
	configuration_t *const cfg;
	source *const s;
	const double fps;
	const int w, h;
	std::vector<filter *> *const filters;
	const bool handle_failure;

public:
	gui(configuration_t *const cfg, const std::string & id, const std::string & descr, source *const s, const double fps, const int w, const int h, std::vector<filter *> *const filters, const bool handle_failure);

	virtual ~gui();

	virtual void operator()();
};
