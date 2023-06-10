// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <cstring>

#include "gen.h"
#include "filter.h"
#include "log.h"
#include "utils.h"

void apply_filters(instance *const i, interface *const specific_int, const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h)
{
	if (!filters)
		return;

	const size_t bytes = IMS(w, h, 3);
	uint8_t *temp = nullptr;

	bool flag = false;
	for(filter *f : *filters) {
		if (f -> uses_in_out()) {
			if (!temp)
				temp = (uint8_t *)malloc(bytes);

			if (flag == false)
				f -> apply_io(i, specific_int, ts, w, h, prev, work, temp);
			else
				f -> apply_io(i, specific_int, ts, w, h, prev, temp, work);

			flag = !flag;
		}
		else {
			if (flag == false)
				f -> apply(i, specific_int, ts, w, h, prev, work);
			else
				f -> apply(i, specific_int, ts, w, h, prev, temp);
		}
	}

	if (flag == true)
		memcpy(work, temp, bytes);

	free(temp);
}

void free_filters(const std::vector<filter *> *filters)
{
	if (filters) {
		for(filter *f : *filters)
			delete f;

		delete filters;
	}
}

filter::filter() : interface("filter", "filter")
{
	ct = CT_FILTER;
}

filter::~filter()
{
}

void filter::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	log(id, LL_FATAL, "filter::apply_io should not be called");
}

void filter::apply(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	log(id, LL_FATAL, "filter::apply should not be called");
}
