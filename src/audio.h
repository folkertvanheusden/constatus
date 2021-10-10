// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <tuple>

class audio
{
public:
	audio();
	virtual ~audio();

	virtual int get_samplerate() = 0;

	virtual std::tuple<int16_t *, size_t> get_audio_mono(const size_t n_samples) = 0;

	virtual void operator()() = 0;
};
