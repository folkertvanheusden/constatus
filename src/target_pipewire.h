// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "target.h"
#if HAVE_PIPEWIRE == 1

#include <spa/param/video/format-utils.h>
#include <pipewire/pipewire.h>


struct pmit_data
{
	std::string id;

	struct pw_context *context { nullptr };
        struct pw_main_loop *loop { nullptr };
	struct pw_core *core { nullptr };

	struct pw_stream *stream { nullptr };
	struct spa_hook stream_listener;

	struct spa_video_info_raw format;
	int32_t stride { 0 };

	uint32_t seq { 0 };

	source *s { nullptr };

	std::atomic_bool playing { false };
};

class target_pipewire : public target
{
private:
	const int quality;

	struct pmit_data data;

	std::atomic_bool playing { false };

public:
	target_pipewire(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_pipewire();

	void operator()() override;
};
#endif
