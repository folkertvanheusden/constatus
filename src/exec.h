// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <string>

bool check_thread(std::thread **const handle);
std::thread * exec(const std::string & what, const std::string & parameter);
FILE * exec(const std::string & command_line);

void exec_with_pty(const std::string & command, int *const fd, pid_t *const pid);
