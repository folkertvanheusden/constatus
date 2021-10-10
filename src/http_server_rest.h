// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "http_server.h"

void run_rest(h_handle_t & hh, configuration_t *const cfg, const std::string & path, const std::map<std::string, std::string> & pars, const std::string & snapshot_dir, const int quality);
