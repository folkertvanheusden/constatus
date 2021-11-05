#include "config.h"

#ifdef HAVE_THEORA

#include <theora/theoraenc.h>

typedef struct {
	ogg_stream_state ss;
	th_enc_ctx *ctx;
} theora_t;

theora_t *theora_init(const int w, const int h, const int fps, const int quality);
void theora_uninit(theora_t *t);

int theora_write_frame(theora_t *const t, h_handle_t & hh, int w, int h, uint8_t *yuv_y, uint8_t *yuv_u, uint8_t *yuv_v, int last);

#endif
