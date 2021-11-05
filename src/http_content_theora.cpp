#include "config.h"

#ifdef HAVE_THEORA
#include "http_server.h"
#include "http_utils.h"
#include "http_content_theora.h"

int ilog(unsigned _v)
{
	int ret = 0;

	for(ret=0; _v; ret++)
		_v >>= 1;

	return ret;
}

theora_t *theora_init(const int w, const int h, const int fps, const int quality)
{
	theora_t *t = new theora_t();

	constexpr int keyframe_frequency = 64;
	th_info ti;

	th_info_init(&ti);
	ti.frame_width = ((w + 15) >>4)<<4;
	ti.frame_height = ((h + 15)>>4)<<4;
	ti.pic_width = w;
	ti.pic_height = h;
	ti.pic_x = 0;
	ti.pic_y = 0;
	ti.fps_numerator = fps;
	ti.fps_denominator = 1;
	ti.aspect_numerator = 0;
	ti.aspect_denominator = 0;
	ti.colorspace = TH_CS_UNSPECIFIED;
	ti.pixel_fmt = TH_PF_420;
	ti.target_bitrate = 90000;  // TODO configurable
	ti.quality = quality / 10;
	ti.keyframe_granule_shift = ilog(keyframe_frequency - 1);

	ogg_stream_init(&t->ss, rand());

	t->ctx = th_encode_alloc(&ti);

	return t;
}

void theora_uninit(theora_t *t)
{
	th_encode_free(t->ctx);
	delete t;
}

int theora_write_frame(theora_t *const t, h_handle_t & hh, int w, int h, uint8_t *yuv_y, uint8_t *yuv_u, uint8_t *yuv_v, int last)
{
	th_ycbcr_buffer ycbcr;
	ogg_packet op;
	ogg_page og;

	/* Must hold: yuv_w >= w */
	int yuv_w = (w + 15) & ~15;
	/* Must hold: yuv_h >= h */
	int yuv_h = (h + 15) & ~15;

	// Fill out the ycbcr buffer
	ycbcr[0].width = yuv_w;
	ycbcr[0].height = yuv_h;
	ycbcr[0].stride = yuv_w;
	ycbcr[1].width = yuv_w;
	ycbcr[1].stride = ycbcr[1].width;
	ycbcr[1].height = yuv_h;
	ycbcr[2].width = ycbcr[1].width;
	ycbcr[2].stride = ycbcr[1].stride;
	ycbcr[2].height = ycbcr[1].height;

	// Chroma is decimated by 2 in both directions
	ycbcr[1].width = yuv_w >> 1;
	ycbcr[2].width = yuv_w >> 1;
	ycbcr[1].height = yuv_h >> 1;
	ycbcr[2].height = yuv_h >> 1;

	ycbcr[0].data = yuv_y;
	ycbcr[1].data = yuv_u;
	ycbcr[2].data = yuv_v;

	/* Theora is a one-frame-in,one-frame-out system; submit a frame
	   for compression and pull out the packet */
	if (th_encode_ycbcr_in(t->ctx, ycbcr)) {
		//		fprintf(stderr, "[theora_write_frame] Error: could not encode frame\n");
		return -1;
	}

	if (!th_encode_packetout(t->ctx, last, &op)) {
		//		fprintf(stderr, "[theora_write_frame] Error: could not read packets\n");
		return -1;
	}

	ogg_stream_packetin(&t->ss, &op);
	ssize_t bytesWritten = 0;
	int pagesOut = 0;
	while(ogg_stream_pageout(&t->ss, &og)) {
		pagesOut ++;

		bytesWritten = WRITE_SSL(hh, (const char *)og.header, og.header_len);
		if(bytesWritten != og.header_len) {
			//			fprintf(stderr, "[theora_write_frame] Error: Could not write to file\n");
			return -1;
		}

		bytesWritten = WRITE_SSL(hh, (const char *)og.body, og.body_len);
		if(bytesWritten != og.body_len) {
			//			bytesWritten = fprintf(stderr, "[theora_write_frame] Error: Could not write to file\n");
			return -1;
		}
	}

	return pagesOut;
}
#endif
