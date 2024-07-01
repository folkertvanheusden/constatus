// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "gen.h"
#include <stdint.h>
#include <stdio.h>
#include <turbojpeg.h>

class meta;

bool read_PNG_file_rgba(bool with_alpha, FILE *fh, int *w, int *h, uint8_t **pixels);
void write_PNG_file(FILE *fh, int ncols, int nrows, unsigned char *pixels);

void load_PBM_file(FILE *const fh, int *const w, int *const h, uint8_t **out);

#define transformer_t tjhandle

class myjpeg
{
private:
	tjhandle jpegDecompressor, jpegCompressor;

public:
	myjpeg();
	virtual ~myjpeg();

	bool write_JPEG_memory(const meta *const m, const int ncols, const int nrows, const int quality, const uint8_t *const pixels, uint8_t **out, size_t *out_len);
	bool read_JPEG_memory(unsigned char *in, int n_bytes_in, int *w, int *h, unsigned char **pixels);

	static void rgb_to_i420(transformer_t t, const uint8_t *const in, const int width, const int height, uint8_t **const out, uint8_t **const y = nullptr, uint8_t **const u = nullptr, uint8_t **const v = nullptr, const bool swap_rgb = false);
	static void i420_to_rgb(transformer_t t, const uint8_t *const in, const int width, const int height, uint8_t **const out);
	static transformer_t allocate_transformer();
	static void free_transformer(transformer_t h);
};

extern thread_local myjpeg my_jpeg;

bool read_bmp(unsigned char *in, int n_bytes_in, int *w, int *h, unsigned char **pixels);
