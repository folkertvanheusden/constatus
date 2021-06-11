// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once

#include "audio.h"

class audio_ffmpeg : public audio
{
public:
	audio_ffmpeg();
	~audio_ffmpeg();

	void put_audio(const int16_t *samples, const size_t n_samples);

	void operator()() override;
};
