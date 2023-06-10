// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
void error_exit(const bool se, const char *format, ...) __attribute__ ((__noreturn__));
void cpt(const int err);
