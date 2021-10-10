// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_LIBSDL2 == 1
#include "utils.h"
#include "gen.h"
#include "filter.h"
#include "error.h"
#include "log.h"
#include "source.h"
#include "gui_sdl.h"
#include "resize.h"

gui_sdl::gui_sdl(configuration_t *const cfg, const std::string & id, const std::string & descr, source *const s, const double fps, const int w, const int h, std::vector<filter *> *const filters, const bool handle_failure) : gui(cfg, id, descr, s, fps, w, h, filters, handle_failure)
{
}

gui_sdl::~gui_sdl()
{
	stop();
}

void gui_sdl::operator()()
{
	log(id, LL_INFO, "GUI-SDL thread starting");

	SDL_Window *win = SDL_CreateWindow(NAME " " VERSION, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, 0);
	if (!win)
		error_exit(false, "Failed to create window: %s", SDL_GetError());

	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	if (!ren)
		error_exit(false, "Failed to setup renderer: %s", SDL_GetError());

	SDL_Surface *surface = SDL_GetWindowSurface(win);
	if (!surface)
		error_exit(false, "Cannot retrieve window-surface: %s", SDL_GetError());

	set_thread_name("SDL_");

	s -> start();

	uint64_t prev_ts = 0;
	video_frame *prev_frame = nullptr;

	instance *const inst = find_instance_by_interface(cfg, s);

	for(;!local_stop_flag;) {
		SDL_Event event { 0 };
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT)
				local_stop_flag = true;
		}

		if (local_stop_flag)
			break;

		pauseCheck();

		uint64_t before_ts = get_us();

		video_frame *pvf = s -> get_frame(handle_failure, prev_ts);
		if (!pvf)
			continue;

		if (pvf->get_w() != this->w || pvf->get_h() != this->h) {
			video_frame *temp = pvf->do_resize(cfg->r, this->w, this->h);
			delete pvf;
			pvf = temp;
		}

		if (filters && !filters->empty()) {
			video_frame *temp = pvf->apply_filtering(inst, s, prev_frame, filters, nullptr);
			delete pvf;
			pvf = temp;
		}

		SDL_Surface *put = SDL_CreateRGBSurfaceWithFormatFrom(pvf->get_data(E_RGB), this->w, this->h, 24, this->w * 3, SDL_PIXELFORMAT_RGB24);
		SDL_Rect rect{ 0, 0, this->w, this->h };
		SDL_BlitSurface(put, &rect, surface, &rect);
		SDL_FreeSurface(put);

		prev_ts = pvf->get_ts();

		delete prev_frame;
		prev_frame = pvf;

		SDL_UpdateWindowSurface(win);

		st->track_cpu_usage();
		handle_fps(&local_stop_flag, fps, before_ts);
	}

	log(id, LL_INFO, "GUI-SDL thread terminating");

	delete prev_frame;

	s -> stop();

	SDL_FreeSurface(surface);

	SDL_DestroyRenderer(ren);

	SDL_DestroyWindow(win);
}
#endif
