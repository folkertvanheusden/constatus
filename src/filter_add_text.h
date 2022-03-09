// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <stdint.h>
#include <string>

#include "filter.h"

class interface;
class feed;

void find_text_dim(const char *const in, int *const n_lines, int *const n_cols);

std::string unescape(const std::string & in, const uint64_t ts, instance *const i, interface *const specific_int, const std::map<std::string, feed *> & text_feeds);

class filter_add_text : public filter
{
private:
	const std::string                   what;
	const pos_t                         tp;
	const std::map<std::string, feed *> text_feeds;

public:
	filter_add_text(const std::string & what, const pos_t tp, const std::map<std::string, feed *> & text_feeds);
	~filter_add_text();

	bool uses_in_out() const override { return false; }
	void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) override;
};
