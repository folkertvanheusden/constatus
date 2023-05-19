// (C) 2017-2022 by folkert van heusden, released under Apache License v2.0
// with code from https://stackoverflow.com/questions/39536746/ffmpeg-leak-while-reading-image-files
#include "config.h"
#if HAVE_FFMPEG == 1
#include <atomic>
#include <string>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#include "source_ffmpeg.h"
#include "error.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

static bool v = false;

source_ffmpeg::source_ffmpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & url, const bool tcp, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, loglevel, timeout, filters, failure, c, jpeg_quality, text_feeds, keep_aspectratio), url(url), tcp(tcp)
{
	v = loglevel >= LL_DEBUG;
}

source_ffmpeg::~source_ffmpeg()
{
	stop();
	delete c;
}

void my_log_callback(void *ptr, int level, const char *fmt, va_list vargs)
{
	if (level < AV_LOG_WARNING) {
		char *buffer = NULL;
		if (vasprintf(&buffer, fmt, vargs) == -1) {
			log(LL_DEBUG, "%s", fmt);  // last resort
		}
		else {
			char *lf = strchr(buffer, '\n');
			if (lf)
				*lf = ' ';

			log(LL_DEBUG, buffer);

			free(buffer);
		}
	}
}

int interrupt_cb(void *ctx)
{
	std::atomic_bool *stop_flag = (std::atomic_bool *)ctx;

	if (*stop_flag)
		log(LL_INFO, "Interrupting ffmpeg by stop flag");

	return *stop_flag ? 1 : 0;
}

void source_ffmpeg::operator()()
{
	log(id, LL_INFO, "source rtsp thread started");

	set_thread_name("src_stream");

	av_log_set_level(AV_LOG_FATAL);

	av_log_set_callback(my_log_callback);

	avformat_network_init();

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	for(;;) {
		uint64_t session_start = get_us();

		int err = 0, video_stream_index = -1;
		AVDictionary *opts = NULL;
		AVCodecContext *codec_ctx = NULL;
		AVFormatContext *output_ctx = NULL, *format_ctx = NULL;
		AVStream *stream = NULL;
		AVPacket packet;
		const AVCodec *codec = NULL;
		uint8_t *pixels = NULL;
		SwsContext *img_convert_ctx = NULL;
		int size = 0, size2 = 0;
		AVFrame *picture = NULL, *picture_rgb = NULL;
		uint8_t *picture_buffer = NULL, *picture_buffer_2 = NULL;
		bool do_get = false;
		uint64_t now_ts = 0, next_frame_ts = 0;
		AVIOInterruptCB int_cb = { interrupt_cb, &local_stop_flag };

		av_init_packet(&packet);

		format_ctx = avformat_alloc_context();
		if (!format_ctx) {
			set_error("avformat_alloc_context fails", true);
			goto fail;
		}

		format_ctx->interrupt_callback = int_cb;

		if (tcp)
			av_dict_set(&opts, "rtsp_transport", "tcp", 0);
		av_dict_set(&opts, "max_delay", "100000", 0);  //100000 is the default
		av_dict_set(&opts, "analyzeduration", "5000000", 0);
		av_dict_set(&opts, "probesize", "32000000", 0);
		av_dict_set(&opts, "user-agent", NAME " " VERSION, 0);

		char to_buf[32];
		snprintf(to_buf, sizeof to_buf, "%ld", long(timeout * 1000 * 1000));
		av_dict_set(&opts, "stimeout", to_buf, 0);

		// open RTSP
		if ((err = avformat_open_input(&format_ctx, url.c_str(), NULL, &opts)) != 0) {
			char err_buffer[4096];
			av_strerror(err, err_buffer, sizeof err_buffer);

			set_error(myformat("Cannot open %s (%s (%d))", url.c_str(), err_buffer, err), true);

			do_exec_failure();

			av_dict_free(&opts);

			goto fail;
		}

		av_dict_free(&opts);

		if (avformat_find_stream_info(format_ctx, NULL) < 0) {
			set_error("avformat_find_stream_info failed (rtsp)", false);
			goto fail;
		}

		// search video stream
		video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		if (video_stream_index == -1) {
			set_error("No video stream in rstp feed", false);
			goto fail;
		}

		output_ctx = avformat_alloc_context();

//		av_read_play(format_ctx);

		///////
		// Get the codec
		codec = avcodec_find_decoder(format_ctx->streams[video_stream_index]->codecpar->codec_id);
		if (!codec) {
			set_error("Decoder not found (rtsp)", true);
			goto fail;
		}

		// Add this to allocate the context by codec
		codec_ctx = avcodec_alloc_context3(codec);
		//avcodec_get_context_defaults3(codec_ctx, codec);
		avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_index]->codecpar);

		if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
			set_error("avcodec_open2 failed", false);
			goto fail;
		}
		///////////

		if (need_scale())
			set_size(resize_w, resize_h);
		else {
			log(id, LL_INFO, "Resolution: %dx%d", codec_ctx -> width, codec_ctx -> height);

			if (codec_ctx -> width <= 0 || codec_ctx -> height <= 0) {
				set_error("Invalid resolution", false);
				goto fail;
			}

			set_size(codec_ctx -> width, codec_ctx -> height);
		}

		pixels = (uint8_t *)malloc(IMS(codec_ctx -> width, codec_ctx -> height, 3));

		img_convert_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

		size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);
		picture_buffer = (uint8_t*) (av_malloc(size));
		picture = av_frame_alloc();
		picture_rgb = av_frame_alloc();
		size2 = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
		picture_buffer_2 = (uint8_t*) (av_malloc(size2));
		av_image_fill_arrays(picture -> data, picture -> linesize, picture_buffer, AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);
		av_image_fill_arrays(picture_rgb -> data, picture_rgb -> linesize, picture_buffer_2, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

		next_frame_ts = get_us();

		while(!local_stop_flag && av_read_frame(format_ctx, &packet) >= 0) {
			if (packet.stream_index == video_stream_index) {    //packet is video
				if (stream == NULL) {    //create stream in file
					log(id, LL_DEBUG, "Create stream");

					stream = avformat_new_stream(output_ctx, NULL);

					avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_index]->codecpar);

					stream->sample_aspect_ratio = format_ctx->streams[video_stream_index]->codecpar->sample_aspect_ratio;
				}

				packet.stream_index = stream->id;

				if (avcodec_send_packet(codec_ctx, &packet) < 0) {
					set_error("rtsp error", false);
					goto fail;
				}

				int result = avcodec_receive_frame(codec_ctx, picture);
				if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
					set_error(myformat("rtsp error %d", result), false);
					goto fail;
				}

				do_get = false;

				// framerate limiter
				now_ts = get_us();
				if (now_ts >= next_frame_ts || interval == 0) {
					do_get = true;
					next_frame_ts += interval;
				}

				if (work_required() && do_get && !paused) {
					sws_scale(img_convert_ctx, picture->data, picture->linesize, 0, codec_ctx->height, picture_rgb->data, picture_rgb->linesize);

					for(int y = 0; y < codec_ctx->height; y++) {
						uint8_t *out_pointer = &pixels[y * codec_ctx -> width * 3];
						uint8_t *in_pointer = picture_rgb->data[0] + y * picture_rgb->linesize[0];

						memcpy(&out_pointer[0], &in_pointer[0], codec_ctx->width * 3);
					}

					if (need_scale())
						set_scaled_frame(pixels, codec_ctx -> width, codec_ctx -> height, keep_aspectratio);
					else
						set_frame(E_RGB, pixels, IMS(codec_ctx -> width, codec_ctx -> height, 3));

					clear_error();
				}
			}

			av_frame_unref(picture);

			av_packet_unref(&packet);
			av_init_packet(&packet);

			st->track_cpu_usage();
		}

	fail:
		av_packet_unref(&packet);

		av_free(picture);
		av_free(picture_rgb);
		av_free(picture_buffer);
		av_free(picture_buffer_2);

//		av_read_pause(format_ctx);
		avformat_close_input(&format_ctx);

		if (output_ctx)
			avio_close(output_ctx->pb);

		avformat_free_context(output_ctx);

		sws_freeContext(img_convert_ctx);

		avcodec_free_context(&codec_ctx);

		free(pixels);

		if (local_stop_flag)
			break;

		uint64_t session_end = get_us();

		// when the session took less than a second, wait
		// for a second, else 100ms
		if (session_end - session_start < 1000000)
			myusleep(1000000);
		else
			myusleep(100000);
	}

	register_thread_end("source_rtsp/ffmpeg");
}
#endif
