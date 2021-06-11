// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <syslog.h>

#include "log.h"
#include "utils.h"

void error_exit(const bool se, const char *format, ...)
{
	int e = errno;
	va_list ap;

	va_start(ap, format);
	char *temp = NULL;
	vasprintf(&temp, format, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", temp);
	lognee(LL_ERR, "%s", temp);
	syslog(LOG_ERR, "%s", temp);

	if (se && e) {
		fprintf(stderr, "errno: %d (%s)\n", e, strerror(e));
		lognee(LL_ERR, "errno: %d (%s)", e, strerror(e));
		syslog(LOG_ERR, "errno: %d (%s)", e, strerror(e));
	}

	free(temp);

	exit(EXIT_FAILURE);
}

void cpt(const int err)
{
	if (err)
		error_exit(true, "pthread error %s", strerror(errno));
}
