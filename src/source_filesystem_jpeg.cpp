// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "source.h"
#include "source_filesystem_jpeg.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "controls.h"

source_filesystem_jpeg::source_filesystem_jpeg(const std::string & id, const std::string & descr, const std::string & exec_failure, const std::string & path, const double fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : source(id, descr, exec_failure, fps, r, resize_w, resize_h, loglevel, -1., filters, failure, c, jpeg_quality, text_feeds, keep_aspectratio), path(path)
{
}

source_filesystem_jpeg::~source_filesystem_jpeg()
{
	stop();
	delete c;
}

void source_filesystem_jpeg::operator()()
{
	log(id, LL_INFO, "source filesystem jpeg thread started");

	set_thread_name("src_f_jpeg");

	bool first = true, resize = resize_h != -1 || resize_w != -1;

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	int check_fd = inotify_init();

	for(;!local_stop_flag;)
	{
		uint64_t start_ts = get_us();

		if (work_required()) {
			if (inotify_add_watch(check_fd, path.c_str(), IN_CLOSE_WRITE | IN_ATTRIB | IN_ONESHOT) == -1) {
				if (errno != ENOENT) {
					log(id, LL_ERR, "inotify_add_watch failed(1): %s", strerror(errno));
					break;
				}

				std::string dir = ".";
				size_t last_slash = path.rfind('/'); // windows users may want to replace this by a backslash
				if (last_slash != std::string::npos) // no path
					dir = path.substr(0, last_slash);

				if (inotify_add_watch(check_fd, dir.c_str(), IN_CLOSE_WRITE | IN_ATTRIB | IN_ONESHOT) == -1) {
					log(id, LL_ERR, "inotify_add_watch failed(2): %s", strerror(errno));
					break;
				}
			}

			struct inotify_event event { 0 };
			if (read(check_fd, &event, sizeof event) != sizeof event)
				log(id, LL_WARNING, "Short read on inotify?");

			FILE *fh = fopen(path.c_str(), "rb");
			if (!fh) {
				if (errno == ENOENT)
					continue;

				log(id, LL_ERR, "Cannot access %s: %s", path.c_str(), strerror(errno));
				sleep(1);
				break;
			}

			struct stat st;
			if (fstat(fileno(fh), &st) == -1) {
				log(id, LL_ERR, "Cannot get meta data of %s: %s", path.c_str(), strerror(errno));
				fclose(fh);
				sleep(1);
				break;
			}

			size_t work_len = st.st_size;
		        uint8_t *work = (uint8_t *)malloc(work_len);

			if (fread(work, work_len, 1, fh) != work_len)
				log(id, LL_WARNING, "Short read on %s: %s", path.c_str(), strerror(errno));

			fclose(fh);

			if (!is_paused()) {
				unsigned char *temp = NULL;
				int dw = -1, dh = -1;
				if (first || resize) {
					if (!my_jpeg.read_JPEG_memory(work, work_len, &dw, &dh, &temp)) {
						// this may happen if the file is still being copied
						// should look into 'inotify' to prevent this from
						// happening
						log(id, LL_WARNING, "JPEG decode error");
						usleep(101000);
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
					set_scaled_frame(temp, dw, dh, keep_aspectratio);
				else
					set_frame(E_JPEG, work, work_len);

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

	close(check_fd);

	register_thread_end("source filesystem jpeg");
}
