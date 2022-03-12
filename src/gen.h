// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <optional>
#include <stdint.h>

#include "video_frame.h"

typedef struct {
	std::string msg;
	bool critical;
} error_state_t;

typedef struct {
	uint8_t r, g, b;
} rgb_t;

typedef struct
{
	uint64_t ts;
	int w, h;
	uint16_t *data;
	size_t len;
	encoding_t e;
} depth_frame_t;

typedef struct
{
	uint64_t ts;
	int sample_rate;
	short *data;
	size_t len;
} audio_frame_t;

typedef struct
{
	video_frame *vf;
	depth_frame_t *depth_frame;
	audio_frame_t *audio_frame;
} bundle_t;

extern struct timeval app_start_ts;

// int multiply to size_t; to silence lgtm.com
#define IMS(w, h, n) (size_t(w) * size_t(h) * size_t(n))
#define IMUL(w, h, n) ((unsigned long)(w) * (unsigned long)(h) * (unsigned long)(n))
