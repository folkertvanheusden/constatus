// (C) 2023 by folkert van heusden, released under the MIT license
#pragma once

#include "config.h"

#if HAVE_USBGADGET == 1
#include <optional>
#include <string>
#include <usbg/usbg.h>
#include <usbg/function/uvc.h>

#include "source.h"
#include "target.h"

extern "C" {
#include "uvcgadget/events.h"
}


class schedule;

class target_usbgadget : public target
{
private:
	const int fixed_width  { -1 };
	const int fixed_height { -1 };

	const int quality      { 0 };

        usbg_state  *ug_state  { nullptr };
        usbg_gadget *g         { nullptr };
        usbg_config *c         { nullptr };
        usbg_function *f_uvc   { nullptr };

	struct events events   { 0 };

	video_frame *prev_frame { nullptr };
	uint64_t s_prev_ts     { 0 };
	uint64_t t_prev_ts     { 0 };

	std::optional<std::string> setup();
	void                       unsetup();

public:
	target_usbgadget(const std::string & id, const std::string & descr, source *const s, const int width, const int height, const double interval, const std::vector<filter *> *const filters, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched);
	virtual ~target_usbgadget();

	void stop() override;

	video_frame * get_prepared_frame();
	void override_fps(const unsigned new_fps);

	void operator()() override;
};
#endif
