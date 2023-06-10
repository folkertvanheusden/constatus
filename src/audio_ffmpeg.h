// (C) 2017-2023 by folkert van heusden, released under the MIT license
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
