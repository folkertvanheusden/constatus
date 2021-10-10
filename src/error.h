// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
void error_exit(const bool se, const char *format, ...) __attribute__ ((__noreturn__));
void cpt(const int err);
