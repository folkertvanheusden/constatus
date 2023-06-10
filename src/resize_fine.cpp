// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#include <stdlib.h>
#include <cstring>

#include "resize_fine.h"
#include "log.h"
#include "utils.h"

resize_fine::resize_fine()
{
	log(LL_INFO, "resize_fine instantiated");
}

resize_fine::~resize_fine()
{
}

typedef struct {
	double n;
	double r, g, b;
} pixel_t;

void resize_fine::do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	*out = (uint8_t *)malloc(IMS(wout, hout, 3));

	const double x_scale = win / double(wout);
	const double y_scale = hin / double(hout);

//	printf("scale: %f,%f\n", x_scale, y_scale);

	pixel_t *work = new pixel_t[wout * hout]();

	for(int y=0; y<hout; y++) {
		const double in_y_offset = y * y_scale, end_y = in_y_offset + y_scale;

		for(double in_y = in_y_offset; in_y < end_y; in_y += 1.0) {
			double mul_y = std::min(end_y - in_y, 1.0);
			int o_y = int(in_y) * win * 3;

			for(int x=0; x<wout; x++) {
				const double in_x_offset = x * x_scale, end_x = in_x_offset + x_scale;

				int put_offset = y * wout + x;

				for(double in_x = in_x_offset; in_x < end_x; in_x += 1.0) {
					double mul_x = std::min(end_x - in_x, 1.0);
					double mul = mul_x * mul_y;

					int get_offset = o_y + int(in_x) * 3;

					work[put_offset].n += mul;
					work[put_offset].r += mul * in[get_offset + 0];
					work[put_offset].g += mul * in[get_offset + 1];
					work[put_offset].b += mul * in[get_offset + 2];
				}
			}
		}
	}

	for(int y=0; y<hout; y++) {
		int yo = y * wout, yo3 = yo * 3;

		for(int x=0, i = yo, o = yo3; x<wout; x++, i++, o += 3) {
			if (work[i].n) {
				(*out)[o + 0] = work[i].r / work[i].n;
				(*out)[o + 1] = work[i].g / work[i].n;
				(*out)[o + 2] = work[i].b / work[i].n;
			}
			else {
				(*out)[o + 0] =
				(*out)[o + 1] =
				(*out)[o + 2] = 0;
			}
		}
	}

	delete [] work;
}
