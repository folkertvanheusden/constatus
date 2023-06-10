// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
void draw_horizonal(uint8_t *const target, const int tw, const int x, const int y, const int w, const rgb_t col);
void draw_vertical(uint8_t *const target, const int tw, const int x, const int y, const int h, const rgb_t col);
void draw_box(uint8_t *const target, const int tw, const int x1, const int y1, const int x2, const int y2, const rgb_t col);
