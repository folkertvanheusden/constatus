// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <string>
#include <vector>
#if HAVE_IMAGICK == 1
#include <Magick++.h>

#include "filter.h"

class filter_anigif_overlay : public filter
{
private:
	const pos_t pos;
	std::vector<Magick::Image> imageList;
	size_t cur_page { 0 };

public:
	filter_anigif_overlay(const std::string & file, const pos_t & pos);
	~filter_anigif_overlay();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
#endif
