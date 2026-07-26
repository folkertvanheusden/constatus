#include <stdint.h>
#include <cairo/cairo.h>

cairo_surface_t * rgb_to_cairo(const uint8_t *const in, const int w, const int h, uint32_t **temp);
void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out);
