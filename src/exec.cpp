// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <errno.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "log.h"
#include "utils.h"

bool check_thread(std::thread **const handle)
{
	if (*handle == nullptr)
		return true;

	if ((*handle)->joinable()) {
		(*handle)->join();
		delete *handle;
		*handle = nullptr;

		return true;
	}

	return false;
}

std::thread * exec(const std::string & what, const std::string & parameter)
{
	if (!what.empty()) {
		return new std::thread([what, parameter]{
			set_thread_name("exec" + what);

			if (system((what + " " + parameter).c_str()) == -1)
				log(LL_ERR, "system(%s) failed (%s)", what.c_str(), strerror(errno));
		});
	}

	return nullptr;
}

FILE * exec(const std::string & command_line)
{
	FILE *fh = popen(command_line.c_str(), "r");

	if (!fh)
		error_exit(true, "Cannot exec %s", command_line.c_str());

	return fh;
}

void setup_for_childproc(const int fd, const bool close_fd_0, const std::string & term)
{
	if (close_fd_0)
		close(0);
	close(1);
	close(2);

	bool fail = false;

	if (close_fd_0)
		fail |= dup(fd) == -1;
	fail |= dup(fd) == -1;
	fail |= dup(fd) == -1;

	if (fail)
		error_exit(true, "setup_for_childproc: cannot dup() filedescriptors\n");

	char *dummy = nullptr;
	if (asprintf(&dummy, "TERM=%s", term.c_str()) == -1)
		error_exit(true, "asprint failed (needed to set environment variables)\n");

	if (putenv(dummy) == -1)
		error_exit(true, "setup_for_childproc: Could not set TERM environment-variable (%s)\n", dummy);

	umask(007);
}

int get_pty_and_fork(int *const fd_master, int *const fd_slave)
{
	if (openpty(fd_master, fd_slave, NULL, NULL, NULL) == -1)
		error_exit(true, "openpty() failed");

	return fork();
}

void exec_with_pty(const std::string & command, int *const fd, pid_t *const pid)
{
	int fd_master = -1, fd_slave = -1;

	*pid = get_pty_and_fork(&fd_master, &fd_slave);
	if (*pid == -1)
		error_exit(true, "An error occured while invoking get_pty_and_fork");

	if (*pid == 0) {
		setpgid(0, 0);

		close(fd_master);

		/* connect slave-fd to stdin/out/err */
		setup_for_childproc(fd_slave, 1, "dumb");

		/* start process */
		if (execlp("/bin/sh", "/bin/sh", "-c", command.c_str(), (void *)nullptr) == -1)
			error_exit(true, "Error while starting '/bin/sh -c \"%s\"'", command.c_str());
	}

	*fd = fd_master;

	close(fd_slave);
}

void fire_and_forget(const std::string & command, const std::string & argument)
{
	pid_t pid = fork();

	if (pid == 0) {
		if (execlp("/bin/sh", "/bin/sh", "-c", argument.empty() ? command.c_str() : (command + " " + argument).c_str(), (void *)nullptr) == -1)
			error_exit(true, "Error while starting '/bin/sh -c \"%s\"'", command.c_str());
	}
	else if (pid == -1) {
		log(LL_ERR, "fork() failed (%s)", strerror(errno));
	}
}
