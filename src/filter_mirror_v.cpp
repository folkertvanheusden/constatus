// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stddef.h>
#include <cstring>

#include "gen.h"
#include "filter_mirror_v.h"

filter_mirror_v::filter_mirror_v()
{
}

filter_mirror_v::~filter_mirror_v()
{
}

void filter_mirror_v::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	const size_t bytes = w * 3;

	for(int y=0; y<h; y++)
		memcpy(&out[(h - y - 1) * bytes], &in[y * bytes], bytes);
}
