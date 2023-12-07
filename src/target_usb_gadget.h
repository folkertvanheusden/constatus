// (C) 2023 by folkert van heusden, released under the MIT license
#pragma once

#include "config.h"
#include <optional>
#include <string>

#include "target.h"


class schedule;

class target_usbgadget : public target
{
private:
	const int fixed_width  { -1 };
	const int fixed_height { -1 };

	const int quality      { 0 };

	std::string uvc_id;

public:
	target_usbgadget(const std::string & id, const std::string & descr, source *const s, const int width, const int height, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched, const std::string & uvc_id);
	virtual ~target_usbgadget();

	void operator()() override;
};
