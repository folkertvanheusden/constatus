// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include <string>
#include <thread>

bool check_thread(std::thread **const handle);
std::thread * exec(const std::string & what, const std::string & parameter);
FILE * exec(const std::string & command_line);

void exec_with_pty(const std::string & command, int *const fd, pid_t *const pid);

void fire_and_forget(const std::string & command, const std::string & argument);
