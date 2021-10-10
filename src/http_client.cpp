// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <syslog.h>
#include <curl/curl.h>

#include "error.h"
#include "log.h"

typedef struct
{
	uint8_t *p;
	size_t len;
	std::atomic_bool *const stop_flag;
} curl_data_t;

static int xfer_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	curl_data_t *w = (curl_data_t *)clientp;

	if (w -> stop_flag && *w -> stop_flag)
		return 1;

	return 0;
}

static size_t curl_write_data(void *ptr, size_t size, size_t nmemb, void *ctx)
{
	curl_data_t *pctx = (curl_data_t *)ctx;

	size_t n = size * nmemb;

	size_t tl = pctx -> len + n;
	pctx -> p = (uint8_t *)realloc(pctx -> p, tl);
	if (!pctx -> p)
		error_exit(true, "Cannot allocate %zu bytes of memory", tl);

	memcpy(&pctx -> p[pctx -> len], ptr, n);
	pctx -> len += n;

	// time-out?
	if (pctx -> stop_flag && *pctx -> stop_flag)
		return 0;

	return n;
}

bool http_get(const std::string & url, const bool ignore_cert, const char *const auth, const bool verbose, uint8_t **const out, size_t *const out_n, std::atomic_bool *const stop_flag)
{
	CURL *ch = curl_easy_init();
	if (!ch)
		error_exit(false, "Failed to initialize CURL session");

	char error[CURL_ERROR_SIZE] = "?";
	if (curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, error))
		error_exit(false, "curl_easy_setopt(CURLOPT_ERRORBUFFER) failed: %s", error);

	curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, curl_log);

	long timeout = 15;
	curl_data_t data = { NULL, 0, stop_flag };

	std::string useragent = NAME " " VERSION;

	if (ignore_cert) {
		if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0))
			error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed: %s", error);

		if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0))
			error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYHOST) failed: %s", error);
	}

	if (auth) {
		curl_easy_setopt(ch, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
 
		curl_easy_setopt(ch, CURLOPT_USERPWD, auth);
	}

	if (curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L))
		error_exit(false, "curl_easy_setopt(CURLOPT_FOLLOWLOCATION) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_USERAGENT, useragent.c_str()))
		error_exit(false, "curl_easy_setopt(CURLOPT_USERAGENT) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_URL, url.c_str()))
		error_exit(false, "curl_easy_setopt(CURLOPT_URL) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, timeout))
		error_exit(false, "curl_easy_setopt(CURLOPT_CONNECTTIMEOUT) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_write_data))
		error_exit(false, "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_WRITEDATA, &data))
		error_exit(false, "curl_easy_setopt(CURLOPT_WRITEDATA) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_VERBOSE, verbose))
		error_exit(false, "curl_easy_setopt(CURLOPT_VERBOSE) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_XFERINFODATA, &data))
		error_exit(false, "curl_easy_setopt(CURLOPT_XFERINFODATA) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_XFERINFOFUNCTION, xfer_callback))
		error_exit(false, "curl_easy_setopt(CURLOPT_XFERINFOFUNCTION) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0L))
		error_exit(false, "curl_easy_setopt(CURLOPT_NOPROGRESS) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPALIVE, 1L))
		error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPALIVE) failed: %s", error);
 
	if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPIDLE, 120L))
		error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPIDLE) failed: %s", error);
 
	if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPINTVL, 60L))
		error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPINTVL) failed: %s", error);

	curl_easy_setopt(ch, CURLOPT_TCP_FASTOPEN, 1);

	bool ok = true;
	if (curl_easy_perform(ch)) {
		free(data.p);
		data.p = NULL;
		data.len = 0;
		log(LL_ERR, "curl_easy_perform() failed: %s", error);
		ok = false;
	}

	curl_easy_cleanup(ch);

	*out = data.p;
	*out_n = data.len;

	return ok;
}
