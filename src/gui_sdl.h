// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_LIBSDL2 == 1
#include <SDL2/SDL.h>

#include "gui.h"

class gui_sdl : public gui
{
public:
	gui_sdl(configuration_t *const cfg, const std::string & id, const std::string & descr, source *const s, const double fps, const int w, const int h, std::vector<filter *> *const filters, const bool handle_failure);
	virtual ~gui_sdl();

	virtual void operator()() override;
};
#endif
