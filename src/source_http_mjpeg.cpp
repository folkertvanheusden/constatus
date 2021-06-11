// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <assert.h>
#include <string>
#include <cstring>
#include <unistd.h>

#if HAVE_LIBCURL == 1
#include <curl/curl.h>
#endif

#include "utils.h"
#include "source.h"
#include "source_http_mjpeg.h"
#include "picio.h"
#include "error.h"
#include "log.h"
#include "controls.h"

typedef struct
{
	std::string id;
	uint8_t *data;
	size_t len;
	char *boundary;
	uint64_t latest_io;
	stats_tracker *st;
} work_header_t;

static size_t write_headers(void *ptr, size_t size, size_t nmemb, void *mypt)
{
	work_header_t *pctx = (work_header_t *)mypt;

	size_t n = size * nmemb;

	pctx->st->track_bw(n);

	size_t tl = pctx -> len + n;
	pctx -> data = (uint8_t *)realloc(pctx -> data, tl + 1);
	if (!pctx -> data)
		error_exit(true, "HTTP headers: cannot allocate %zu bytes of memory", tl);

	memcpy(&pctx -> data[pctx -> len], ptr, n);
	pctx -> len += n;
	pctx -> data[pctx -> len] = 0x00;

	// 512kB header limit
	if (pctx -> len >= 512 * 1024) {
		log(pctx -> id, LL_INFO, "headers too large");
		return 0;
	}

//	printf("header %s\n", pctx -> data);

	char *p = (char *)pctx -> data, *em = NULL;
	char *ContentType = strcasestr(p, "Content-Type:");
	if (ContentType) {
		em = strstr(ContentType, "\r\n");
		if (!em)
			em = strstr(ContentType, "\n");

		if (em != nullptr) {
			char *temp = strndup(ContentType, em - ContentType);
			//printf("|%s|\n", temp);

			char *bs = strchr(temp, '=');
			if (bs) {
				free(pctx -> boundary);

				if (bs[1] == '"')
					pctx -> boundary = strdup(bs + 2);
				else
					pctx -> boundary = strdup(bs + 1);

				char *dq = strchr(pctx -> boundary, '"');
				if (dq)
					*dq = 0x00;

				log(pctx -> id, LL_DEBUG, "HTTP boundary header is %s\n", pctx -> boundary);
			}

			free(temp);
		}
	}

	pctx -> latest_io = get_us();

	return n;
}

typedef struct
{
	std::atomic_bool *stop_flag;
	std::string id;
	work_header_t *headers;
	source *s;
	bool first;
	bool header;
	uint8_t *data;
	size_t n;
	size_t req_len;
	uint64_t interval, next_frame_ts;
	stats_tracker *st;
	double timeout;
	myjpeg *j;
	int resize_w, resize_h;
} work_data_t;

#if HAVE_LIBCURL == 1
static int xfer_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	work_data_t *w = (work_data_t *)clientp;

	if (*w -> stop_flag) {
		log(w -> id, LL_DEBUG, "MJPEG stream aborted by stop flag");
		return 1;
	}

	if (get_us() - w -> headers -> latest_io >= w->timeout * 1000000) {
		log(w -> id, LL_INFO, "MJPEG camera fell asleep?");
		return 1;
	}

	return 0;
}
#endif

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *mypt)
{
	work_data_t *w = (work_data_t *)mypt;
	const size_t full_size = size * nmemb;

	if (*w -> stop_flag) {
		log(w -> id, LL_INFO, "stop-flag set");
		return 0;
	}

	w -> data = (uint8_t *)realloc(w -> data, w -> n + full_size + 1);
	memcpy(&w -> data[w -> n], ptr, full_size);
	w -> n += full_size;
	w -> data[w -> n] = 0x00;

	w -> headers -> latest_io = get_us();

	if (w -> n >= 32 * 1024 * 1024) { // sanity limit
		log(w -> id, LL_INFO, "frame too big");
		return 0;
	}

//	printf("h:%d  n:%zu req:%zu %s\n", w -> header, w -> n, w -> req_len, w -> data, w -> headers -> boundary);

process:
	if (w -> header) {
		bool proper_header = true;

		char *header_end = strstr((char *)w -> data, "\r\n\r\n");
		if (!header_end) {
			header_end = strstr((char *)w -> data, "\n\n");
			if (!header_end)
				return full_size;

			proper_header = false;
		}

		*header_end = 0x0;

		char *cl = strcasestr((char *)w -> data, "Content-Length:");
		if (!cl && w -> headers -> boundary == NULL) {
			//printf("====> %s\n",w->data);
			if (w -> headers -> boundary)
				goto with_boundary;
			log(w -> id, LL_INFO, "Content-Length missing in header");
			return 0;
		}

		if (cl)
			w -> req_len = atoi(&cl[15]);
		else
			w -> req_len = 0;

		// printf("needed len: %zu (%s)\n", w -> req_len, cl ? &cl[15] : "?");

		w -> header = false;

		size_t left = w -> n - (strlen((char *)w -> data) + (proper_header?4:2));
		if (left) {
			// printf("LEFT %zu\n", left);
			memmove(w -> data, header_end + (proper_header?4:2), left);
		}
		w -> n = left;
	}
	else if (w -> n >= w -> req_len && w -> req_len) {
		//printf("frame! (%p %zu/%zu)\n", w -> data, w -> n, w -> req_len);

		if (w -> first) {
			int width = -1, height = -1;
			unsigned char *temp = NULL;
			bool rc = w -> j -> read_JPEG_memory(w -> data, w -> req_len, &width, &height, &temp);
			free(temp);

			if (rc) {
				w -> first = false;
				log(w -> id, LL_INFO, "first frame received, mjpeg size: %dx%d", width, height);

				w -> s -> set_size(width, height);
			}
			else {
				log(w -> id, LL_INFO, "JPEG decode error");
			}
		}

		uint64_t now_ts = get_us();
		bool do_get = false;
		if (now_ts >= w -> next_frame_ts || w -> interval == 0) {
			do_get = true;
			w -> next_frame_ts += w -> interval;
		}

		if (w -> s -> work_required() && do_get && !w -> s -> is_paused()) {
			if (w -> resize_w != -1 || w -> resize_h != -1) {
				int dw, dh;
				unsigned char *temp = NULL;
				if (w -> j -> read_JPEG_memory(w -> data, w -> req_len, &dw, &dh, &temp))
					w -> s -> set_scaled_frame(temp, dw, dh);
				free(temp);
			}
			else {
				w->s->set_frame(E_JPEG, w -> data, w -> req_len);
				w->st->track_bw(w -> req_len);
			}
		}

		size_t left = w -> n - w -> req_len;
		if (left) {
			memmove(w -> data, &w -> data[w -> req_len], left);
		//printf("LEFT %zu\n", left);
		}
		w -> n = left;

		w -> req_len = 0;

		w -> header = true;
	}
	// for broken cameras that don't include a content-length in their headers
	else if (w -> headers -> boundary && w -> req_len == 0) {
with_boundary:
		ssize_t bl = strlen(w -> headers -> boundary);

		for(ssize_t i=0; i<w -> n - bl; i++) {
			if (memcmp(&w -> data[i], w -> headers -> boundary, bl) == 0) {
				char *em = strstr((char *)&w -> data[i], "\r\n\r\n");

				if (em) {
					w -> req_len = i;
					// printf("detected marker: %s\n", (char *)&w -> data[i]);
					goto process;
				}
			}
		}
	}

	return full_size;
}


source_http_mjpeg::source_http_mjpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & urlIn, const bool ic, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, loglevel, timeout, filters, failure, c, jpeg_quality), url(urlIn), ignore_cert(ic)
{
}

source_http_mjpeg::~source_http_mjpeg()
{
	stop();
	delete c;
}

void source_http_mjpeg::operator()()
{
	log(id, LL_INFO, "source http mjpeg thread started");

	set_thread_name("src_h_mjpeg");

	for(;!local_stop_flag;)
	{
#if HAVE_LIBCURL == 1
		log(id, LL_INFO, "(re-)connect to MJPEG source %s", url.c_str());

		CURL *ch = curl_easy_init();

		char error[CURL_ERROR_SIZE] = "?";
		if (curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, error))
			error_exit(false, "curl_easy_setopt(CURLOPT_ERRORBUFFER) failed: %s", error);

		curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, curl_log);

		curl_easy_setopt(ch, CURLOPT_URL, url.c_str());

		if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPALIVE, 1L))
			error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPALIVE) failed: %s", error);
	 
		if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPIDLE, 120L))
			error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPIDLE) failed: %s", error);
	 
		if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPINTVL, 60L))
			error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPINTVL) failed: %s", error);

		std::string useragent = NAME " " VERSION;

		if (curl_easy_setopt(ch, CURLOPT_USERAGENT, useragent.c_str()))
			error_exit(false, "curl_easy_setopt(CURLOPT_USERAGENT) failed: %s", error);

		if (ignore_cert) {
			if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0))
				error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed: %s", error);

			if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0))
				error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYHOST) failed: %s", error);
		}

		// abort if slower than 5 bytes/sec during "60 + 2 * timeout" seconds
		curl_easy_setopt(ch, CURLOPT_LOW_SPEED_TIME, 60L + timeout * 2);
		curl_easy_setopt(ch, CURLOPT_LOW_SPEED_LIMIT, 5L);

		work_header_t wh = { id, NULL, 0, NULL, get_us(), st };
		curl_easy_setopt(ch, CURLOPT_HEADERDATA, &wh);
		curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, write_headers);

		curl_easy_setopt(ch, CURLOPT_VERBOSE, loglevel >= LL_DEBUG_VERBOSE);

		curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, write_data);

		curl_easy_setopt(ch, CURLOPT_TCP_FASTOPEN, 1);

		work_data_t *w = new work_data_t;
		w -> id = id;
		w -> stop_flag = &local_stop_flag;
		w -> s = this;
		w -> first = w -> header = true;
		w -> data = NULL;
		w -> n = 0;
		w -> interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;
		w -> next_frame_ts = get_us();
		w -> headers = &wh;
		w -> st = st;
		w -> timeout = timeout;
		w -> j = &my_jpeg;
		w -> resize_w = resize_w;
		w -> resize_h = resize_h;
		curl_easy_setopt(ch, CURLOPT_WRITEDATA, w);

		curl_easy_setopt(ch, CURLOPT_XFERINFODATA, w);
		curl_easy_setopt(ch, CURLOPT_XFERINFOFUNCTION, xfer_callback);
		curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0L);

		CURLcode rc = CURLE_OK;
		if ((rc = curl_easy_perform(ch)) != CURLE_OK) {
			log(id, LL_INFO, "CURL error: %s", curl_easy_strerror(rc));
			do_exec_failure();
		}

		free(w -> data);
		delete w;

		free(wh.boundary);
		free(wh.data);

		long http_code = 0;
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &http_code);

		if (http_code == 200) {
			// all fine
			clear_error();
		}
		else if (http_code == 401)
			set_error("HTTP: Not authenticated", false);
		else if (http_code == 404)
			set_error("HTTP: URL not found", false);
		else if (http_code >= 500 && http_code <= 599)
			set_error("HTTP: Server error", false);
		else {
			set_error(myformat("HTTP error %ld", http_code), false);
		}

		curl_easy_cleanup(ch);

#endif
		st->track_cpu_usage();
		myusleep(101000);
	}

	register_thread_end("source http mjpeg");
}
