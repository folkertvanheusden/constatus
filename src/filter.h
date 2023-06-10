// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

#include <stdint.h>
#include <vector>

#include "cfg.h"
#include "interface.h"

// NULL filter
class filter : public interface
{
protected:
	filter();

public:
	virtual ~filter();

	virtual bool uses_in_out() const { return true; }
	virtual void apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
	virtual void apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};

void apply_filters(instance *const i, interface *const specific_int, const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h);

void free_filters(const std::vector<filter *> *filters);
