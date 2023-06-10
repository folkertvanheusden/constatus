// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "http_server.h"

void run_rest(h_handle_t & hh, configuration_t *const cfg, const std::string & path, const std::map<std::string, std::string> & pars, const std::string & snapshot_dir, const int quality);
