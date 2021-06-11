// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <stdint.h>

#include "filter.h"

class selection_mask;

typedef enum { m_red, m_red_invert, m_invert, m_color } sm_mode_t;

typedef struct {
	sm_mode_t mode;
	uint8_t r, g, b;
} sm_pixel_t;

typedef enum { t_box, t_cross, t_circle, t_border } sm_type_t;

void update_pixel(uint8_t *const out, const int x, const int y, const int w);

class filter_marker_simple : public filter
{
private:
	const sm_pixel_t pixel;
	const sm_type_t type;
	selection_mask *const pixel_select_bitmap;
	const int noise_level;
	const double percentage_pixels_changed;
	const int thick;
	instance *const i;
	const bool store_motion_bitmap;

	std::atomic_bool changed { false };

public:
	filter_marker_simple(instance *const i, const sm_pixel_t pixel, const sm_type_t type, selection_mask *const sb, const int noise_level, const double percentage_pixels_changed, const int thick, const bool store_motion_bitmap);
	virtual ~filter_marker_simple();

	void notify_thread_of_event(const std::string & subject) override;

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
