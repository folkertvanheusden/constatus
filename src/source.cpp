// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#include "config.h"
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <cstring>
#include <sys/time.h>

#include "source.h"
#include "error.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "exec.h"
#include "draw.h"
#include "draw_text.h"
#include "filter_add_text.h"
#include "resize.h"
#include "controls.h"

source::source(const std::string & id, const std::string & descr, const std::string & exec_failure, const int w, const int h, resize *const r, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : interface(id, descr), width(w), height(h), max_fps(-1), r(r), resize_w(-1), resize_h(-1), loglevel(get_default_loglevel()), timeout(3.0), filters(nullptr), exec_failure(exec_failure), jpeg_quality(jpeg_quality), text_feeds(text_feeds), keep_aspectratio(keep_aspectratio)
{
	this->c = c;
	init();
}

source::source(const std::string & id, const std::string & descr, const std::string & exec_failure, const int w, const int h, resize *const r, std::vector<filter *> *const filters, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : interface(id, descr), width(w), height(h), max_fps(-1), r(r), resize_w(-1), resize_h(-1), loglevel(get_default_loglevel()), timeout(3.0), filters(filters), exec_failure(exec_failure), jpeg_quality(jpeg_quality), text_feeds(text_feeds), keep_aspectratio(keep_aspectratio)
{
	this->c = c;
	init();
}

source::source(const std::string & id, const std::string & descr, const std::string & exec_failure, const double max_fps, const int w, const int h, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : interface(id, descr), width(w), height(h), max_fps(max_fps), r(nullptr), resize_w(-1), resize_h(-1), loglevel(loglevel), timeout(3.0), filters(filters), exec_failure(exec_failure), failure(failure), jpeg_quality(jpeg_quality), text_feeds(text_feeds), keep_aspectratio(keep_aspectratio)
{
	this->c = c;
	init();
}

source::source(const std::string & id, const std::string & descr, const std::string & exec_failure, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality, const std::map<std::string, feed *> & text_feeds, const bool keep_aspectratio) : interface(id, descr), max_fps(max_fps), r(r), resize_w(resize_w), resize_h(resize_h), loglevel(loglevel), timeout(timeout), filters(filters), exec_failure(exec_failure), failure(failure), jpeg_quality(jpeg_quality), text_feeds(text_feeds), keep_aspectratio(keep_aspectratio)
{
	width = height = -1;
	this->c = c;
	init();
}

source::~source()
{
	delete vf;

	free_filters(filters);

	free(failure_bitmap);

	free(prev_frame_rgb);

	join_thread(&wd_th, id, "watchdog");

	join_thread(&exec_failure_th, id, "exec-failure");

	delete font;
}

void source::init()
{
	vf = nullptr;
	user_count = 0;
	ct = CT_SOURCE;

	font = new draw_text("", 32);

	if (!failure.bg_bitmap.empty()) {
		FILE *fh = fopen(failure.bg_bitmap.c_str(), "rb");
		if (!fh)
			error_exit(true, "Cannot open failure JPEG bitmap %s", failure.bg_bitmap.c_str());

		if (fseek(fh, 0L, SEEK_END) == -1)
			error_exit(true, "fseek in %s failed", failure.bg_bitmap.c_str());

		int size = ftell(fh);
		if (size == -1)
			error_exit(true, "ftell failed for %s", failure.bg_bitmap.c_str());

		if (fseek(fh, 0L, SEEK_SET) == -1)
			error_exit(true, "fseek in %s failed", failure.bg_bitmap.c_str());

		uint8_t *temp = new uint8_t[size];
		if (fread(temp, 1, size, fh) != size)
			error_exit(true, "Short read on %s", failure.bg_bitmap.c_str());

		if (!my_jpeg.read_JPEG_memory(temp, size, &f_w, &f_h, &failure_bitmap))
			error_exit(false, "Cannot read failure JPEG bitmap %s", failure.bg_bitmap.c_str());

		delete [] temp;

		fclose(fh);
	}
}

void source::do_exec_failure()
{
	if (!exec_failure.empty() && !local_stop_flag) {
		if (check_thread(&exec_failure_th))
			exec_failure_th = exec(exec_failure, id);
	}
}

void source::set_frame(const encoding_t pe, const uint8_t *const data, const size_t size, const bool do_duplicate)
{
	uint64_t use_ts = get_us();

	st->track_fps();

	uint8_t *copy = (uint8_t *)(do_duplicate ? duplicate(data, size) : data);

	std::unique_lock<std::mutex> lck(lock);

	delete vf;
	vf = new video_frame(get_meta(), jpeg_quality, use_ts, width, height, copy, size, pe);

	cond.notify_all();

	lck.unlock();
}

void source::set_scaled_frame(const uint8_t *const in, const int sourcew, const int sourceh, const bool keep_aspectratio)
{
	uint64_t use_ts = get_us();

        int target_w = resize_w != -1 ? resize_w : sourcew;
        int target_h = resize_h != -1 ? resize_h : sourceh;

        uint8_t *out = nullptr;

	if (keep_aspectratio) {
		out = reinterpret_cast<uint8_t *>(malloc(target_w * target_h * 3));

		int perc = std::min(sourcew / resize_w, sourceh / resize_h);

		pos_t pos { center_center, 0, 0 };

		picture_in_picture(r, out, target_w, target_h, in, sourcew, sourceh, perc, pos);
	}
	else {
		r -> do_resize(sourcew, sourceh, in, target_w, target_h, &out);
	}

	std::unique_lock<std::mutex> lck(lock);

	delete vf;
	vf = new video_frame(get_meta(), jpeg_quality, use_ts, target_w, target_h, out, IMS(target_w, target_h, 3), E_RGB);

	lck.unlock();

	cond.notify_all();
}

video_frame * source::get_frame(const bool handle_failure, const uint64_t after)
{
	bool err = false;

	std::unique_lock<std::mutex> lck(lock);

	uint64_t stop_at = get_us() + timeout * 1000000;
	int64_t to = 0;

	while(!vf || vf->get_ts() <= after) {
		to = stop_at - get_us();

		if (to <= 0) {
			err = true;
			break;
		}

		if (cond.wait_for(lck, std::chrono::microseconds(to)) == std::cv_status::timeout) {
			err = true;
			break;
		}
	}

	if (err) {
fail:
		uint64_t use_ts = vf ? vf->get_ts() : 0;

		lck.unlock();

		log(id, LL_DEBUG, "v-frame fail err %d pvf %p | to % " PRId64 ", ts %" PRIu64 " reqts %" PRIu64 " > tdiff %" PRId64 " | timeout %f", err, vf, to, use_ts, after, use_ts - after, timeout);

		do_exec_failure();

		if (handle_failure && failure.fm != F_NOTHING)
			return get_failure_frame();

		return nullptr;
	}

	std::shared_lock clck(controls_lock);
	bool need_controls_apply = c && c->requires_apply();
	clck.unlock();

	bool need_filters = filters && !filters->empty();

	if (need_controls_apply || need_filters) {
		uint8_t *work = vf->get_data(E_RGB);
		uint64_t vf_ts = vf->get_ts();

		size_t n_bytes = IMS(vf->get_w(), vf->get_h(), 3);

		uint8_t *copy = (uint8_t *)duplicate(work, n_bytes);

		lck.unlock();

		if (need_filters) {
			std::lock_guard<std::mutex> pflck(prev_frame_rgb_lock);

			apply_filters(nullptr, this, filters, prev_frame_rgb, copy, vf_ts, vf->get_w(), vf->get_h());

			if (!prev_frame_rgb)
				prev_frame_rgb = (uint8_t *)malloc(n_bytes);

			memcpy(prev_frame_rgb, copy, n_bytes);
		}

		if (need_controls_apply) {
			std::shared_lock clck(controls_lock);

			c->apply(copy, vf->get_w(), vf->get_h());
		}

		return new video_frame(get_meta(), jpeg_quality, vf->get_ts(), vf->get_w(), vf->get_h(), copy, n_bytes, E_RGB);
	}

	return vf->duplicate({ });
}

video_frame * source::get_frame_to(const bool handle_failure, const uint64_t after, const uint64_t us)
{
	std::unique_lock<std::mutex> lck(lock);

	bool     no_frame = false;

	uint64_t stop_at = get_us() + us;
	int64_t  to = 0;

	while(!vf || vf->get_ts() <= after) {
		to = stop_at - get_us();

		if (to <= 0 || cond.wait_for(lck, std::chrono::microseconds(to)) == std::cv_status::timeout) {
			no_frame = true;
			break;
		}
	}

	if (no_frame)
		return nullptr;

	std::shared_lock clck(controls_lock);
	bool need_controls_apply = c && c->requires_apply();
	clck.unlock();

	bool need_filters = filters && !filters->empty();

	if (need_controls_apply || need_filters) {
		uint8_t *work = vf->get_data(E_RGB);
		uint64_t vf_ts = vf->get_ts();

		size_t n_bytes = IMS(vf->get_w(), vf->get_h(), 3);

		uint8_t *copy = (uint8_t *)duplicate(work, n_bytes);

		lck.unlock();

		if (need_filters) {
			std::lock_guard<std::mutex> pflck(prev_frame_rgb_lock);

			apply_filters(nullptr, this, filters, prev_frame_rgb, copy, vf_ts, vf->get_w(), vf->get_h());

			if (!prev_frame_rgb)
				prev_frame_rgb = (uint8_t *)malloc(n_bytes);

			memcpy(prev_frame_rgb, copy, n_bytes);
		}

		if (need_controls_apply) {
			std::shared_lock clck(controls_lock);

			c->apply(copy, vf->get_w(), vf->get_h());
		}

		return new video_frame(get_meta(), jpeg_quality, vf->get_ts(), vf->get_w(), vf->get_h(), copy, n_bytes, E_RGB);
	}

	return vf->duplicate({ });
}

bool source::wait_for_meta()
{
	bool err = false;

	std::unique_lock<std::mutex> lck(lock);

	uint64_t stop_at = get_us() + timeout * 1000000;

	while(!vf || vf->get_ts() == 0) {
		int64_t to = stop_at - get_us();

		if (to <= 0 || cond.wait_for(lck, std::chrono::microseconds(to)) == std::cv_status::timeout) {
			err = true;
			break;
		}
	}

	// lck/lock is now locked but will be auto-unlocked

	return !err;
}

int btr(const int v, const int r)
{
	return std::min(v * r / 255, r);
}

void draw_noise(uint8_t *const fail, const int width, const int height, const int x1, const int y1, const int x2, const int y2)
{
	for(int y=y1; y<y2; y++) {
		int yo = y * width * 3;

		for(int x=x1; x<x2; x++) {
			int offset = yo + x * 3;

			fail[offset + 0] = fail[offset + 1] = fail[offset + 2] = rand();
		}
	}
}

video_frame * source::get_failure_frame()
{
	std::unique_lock<std::mutex> lck(lock);
	int lw = width, lh = height;
	lck.unlock();

	if (lw == -1)
		lw = 640;
	if (lh == -1)
		lh = 480;

	int fail_len = lw * lh * 3;
	uint8_t *fail = (uint8_t *)malloc(fail_len);

	if (failure_bitmap) {
		if (f_w != lw || f_h != lh) {
			uint8_t *out = nullptr;
			r -> do_resize(f_w, f_h, failure_bitmap, lw, lh, &out);

			free(fail);
			fail = out;
		}
		else {
			memcpy(fail, failure_bitmap, fail_len);
		}
	}
	else if (failure.fm == F_MESSAGE) {
		memset(fail, 0x60, fail_len);

		constexpr int stepx = 32;
		constexpr int stepy = 32, hstepy = stepy / 2;
		int nr = 0;
		for(int y=0; y<256; y += stepy) {
			for(int x=0; x<256; x += stepx) {
				rgb_t col = { 160, 160, 160 };

				if (nr == 0)
					col.r = 255;
				if (nr == 1)
					col.g = 255;
				if (nr == 2)
					col.b = 255;

				nr = (nr + 1) % 3;

				if (x == 0 || y == 0)
					draw_noise(fail, lw, lh, btr(x + 2, lw), btr(y + 2, lh), btr(x - 2 + stepx, lw), btr(y - 2 + stepy, lh));
				else
					draw_box(fail, lw, btr(x + 2, lw), btr(y + 2, lh), btr(x - 2 + stepx, lw), btr(y - 2 + stepy, lh), col);
			}
		}

		for(int y=0; y<4; y++) {
			int work_y = stepy + y * hstepy;

			for(int x=0; x<8; x++) {
				double rd = 0., gd = 0., bd = 0.;
				hls_to_rgb(x / 8.0, 0.5, (y + 1) / 4.0, &rd, &gd, &bd);

				rgb_t col { int(rd * 255), int(gd * 255), int(bd * 255) };

				draw_box(fail, lw, btr(x * stepx, lw), btr(work_y, lh), btr(x * stepx + stepx, lw), btr(work_y + hstepy, lh), col);
			}
		}

		for(int y=0; y<256; y += stepy) {
			int c = ((y / stepy) & 1) ? 255 : 0;
			rgb_t col { c, c, c };

			draw_box(fail, lw, btr(0, lw), btr(y, lh), btr(2, lw), btr(y + stepy, lh), col);
			draw_box(fail, lw, btr(254, lw), btr(y, lh), lw, btr(y + stepy, lh), col);
		}

		for(int x=0; x<256; x += stepx) {
			int c = ((x / stepx) & 1) ? 255 : 0;
			rgb_t col { c, c, c };

			draw_box(fail, lw, btr(x, lw), btr(0, lh), btr(x + stepx, lw), btr(2, lh), col);
			draw_box(fail, lw, btr(x, lw), btr(254, lh), btr(x + stepx, lw), lh, col);
		}

		for(int x=0; x<256; x++) {
			rgb_t col1 { x, x, x };
			draw_box(fail, lw, btr(x, lw), btr(150, lh), btr(x + 1, lw), btr(175, lh), col1);

			rgb_t col2 { x ^ 255, x ^ 255, x ^ 255 };
			draw_box(fail, lw, btr(x, lw), btr(175, lh), btr(x + 1, lw), btr(200, lh), col2);
		}

		std::string msg = unescape(failure.message, 0, nullptr, this, { });

		std::vector<std::string> *parts = split(msg, "\n");

		int work_y = lh / 2 - (parts->size() / 2 * 32) - 32;

		constexpr rgb_t text_bg { 0, 0, 0 };

		draw_box(fail, lw, 0, work_y, lw, work_y + 32, text_bg);

		draw_text_on_bitmap(font, "Camera: " + get_id(), lw, lh, fail, 32, { 255, 255, 255 }, text_bg, -1, work_y);

		work_y += 32;

		for(auto s : *parts) {
			draw_box(fail, lw, 0, work_y, lw, work_y + 32, text_bg);

			draw_text_on_bitmap(font, s, lw, lh, fail, 32, { 255, 255, 255 }, text_bg, -1, work_y);

			work_y += 32;
		}

		delete parts;

		filter_add_text fat(NAME " v" VERSION, { lower_center, -1, -1 }, text_feeds);
		fat.apply(nullptr, this, 0, lw, lh, nullptr, fail);

		filter_add_text fat2("%c", { upper_center, -1, -1 }, text_feeds);
		fat2.apply(nullptr, this, get_us(), lw, lh, nullptr, fail);
	}
	else if (failure.fm == F_SIMPLE) {
		filter_add_text fat(failure.message, failure.position, text_feeds);

		fat.apply(nullptr, this, 0, lw, lh, nullptr, fail);
	}

	return new video_frame(get_meta(), jpeg_quality, 0, lw, lh, fail, fail_len, E_RGB);
}

uint64_t source::get_current_ts() const
{
	const std::lock_guard<std::mutex> lck(lock);

	return vf ? vf->get_ts() : 0;
}

void source::pan_tilt(const double abs_pan, const double abs_tilt)
{
	// default: do nothing
}

void source::get_pan_tilt(double *const pan, double *const tilt) const
{
	const std::lock_guard<std::mutex> lck(lock);

	*pan = *tilt = 0.;
}

void source::start_watchdog(const double restart_interval)
{
	wd_th = new std::thread([&, restart_interval]() {
		set_thread_name("wd:" + id);

		log(get_id(), LL_INFO, "Watchdog for %s started with interval %fs", get_id().c_str(), restart_interval);

		start(); // device must be running

		// this way it'll take 'restart_interval' seconds before the watchdog will start
		// monitoring
		uint64_t mute = get_us();

		while(!local_stop_flag) {
			myusleep(101000);

			uint64_t now = get_us();

			if (now - mute >= restart_interval * 1000000 && work_required()) {
				std::unique_lock<std::mutex> th_lck(th_lock);

				if (th) {
					std::unique_lock<std::mutex> vf_lck(lock);
					uint64_t vf_ts = vf ? vf->get_ts() : 0;
					vf_lck.unlock();

					if (now - vf_ts > restart_interval * 1000000) {
						th_lck.unlock();

						log(id, LL_WARNING, "Restarting %s", get_id().c_str());
						restart();

						mute = get_us();

						th_lck.lock();
					}
				}

				th_lck.unlock();
			}
		}

		stop();

		log(id, LL_INFO, "Watchdog for %s terminating", get_id().c_str());
	});
}

void source::set_size(const int w, const int h)
{
	const std::lock_guard<std::mutex> lck(lock);

	width = w;
	height = h;
}

bool source::need_scale() const
{
	return resize_h != -1 || resize_w != -1;
}

int source::get_width() const
{
	const std::lock_guard<std::mutex> lck(lock);

	return width;
}

int source::get_height() const
{
	const std::lock_guard<std::mutex> lck(lock);

	return height;
}
