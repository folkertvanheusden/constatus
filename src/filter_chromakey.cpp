// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <cstring>

#include "error.h"
#include "gen.h"
#include "filter_chromakey.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "resize.h"

filter_chromakey::filter_chromakey(source *const cks, resize *const r) : cks(cks), r(r)
{
	cks->start();
}

filter_chromakey::~filter_chromakey()
{
	// at this point (shutdown) the cks no longer exists
	// cks->stop();
}

void filter_chromakey::apply(instance *const inst, interface *const specific_int, const uint64_t ts, const int width, const int height, const uint8_t *const prev, uint8_t *const in_out)
{
	video_frame *pvf = cks-> get_frame(true, ts);
	if (!pvf)
		return;

	if (pvf->get_w() != width || pvf->get_h() != height) {
		video_frame *temp = pvf->do_resize(r, width, height);
		delete pvf;
		pvf = temp;
	}


	// if pixel in `in_out' == green,
	// replace by cks frame

	const uint8_t *const in = pvf->get_data(E_RGB);

	for(int i=0; i<width * height; i++) {
		int i3 = i * 3;

		double h, l, s;
		rgb_to_hls(in_out[i3 + 0], in_out[i3 + 1], in_out[i3 + 2], &h, &s, &l);

		if (h >= 90 && h < 150 && l >= 0.2) {
			in_out[i3 + 0] = in[i3 + 0];
			in_out[i3 + 1] = in[i3 + 1];
			in_out[i3 + 2] = in[i3 + 2];
		}
	}

	delete pvf;
}
