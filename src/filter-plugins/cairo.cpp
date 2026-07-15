#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <cairo/cairo.h>


std::pair<cairo_surface_t *, uint8_t *> rgb_to_cairo(const uint8_t *const in, const int w, const int h)
{
	int n_bytes = w * h * 4;
	uint8_t *temp = (uint8_t *)malloc(n_bytes);
	memcpy(temp, in, n_bytes);
	uint8_t *work = temp;
	uint8_t *const end = &temp[n_bytes];
	while(work < end) {
		std::swap(work[2], work[0]);
		work += 4;
	}

	return { cairo_image_surface_create_for_data(temp, CAIRO_FORMAT_RGB24, w, h, 4 * w), temp };
}

void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out)
{
	const unsigned char *data = cairo_image_surface_get_data(cs);
	int n_bytes = w * h * 4;
	memcpy(out, data, n_bytes);

	uint8_t *work = out;
	uint8_t *const end = &out[n_bytes];
	while(work < end) {
		std::swap(work[2], work[0]);
		work += 4;
	}
}
