// (c) 2017-2021 by folkert van heusden, released under agpl v3.0
#pragma once

#include "config.h"
#include <atomic>
#include <string>
#include <thread>
#if HAVE_PIPEWIRE == 1
#include <pipewire/core.h>
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>
#include <pipewire/pipewire.h>

#include "source.h"
#include "picio.h"

struct pmis_data
{
        struct pw_main_loop *loop;
        struct pw_stream *stream;

        struct spa_video_info format;

	source *s;

	transformer_t tf;
};

class source_pipewire : public source
{
private:
	const int source_id;
	const double interval;

	struct pmis_data data { 0, };
	const struct spa_pod *params[1] { nullptr };
	struct spa_rectangle min_dim, default_dim, max_dim;
	struct spa_fraction min_frac, default_frac, max_frac;

public:
	source_pipewire(const std::string & id, const std::string & descr, const int source_id, const int width, const int height, const int quality, controls *const c, const double max_fps, const std::map<std::string, feed *> & text_feeds);
	~source_pipewire();

	void operator()() override;
};
#endif
