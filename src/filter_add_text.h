// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <stdint.h>
#include <string>

#include "filter.h"

class interface;

void find_text_dim(const char *const in, int *const n_lines, int *const n_cols);
std::string unescape(const std::string & in, const uint64_t ts, instance *const i, interface *const specific_int);

class filter_add_text : public filter
{
private:
	const std::string what;
	const pos_t tp;

public:
	filter_add_text(const std::string & what, const pos_t tp);
	~filter_add_text();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
