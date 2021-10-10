// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <stddef.h>
#include <cstring>

#include "gen.h"
#include "filter_despeckle.h"
#include "utils.h"

void dilation(const uint8_t *const in, uint8_t *const gray, const int w, const int h, const int kernel_size, uint8_t *const out)
{
	int half_kernel = kernel_size / 2;

	for(int y=half_kernel; y<h-half_kernel; y++) {
		for(int x=half_kernel; x<w-half_kernel; x++) {
			int best = -1, r, g, b;

			for(int ky=y-half_kernel; ky<y+half_kernel; ky++) {
				for(int kx=x-half_kernel; kx<x+half_kernel; kx++) {
					int offset = ky * w + kx;

					if (gray[offset] > best) {
						best = gray[offset];

						r = in[offset * 3 + 0];
						g = in[offset * 3 + 1];
						b = in[offset * 3 + 2];
					}
				}
			}

			int offset = y * w * 3 + x * 3;
			out[offset + 0] = r;
			out[offset + 1] = g;
			out[offset + 2] = b;

			gray[y * w + x] = best;
		}
	}
}

void erode(const uint8_t *const in, uint8_t *const gray, const int w, const int h, const int kernel_size, uint8_t *const out)
{
	int half_kernel = kernel_size / 2;

	for(int y=half_kernel; y<h-half_kernel; y++) {
		for(int x=half_kernel; x<w-half_kernel; x++) {
			int worst = 256, r, g, b;

			for(int ky=y-half_kernel; ky<y+half_kernel; ky++) {
				for(int kx=x-half_kernel; kx<x+half_kernel; kx++) {
					int offset = ky * w + kx;

					if (gray[offset] < worst) {
						worst = gray[offset];

						r = in[offset * 3 + 0];
						g = in[offset * 3 + 1];
						b = in[offset * 3 + 2];
					}
				}
			}

			int offset = y * w * 3 + x * 3;
			out[offset + 0] = r;
			out[offset + 1] = g;
			out[offset + 2] = b;

			gray[y * w + x] = worst;
		}
	}
}

filter_despeckle::filter_despeckle(const std::string & pattern) : pattern(pattern)
{
}

filter_despeckle::~filter_despeckle()
{
}

void make_gray(const uint8_t *const in, const size_t n, uint8_t *const out)
{
	for(size_t i=0; i<n; i++) {
		size_t offset = i * 3;

		out[i] = (in[offset + 0] + in[offset + 1] + in[offset + 2]) / 3;
	}
}

void filter_despeckle::apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	size_t n = size_t(w) * size_t(h);
	size_t n3 = n * size_t(3);
	uint8_t *temp1 = (uint8_t *)malloc(n3);
	uint8_t *temp2 = (uint8_t *)malloc(n3);

	uint8_t *gray = (uint8_t *)malloc(n);

	memcpy(temp1, in, n3);

	for(auto c : pattern) {
		make_gray(temp1, n, gray);

		if (c == 'd')
			dilation(temp1, gray, w, h, 5, temp2);
		else if (c == 'e')
			erode(temp1, gray, w, h, 5, temp2);
		else if (c == 'D')
			dilation(temp1, gray, w, h, 9, temp2);
		else if (c == 'E')
			erode(temp1, gray, w, h, 9, temp2);

		uint8_t *temp = temp1;
		temp1 = temp2;
		temp2 = temp;
	}

	memcpy(out, temp1, n3);

	int half_kernel = 9 / 2;

	for(int x=0; x<w; x++) {
		for(int y=0; y<half_kernel; y++) {
			int offset = y * w * 3 + x * 3;
			out[offset + 0] = 0;
			out[offset + 1] = 0;
			out[offset + 2] = 0;
		}

		for(int y=h-half_kernel; y<h; y++) {
			int offset = y * w * 3 + x * 3;
			out[offset + 0] = 0;
			out[offset + 1] = 0;
			out[offset + 2] = 0;
		}
	}

	for(int y=0; y<h; y++) {
		for(int x=0; x<half_kernel; x++) {
			int offset = y * w * 3 + x * 3;
			out[offset + 0] = 0;
			out[offset + 1] = 0;
			out[offset + 2] = 0;
		}

		for(int x=w-half_kernel; x<w; x++) {
			int offset = y * w * 3 + x * 3;
			out[offset + 0] = 0;
			out[offset + 1] = 0;
			out[offset + 2] = 0;
		}
	}

	free(gray);

	free(temp2);
	free(temp1);
}
