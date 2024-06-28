// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "target.h"
#if HAVE_PIPEWIRE == 1

#include <spa/param/video/format-utils.h>
#include <pipewire/pipewire.h>


struct pmit_data
{
	std::string id;

	pw_context   *context { nullptr };
        pw_main_loop *loop    { nullptr };
	pw_core      *core    { nullptr };

	pw_stream    *stream  { nullptr };
	spa_hook stream_listener;

	spa_video_info_raw format;
	int32_t       stride  { 0 };

	uint32_t      seq     { 0 };

	source       *s       { nullptr };

	std::atomic_bool playing { false };
};

class target_pipewire : public target
{
private:
	const int quality;

	pmit_data data;

	std::atomic_bool playing { false };

public:
	target_pipewire(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_pipewire();

	void operator()() override;
};
#endif
