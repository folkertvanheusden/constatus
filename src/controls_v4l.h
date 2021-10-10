// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_LIBV4L2 == 1
#include "controls.h"

class controls_v4l : public controls
{
private:
	const int fd;

	int brightness_min { 0 }, brightness_max { 0 };
	int contrast_min { 0 }, contrast_max { 0 };
	int saturation_min { 0 }, saturation_max { 0 };

public:
	controls_v4l(const int fd);
	virtual ~controls_v4l();

	virtual void reset() override;

	virtual bool has_controls() const override { return true; }

	bool has_brightness() override;
	int get_brightness() override;
	void set_brightness(const int b) override;

	bool has_contrast() override;
	int get_contrast() override;
	void set_contrast(const int b) override;

	bool has_saturation() override;
	int get_saturation() override;
	void set_saturation(const int b) override;

	void apply(uint8_t *const target, const int w, const int h) override;
};
#endif
