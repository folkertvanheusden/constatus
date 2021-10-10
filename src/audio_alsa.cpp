// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0

#include "config.h"
#if ALSA_FOUND == 1
#include <errno.h>
#include <cstring>
#include "audio_alsa.h"
#include "log.h"
#include "error.h"

audio_alsa::audio_alsa(const std::string & dev_name, const int sample_rate_in) : dev_name(dev_name), sample_rate(sample_rate_in)
{
	/* Capture stream */
	snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;

	/* This structure contains information about    */
	/* the hardware and can be used to specify the  */      
	/* configuration to be used for the PCM stream. */ 
	snd_pcm_hw_params_t *hwparams { nullptr };

	/* Allocate the snd_pcm_hw_params_t structure on the stack. */
	snd_pcm_hw_params_alloca(&hwparams);

	int rc = 0;

	/* Open PCM. The last parameter of this function is the mode. */
	/* If this is set to 0, the standard mode is used. Possible   */
	/* other values are SND_PCM_NONBLOCK and SND_PCM_ASYNC.       */ 
	/* If SND_PCM_NONBLOCK is used, read / write access to the    */
	/* PCM device will return immediately. If SND_PCM_ASYNC is    */
	/* specified, SIGIO will be emitted whenever a period has     */
	/* been completely processed by the soundcard.                */
	if ((rc = snd_pcm_open(&pcm_handle, dev_name.c_str(), stream, 0)) < 0) {
		error_exit(false, "Error opening PCM device %s: %s\n", dev_name.c_str(), strerror(-rc));
	}

	/* Init hwparams with full configuration space */
	if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0)
		error_exit(false, "Can not configure this PCM device.\n");

	unsigned int exact_rate = 0;   /* Sample rate returned by */
	/* snd_pcm_hw_params_set_rate_near */ 
	int dir = 0;          /* exact_rate == rate --> dir = 0 */
	/* exact_rate < rate  --> dir = -1 */
	/* exact_rate > rate  --> dir = 1 */
	int periods = 2;       /* Number of periods */
	// snd_pcm_uframes_t periodsize = 8192; /* Periodsize (bytes) */ 

	/* Set access type. This can be either    */
	/* SND_PCM_ACCESS_RW_INTERLEAVED or       */
	/* SND_PCM_ACCESS_RW_NONINTERLEAVED.      */
	/* There are also access types for MMAPed */
	/* access, but this is beyond the scope   */
	/* of this introduction.                  */
	if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		error_exit(false, "Error setting access.\n");
	}

	/* Set sample format */
	if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
		error_exit(false, "Error setting format.\n");
	}

	snd_pcm_hw_params_get_channels(hwparams, (unsigned int *)&n_channels);

	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = sample_rate;
	if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0) < 0) {
		error_exit(false, "Error setting rate.\n");
	}
	if (sample_rate != exact_rate) {
		log(LL_INFO, "The rate %d Hz is not supported by your hardware.\n ==> Using %d Hz instead.\n", sample_rate, exact_rate);
		sample_rate = exact_rate;
	}

	/* Set number of periods. Periods used to be called fragments. */ 
	if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, periods, 0) < 0) {
		error_exit(false, "Error setting periods.\n");
	}

	/* Apply HW parameter settings to */
	/* PCM device and prepare device  */
	if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
		error_exit(false, "Error setting HW params.\n");
	}
}

audio_alsa::~audio_alsa()
{
	snd_pcm_close(pcm_handle);
}

std::tuple<int16_t *, size_t> audio_alsa::get_audio_mono(const size_t n_samples)
{
	size_t samples = n_samples, index = 0;

	int16_t *buffer = new int16_t[n_samples * n_channels];

	while(samples > 0)
	{
		int cur = samples, pcmreturn = 0;

		while ((pcmreturn = snd_pcm_readi(pcm_handle, &buffer[index], cur)) < 0) {
			snd_pcm_prepare(pcm_handle);

			log(LL_INFO, "Audio underrun");
		}

		index += pcmreturn;
		samples -= pcmreturn;
	}

	if (n_channels > 1) {
		for(size_t i=0; i<n_samples; i++) {
			int temp = 0;

			for(int ch=0; ch<n_channels; ch++)
				temp += buffer[i * n_channels + ch];

			buffer[i] = temp / n_channels;
		}
	}

	return { buffer, n_samples };
}
#endif
