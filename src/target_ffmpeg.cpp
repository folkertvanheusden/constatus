// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#if HAVE_FFMPEG == 1
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

#include "target_ffmpeg.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"
#include "source.h"
#include "view.h"
#include "filter.h"
#include "schedule.h"

target_ffmpeg::target_ffmpeg(const std::string & id, const std::string & descr, const std::string & parameters, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int max_time, const double interval, const std::string & type, const int bitrate, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched) : target(id, descr, s, store_path, prefix, fmt, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps, cfg, is_view_proxy, handle_failure, sched), parameters(parameters), type(type), bitrate(bitrate)
{
	if (this -> descr == "")
		this -> descr = store_path + "/" + prefix;
}

target_ffmpeg::~target_ffmpeg()
{
	stop();
}

static const std::string my_av_err2str(const int nr)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE];

	av_strerror(nr, buffer, sizeof buffer);

        return buffer;
}

// based on https://www.ffmpeg.org/doxygen/2.0/doc_2examples_2muxing_8c-example.html
/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;
	AVCodecContext *enc;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
} OutputStream;

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static bool add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id, int fps, int bitrate, int w, int h)
{
	AVCodecContext *c;
	int i;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		log(LL_ERR, "Can't find encoder for '%s'", avcodec_get_name(codec_id));
		return false;
	}

	ost->st = avformat_new_stream(oc, nullptr);
	if (!ost->st) {
		log(LL_ERR, "Can't allocate stream");
		return false;
	}
	ost->st->discard = AVDISCARD_NONE;
	ost->st->id = oc->nb_streams-1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		log(LL_ERR, "Can't alloc an encoding context");
		return false;
	}
	ost->enc = c;

	switch ((*codec)->type) {
		case AVMEDIA_TYPE_AUDIO:
			c->sample_fmt  = (*codec)->sample_fmts ?
				(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
			c->bit_rate    = 64000;
			c->sample_rate = 44100;
			if ((*codec)->supported_samplerates) {
				c->sample_rate = (*codec)->supported_samplerates[0];
				for (i = 0; (*codec)->supported_samplerates[i]; i++) {
					if ((*codec)->supported_samplerates[i] == 44100)
						c->sample_rate = 44100;
				}
			}
			c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
			c->channel_layout = AV_CH_LAYOUT_STEREO;
			if ((*codec)->channel_layouts) {
				c->channel_layout = (*codec)->channel_layouts[0];
				for (i = 0; (*codec)->channel_layouts[i]; i++) {
					if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
						c->channel_layout = AV_CH_LAYOUT_STEREO;
				}
			}
			c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
			ost->st->time_base = AVRational{ 1, c->sample_rate };
			break;

		case AVMEDIA_TYPE_VIDEO:
			c->codec_id = codec_id;

			c->bit_rate = bitrate;
			/* Resolution must be a multiple of two. */
			c->width    = w;
			c->height   = h;
			/* timebase: This is the fundamental unit of time (in seconds) in terms
			 * of which frame timestamps are represented. For fixed-fps content,
			 * timebase should be 1/framerate and timestamp increments should be
			 * identical to 1. */
			ost->st->time_base = AVRational{1, fps};
			c->time_base       = ost->st->time_base;

			c->gop_size      = std::max(fps / 2, 1); /* emit one intra frame every twelve frames at most */
			c->pix_fmt       = STREAM_PIX_FMT;

			if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
				/* just for testing, we also add B-frames */
				c->max_b_frames = 2;
			}

			if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
				/* Needed to avoid using macroblocks in which some coeffs overflow.
				 * This does not happen with normal video, it just happens here as
				 * the motion of the chroma plane does not match the luma plane. */
				c->mb_decision = 2;
			}
			break;

		default:
			break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return true;
}

/**************************************************************/
/* audio output */
#if 0
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
		uint64_t channel_layout,
		int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		log(id, LL_ERR, "Error allocating an audio frame\n");
		return nullptr;
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			log(id, LL_ERR, "Error allocating an audio buffer\n");
			av_frame_free(&frame);
			return nullptr;
		}
	}

	return frame;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = nullptr;

	c = ost->enc;

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		log(id, LL_ERR, "Can't open audio codec: %s", my_av_err2str(ret).c_str());
		exit(1);
	}

	/* init signal generator */
	ost->t     = 0;
	ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
			c->sample_rate, nb_samples);
	ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
			c->sample_rate, nb_samples);

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		log(id, LL_ERR, "Can't copy the stream parameters");
		exit(1);
	}

	/* create resampler context */
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		log(id, LL_ERR, "Can't allocate resampler context");
		exit(1);
	}

	/* set options */
	av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		log(id, LL_ERR, "Failed to initialize the resampling context\n");
		exit(1);
	}
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
	AVFrame *frame = ost->tmp_frame;
	int j, i, v;
	int16_t *q = (int16_t*)frame->data[0];

//	/* check if we want to generate more frames */
//	if (av_compare_ts(ost->next_pts, ost->enc->time_base,
//				STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
		return nullptr;

	for (j = 0; j <frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->enc->channels; i++)
			*q++ = v;
		ost->t     += ost->tincr;
		ost->tincr += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts  += frame->nb_samples;

	return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame;
	int ret;
	int got_packet;
	int dst_nb_samples;

	av_init_packet(&pkt);
	c = ost->enc;

	frame = get_audio_frame(ost);

	if (frame) {
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
				c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);

		/* when we pass a frame to the encoder, it may keep a reference to it
		 * internally;
		 * make sure we do not overwrite it here
		 */
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);

		/* convert to destination format */
		ret = swr_convert(ost->swr_ctx,
				ost->frame->data, dst_nb_samples,
				(const uint8_t **)frame->data, frame->nb_samples);
		if (ret < 0) {
			log(id, LL_ERR, "Error while converting\n");
			exit(1);
		}
		frame = ost->frame;

		frame->pts = av_rescale_q(ost->samples_count, AVRational{1, c->sample_rate}, c->time_base);
		ost->samples_count += dst_nb_samples;
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		log(id, LL_ERR, "Error encoding audio frame: %s", my_av_err2str(ret).c_str());
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			log(id, LL_ERR, "Error while writing audio frame: %s", my_av_err2str(ret).c_str());
			exit(1);
		}

		printf("%lu\n", ost->st->nb_frames);
	}

	return (frame || got_packet) ? 0 : 1;
}
#endif

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture = av_frame_alloc();
	if (!picture)
		return nullptr;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	int ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		log(LL_ERR, "Can't allocate frame data");
		av_frame_free(&picture);
		return nullptr;
	}

	return picture;
}

static bool open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c = ost->enc;
	AVDictionary *opt = nullptr;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	int ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		log(LL_ERR, "Can't open video codec: %s", my_av_err2str(ret).c_str());
		return false;
	}

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		log(LL_ERR, "Can't allocate video frame");
		return false;
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	ost->tmp_frame = nullptr;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!ost->tmp_frame) {
			log(LL_ERR, "Can't allocate temporary picture");
			return false;
		}
	}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		log(LL_ERR, "Can't copy the stream parameters\n");
		return false;
	}

	return true;
}

static AVFrame *get_video_frame(OutputStream *ost, source *const s, uint64_t *const prev_ts, const std::vector<filter *> *const filters, video_frame **prev_frame, std::vector<video_frame *> & pre_record, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure)
{
	AVCodecContext *c = ost->enc;

//	/* check if we want to generate more frames */
//	if (av_compare_ts(ost->next_pts, c->time_base,
//				STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
//		return nullptr;

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally; make sure we do not overwrite it here */
	int ret = -1;
	if ((ret = av_frame_make_writable(ost->frame)) < 0) {
		log(LL_ERR, "Can't initialize the conversion context %s", my_av_err2str(ret).c_str());
		return nullptr;
	}

	if (s) {
		video_frame *pvf = s -> get_frame(handle_failure, *prev_ts);

		if (pvf)
			pre_record.push_back(pvf);
	}

	if (pre_record.empty())
		return nullptr;

	auto f = pre_record.front();
	pre_record.erase(pre_record.begin());

	*prev_ts = f->get_ts();

	if (filters && !filters->empty()) {
		source *cur_s = is_view_proxy && s ? ((view *)s) -> get_current_source() : s;
		instance *inst = find_instance_by_interface(cfg, cur_s);

		video_frame *temp = f->apply_filtering(inst, cur_s, *prev_frame, filters, nullptr);
		delete f;
		f = temp;
	}

	/* as we only generate a YUV420P picture, we must convert it
	 * to the codec pixel format if needed */
	if (!ost->sws_ctx) {
		ost->sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width, c->height, c->pix_fmt, SCALE_FLAGS, nullptr, nullptr, nullptr);
		if (!ost->sws_ctx) {
			delete f;
			log(LL_ERR, "Can't initialize the conversion context\n");
			return nullptr;
		}
	}

	int line_size = c -> width * 3;
	uint8_t *f_data = f->get_data(E_RGB);
	sws_scale(ost->sws_ctx, &f_data, &line_size, 0, c->height, ost->frame->data, ost->frame->linesize);

	delete *prev_frame;
	*prev_frame = f;

	ost->frame->pts = ost->next_pts++;

	return ost->frame;
}

static void force_flush(AVFormatContext *oc, OutputStream *ost)
{
	int64_t pos = avio_tell(oc->pb);

	AVCodecContext *c = ost->enc;

	do {
		AVPacket pkt { 0 };
		av_init_packet(&pkt);

		int ret = avcodec_send_frame(c, nullptr);
		if (ret)
			break;

		for(;;) {
			ret = avcodec_receive_packet(c, &pkt);
			if (ret)
				break;

			ret = write_frame(oc, &c->time_base, ost->st, &pkt);
			if (ret)
				break;

			av_packet_unref(&pkt);
		}

		avio_flush(oc->pb);
	}
	while(avio_tell(oc->pb) == pos);
}

static bool write_video_frame(AVFormatContext *oc, OutputStream *ost, source *const s, uint64_t *const prev_ts, const std::vector<filter *> *const filters, video_frame **prev_frame, std::vector<video_frame *> & pre_record, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched)
{
	AVCodecContext *c = ost->enc;

	AVFrame *frame = get_video_frame(ost, s, prev_ts, filters, prev_frame, pre_record, cfg, is_view_proxy, handle_failure);
	if (!frame)
		return false;

	const bool allow_store = sched == nullptr || (sched && sched->is_on());

	if (!allow_store)
		return true;

	AVPacket pkt = { 0 };
	av_init_packet(&pkt);

	int ret = avcodec_send_frame(c, frame);
	if (ret < 0) {
		log(LL_ERR, "Error sending a frame for encoding %s", my_av_err2str(ret).c_str());
		return false;
	}

	for(;;) {
		ret = avcodec_receive_packet(c, &pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			log(LL_ERR, "Error during encoding %s", my_av_err2str(ret).c_str());
			return false;
		}

		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			log(LL_ERR, "Error while writing video frame: %s", my_av_err2str(ret).c_str());
			return false;
		}

		av_packet_unref(&pkt);
	}

	return true;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	avcodec_close(ost->enc);
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

void target_ffmpeg::operator()()
{
	set_thread_name("storef_" + prefix);

	s -> start();

	const int fps = interval <= 0 ? 25 : std::max(1, int(1.0 / interval));

	uint64_t prev_ts = get_us();
	int width = -1, height = -1;

	for(;!local_stop_flag;) {
		video_frame *pvf = s -> get_frame(false, prev_ts);

		if (pvf) {
			prev_ts = pvf->get_ts();
			width = pvf->get_w();
			height = pvf->get_h();

			delete pvf;
			break;
		}
	}

	std::string name;
	unsigned f_nr = 0;
	bool is_start = true;

	video_frame *prev_frame = nullptr;

	////////////////////////////////////////////////////////////////////////////

	for(;!local_stop_flag;) {
		OutputStream video_st = { 0 }, audio_st = { 0 };
		AVOutputFormat *fmt = nullptr;
		AVFormatContext *oc = nullptr;
		AVCodec *audio_codec = nullptr, *video_codec = nullptr;
		int ret = -1;
		int have_video = 0, have_audio = 0;
		int encode_video = 0, encode_audio = 0;
		AVDictionary *opt = nullptr;

		/* Initialize libavcodec, and register all codecs and formats. */
		name = gen_filename(s, this->fmt, store_path, prefix, type, get_us(), f_nr++);
		create_path(name);
		register_file(name);

		if (!exec_start.empty() && is_start) {
			if (check_thread(&exec_start_th))
				exec_start_th = exec(exec_start, name);

			is_start = false;
		}
		else if (!exec_cycle.empty()) {
			if (check_thread(&exec_end_th))
				exec_end_th = exec(exec_cycle, name);
		}

		/* allocate the output media context */
		avformat_alloc_output_context2(&oc, nullptr, nullptr, name.c_str());
		if (!oc) {
			printf("Can't deduce output format from file extension: using MPEG");
			avformat_alloc_output_context2(&oc, nullptr, "mpeg", name.c_str());
		}

		if ((ret = av_set_options_string(opt, parameters.c_str(), "=", ":")) < 0)
			error_exit(false, "ffmpeg parameters are incorrect");

		fmt = oc->oformat;

		/* Add the audio and video streams using the default format codecs
		 * and initialize the codecs. */
		if (fmt->video_codec != AV_CODEC_ID_NONE) {
			add_stream(&video_st, oc, &video_codec, fmt->video_codec, override_fps > 0 ? override_fps : fps, bitrate, width, height);
			have_video = 1;
			encode_video = 1;
		}
		//if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		//	add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec, fps, bitrate, width, height);
		//	have_audio = 1;
		//	encode_audio = 1;
		//}

		if (!have_video)
			error_exit(false, "Target encoder does not have video capabilities");

		/* Now that all the parameters are set, we can open the audio and
		 * video codecs and allocate the necessary encode buffers. */
		if (have_video) {
			av_opt_set(opt, "preset", "fastest", 0);
			open_video(oc, video_codec, &video_st, opt);
		}

		//if (have_audio)
		//	open_audio(oc, audio_codec, &audio_st, opt);

		av_dump_format(oc, 0, name.c_str(), 1);

		/* open the output file, if needed */
		if (!(fmt->flags & AVFMT_NOFILE)) {
			ret = avio_open(&oc->pb, name.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				log(id, LL_ERR, "Can't open '%s': %s", name.c_str(), my_av_err2str(ret).c_str());
				break;
			}
		}

		/* Write the stream header, if any. */
		ret = avformat_write_header(oc, &opt);
		if (ret < 0) {
			log(id, LL_ERR, "Error occurred when opening output file: %s", my_av_err2str(ret).c_str());
			break;
		}

		bool file_fail = false;
		const time_t cut_ts = time(nullptr) + max_time;
		for(;;) {
			pauseCheck();
			st->track_fps();

			bool finish = (max_time > 0 && time(nullptr) >= cut_ts) || local_stop_flag;

			uint64_t start = get_us();

			/* select the stream to encode */
			//	if (encode_video &&
			//			(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
			//							audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
			if (!write_video_frame(oc, &video_st, s, &prev_ts, filters, &prev_frame, pre_record, cfg, is_view_proxy, handle_failure, sched))
				break;
			//	} else {
			//		encode_audio = !write_audio_frame(oc, &audio_st);
			//	}

			st->track_cpu_usage();

			if (finish)
				break;

			uint64_t took = get_us() - start;
			uint64_t left = interval * 1000000.0 - took;
			if (left > 0)
				mysleep(left, &local_stop_flag, s);
		}

		if (local_stop_flag) {
			while(write_video_frame(oc, &video_st, nullptr, &prev_ts, filters, &prev_frame, pre_record, cfg, is_view_proxy, handle_failure, sched)) {
			}
		}

		force_flush(oc, &video_st);

		/* Write the trailer, if any. The trailer must be written before you
		 * close the CodecContexts open when you wrote the header; otherwise
		 * av_write_trailer() may try to use memory that was freed on
		 * av_codec_close(). */
		if ((ret = av_write_trailer(oc)) != 0)
			log(id, LL_ERR, "Error occurred when flushing output file: %s", my_av_err2str(ret).c_str());

		/* Close each codec. */
		if (have_video)
			close_stream(oc, &video_st);
		//if (have_audio)
		//	close_stream(oc, &audio_st);

		if (!(fmt->flags & AVFMT_NOFILE)) {
			avio_flush(oc->pb);
			/* Close the output file */
			avio_closep(&oc->pb);
		}

		/* free the stream */
		avformat_free_context(oc);

		////////////////////////////////////////////////////////////////////////////
	}

	join_thread(&exec_start_th, id, "exec-start");

	if (!exec_end.empty()) {
		if (check_thread(&exec_end_th))
			exec_end_th = exec(exec_end, name);

		join_thread(&exec_end_th, id, "exec-end");
	}

	delete prev_frame;

	for(auto f : pre_record)
		delete f;

	pre_record.clear();

	s -> stop();
}
#endif
