// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include <algorithm>
#include <stdint.h>

// based on https://stackoverflow.com/questions/4491649/how-to-convert-yuy2-to-a-bitmap-in-c
void yuy2_to_rgb(const uint8_t *const in, const int width, const int height, uint8_t **out)
{
	const uint8_t *in_work = in;
	const int n_pixels = width * height;
	const int n_bytes_out = n_pixels * 3;

	uint8_t *out_work = *out = (uint8_t *)malloc(n_bytes_out);

	for(int y=0; y<height; y++) {
		for(int i = 0; i < width/2; i++) {
			int y0 = in_work[0];
			int u0 = in_work[1];
			int y1 = in_work[2];
			int v0 = in_work[3];
			in_work += 4;

			int c = y0 - 16;
			int d = u0 - 128;
			int e = v0 - 128;
			out_work[0] = std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255); // red
			out_work[1] = std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255); // green
			out_work[2] = std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255); // blue

			c = y1 - 16;
			out_work[3] = std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255); // red
			out_work[4] = std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255); // green
			out_work[5] = std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255); // blue

			out_work += 6;
		}
	}
}
