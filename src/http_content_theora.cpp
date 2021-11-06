#include "config.h"

#if HAVE_THEORA == 1
#include "http_server.h"
#include "http_utils.h"
#include "http_content_theora.h"
#include "log.h"

theora_t *theora_init(const int w, const int h, const int fps, const int quality, h_handle_t & hh)
{
	theora_t *t = new theora_t();

	int keyframe_frequency = 64;

	th_info_init(&t->ti);
	t->ti.frame_width = ((w + 15) >>4)<<4;
	t->ti.frame_height = ((h + 15)>>4)<<4;
	t->ti.pic_width = w;
	t->ti.pic_height = h;
	t->ti.pic_x = 0;
	t->ti.pic_y = 0;
	t->ti.fps_numerator = fps;
	t->ti.fps_denominator = 1;
	t->ti.aspect_numerator = 0;
	t->ti.aspect_denominator = 0;
	t->ti.colorspace = TH_CS_UNSPECIFIED;
	t->ti.pixel_fmt = TH_PF_420;
	t->ti.target_bitrate = 2000000;  // TODO configurable
	t->ti.quality = quality * 63 / 100;

	t->ctx = th_encode_alloc(&t->ti);
	th_info_clear(&t->ti);

	ogg_stream_init(&t->ss, rand());

        ogg_packet op { 0 };
        ogg_page og { 0 };

	/* write the bitstream header packets with proper page interleave */
	th_comment tc;
	th_comment_init(&tc);
	/* first packet will get its own page automatically */
	if (th_encode_flushheader(t->ctx,&tc,&op) <= 0)
		log(LL_ERR, "Internal Theora library error");
	th_comment_clear(&tc);

	if (ogg_stream_packetin(&t->ss, &op) == -1)
		log(LL_ERR, "ogg_stream_packetin failed");

	if (ogg_stream_pageout(&t->ss,&og)!=1)
		log(LL_ERR, "Internal Ogg library error");
	WRITE_SSL(hh, (const char *)og.header, og.header_len);
	WRITE_SSL(hh, (const char *)og.body, og.body_len);

	// remaining headers
	for(;;) {
		int result = th_encode_flushheader(t->ctx,&tc,&op);
		if(result < 0)
			log(LL_ERR, "Internal Ogg library error");

		if (result == 0)
			break;

		if (ogg_stream_packetin(&t->ss,&op) == -1)
			log(LL_ERR, "ogg_stream_packetin failed");
	}

	for(;;) {
		int result = ogg_stream_flush(&t->ss,&og);
		if (result < 0)
			log(LL_ERR, "Internal Ogg library error");

		if (result == 0)
			break;

		WRITE_SSL(hh, (const char *)og.header, og.header_len);
		WRITE_SSL(hh, (const char *)og.body, og.body_len);
	}

	return t;
}

void theora_uninit(theora_t *t)
{
	th_encode_free(t->ctx);
	delete t;
}

int theora_write_frame(theora_t *const t, h_handle_t & hh, int w, int h, uint8_t *yuv_y, uint8_t *yuv_u, uint8_t *yuv_v, int last)
{
        ogg_packet op { 0 };
        ogg_page og { 0 };

	th_ycbcr_buffer ycbcr { 0 };

	/* Must hold: yuv_w >= w */
	int yuv_w = (w + 15) & ~15;
	/* Must hold: yuv_h >= h */
	int yuv_h = (h + 15) & ~15;

	// Fill out the ycbcr buffer
	ycbcr[0].width = yuv_w;
	ycbcr[0].height = yuv_h;
	ycbcr[0].stride = yuv_w;

	// Chroma is decimated by 2 in both directions
	ycbcr[1].width = yuv_w >> 1;
	ycbcr[1].stride = ycbcr[1].width;
	ycbcr[1].height = yuv_h >> 1;

	ycbcr[2].width = ycbcr[1].width;
	ycbcr[2].stride = ycbcr[1].stride;
	ycbcr[2].height = ycbcr[1].height;

	ycbcr[0].data = yuv_y;
	ycbcr[1].data = yuv_u;
	ycbcr[2].data = yuv_v;

	/* Theora is a one-frame-in,one-frame-out system; submit a frame
	   for compression and pull out the packet */
	if (th_encode_ycbcr_in(t->ctx, ycbcr)) {
		log(LL_ERR, "[theora_write_frame] Error: could not encode frame");
		return -1;
	}

	if (!th_encode_packetout(t->ctx, last, &op)) {
		log(LL_ERR, "th_encode_packetout failed");
		return -1;
	}

	if (ogg_stream_packetin(&t->ss, &op) == -1)
		log(LL_ERR, "ogg_stream_packetin failed");

	while(ogg_stream_pageout(&t->ss, &og)) {
		if (WRITE_SSL(hh, (const char *)og.header, og.header_len) != og.header_len) {
			log(LL_ERR, "[theora_write_frame] Error: Could not write to file\n");
			return -1;
		}

		if (WRITE_SSL(hh, (const char *)og.body, og.body_len) != og.body_len) {
			log(LL_ERR, "[theora_write_frame] Error: Could not write to file\n");
			return -1;
		}
	}

	return 1;
}
#endif
