// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <stdint.h>
#include <string>

#include "filter.h"

class interface;

class filter_overlay_on_motion : public filter
{
private:
	instance *const i;
	source   *const cks;
	resize   *const r;
	const int       display_time_ms;

	std::atomic_bool changed { false };
	uint64_t         since { 0 };

public:
	filter_overlay_on_motion(instance *const i, source *const cks, resize *const r, const int display_time_ms);
	~filter_overlay_on_motion();

	void notify_thread_of_event(const std::string & subject) override;

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
