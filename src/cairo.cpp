#include <stdint.h>
#include <stdlib.h>

#include <cairo/cairo.h>

cairo_surface_t * rgb_to_cairo(const uint8_t *const in, const int w, const int h, uint32_t **temp)
{
	size_t n = w * h;
	*temp = (uint32_t *)valloc(n * 4);

	const uint8_t *win = in;
	uint32_t *wout = *temp;

	for(size_t i=0; i<n; i++) {
		uint8_t r = *win++;
		uint8_t g = *win++;
		uint8_t b = *win++;
		*wout++ = (255 << 24) | (r << 16) | (g << 8) | b;
	}

	return cairo_image_surface_create_for_data((unsigned char *)*temp, CAIRO_FORMAT_RGB24, w, h, 4 * w);
}

void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out)
{
	const unsigned char *data = cairo_image_surface_get_data(cs);
	const uint32_t *in = (const uint32_t *)data;

	size_t n = w * h;
	for(size_t i=0; i<n; i++) {
		uint32_t temp = *in++;
		*out++ = temp >> 16;
		*out++ = temp >> 8;
		*out++ = temp;
	}
}
