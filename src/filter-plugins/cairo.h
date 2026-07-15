#include <stdint.h>
#include <utility>
#include <cairo/cairo.h>

std::pair<cairo_surface_t *, uint8_t *> rgb_to_cairo(const uint8_t *const in, const int w, const int h);
void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out);
