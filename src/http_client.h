// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <string>

bool http_get(const std::string & url, const bool ignore_cert, const char *const auth, const bool verbose, uint8_t **const out, size_t *const out_n, std::atomic_bool *const stop_flag);
