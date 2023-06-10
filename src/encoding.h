// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once

typedef enum { E_RGB, E_BGR, E_JPEG, E_YUYV } encoding_t;

void yuy2_to_rgb(const uint8_t *const in, const int width, const int height, uint8_t **out);
void rgb_to_yuy2(const uint8_t *const in, const int width, const int height, uint8_t **const out);
