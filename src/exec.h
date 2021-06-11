// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>

bool check_thread(std::thread **const handle);
std::thread * exec(const std::string & what, const std::string & parameter);
FILE * exec(const std::string & command_line);

void exec_with_pty(const std::string & command, int *const fd, pid_t *const pid);
