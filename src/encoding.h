// (C) 2017-2021 by folkert van heusden, released under AGPL v3.0
#pragma once

typedef enum { E_RGB, E_JPEG } encoding_t;

void yuy2_to_rgb(const uint8_t *const in, const int width, const int height, uint8_t **out);
