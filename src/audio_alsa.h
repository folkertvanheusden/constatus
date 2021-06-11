// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "config.h"
#if ALSA_FOUND == 1

#include <alsa/asoundlib.h>
#include <string>

#include "audio.h"

class audio_alsa : public audio
{
private:
	const std::string dev_name;
	int sample_rate, n_channels;

	snd_pcm_t *pcm_handle;

public:
	audio_alsa(const std::string & dev_name, const int sample_rate);
	~audio_alsa();

	int get_samplerate() override { return sample_rate; }

	std::tuple<int16_t *, size_t> get_audio_mono(const size_t n_samples) override;

	void operator()() override { }
};

#endif
