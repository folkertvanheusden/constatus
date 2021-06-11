// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_http_jpeg.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

source_http_jpeg::source_http_jpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & urlIn, const bool ignoreCertIn, const std::string & authIn, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int ll, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality) : source(id, descr, exec_failure, max_fps, r, resize_w, resize_h, ll, timeout, filters, failure, c, jpeg_quality), url(urlIn), auth(authIn), ignore_cert(ignoreCertIn)
{
}

source_http_jpeg::~source_http_jpeg()
{
	stop();
	delete c;
}

void source_http_jpeg::operator()()
{
	log(id, LL_INFO, "source http jpeg thread started");

	set_thread_name("src_h_jpeg");

	bool first = true, resize = resize_h != -1 || resize_w != -1;

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;
	long int backoff = 101000;

	for(;!local_stop_flag;)
	{
		uint64_t start_ts = get_us();

		if (work_required()) {
			uint8_t *work = NULL;
			size_t work_len = 0;

			if (!http_get(url, ignore_cert, auth.empty() ? NULL : auth.c_str(), loglevel == LL_DEBUG_VERBOSE, &work, &work_len, &local_stop_flag))
			{
				set_error("did not get a frame", false);
				myusleep(backoff);

				do_exec_failure();

				if (backoff <= 2000000)
					backoff += 51000;
				continue;
			}

			st->track_bw(work_len);

			backoff = 101000;

			if (!is_paused()) {
				unsigned char *temp = NULL;
				int dw = -1, dh = -1;
				if (first || resize) {
					if (!my_jpeg.read_JPEG_memory(work, work_len, &dw, &dh, &temp)) {
						set_error("JPEG decode error", false);
						continue;
					}

					std::unique_lock<std::mutex> lck(lock);
					if (resize) {
						width = resize_w;
						height = resize_h;
					}
					else {
						width = dw;
						height = dh;
					}
					lck.unlock();

					first = false;
				}

				if (resize)
					set_scaled_frame(temp, dw, dh);
				else
					set_frame(E_JPEG, work, work_len);

				clear_error();

				free(temp);
			}

			free(work);
		}

		st->track_cpu_usage();

		uint64_t end_ts = get_us();
		int64_t left = interval <= 0 ? 100000 : (interval - std::max(uint64_t(1), end_ts - start_ts));

		if (left > 0)
			myusleep(left);
	}

	register_thread_end("source http jpeg");
}
