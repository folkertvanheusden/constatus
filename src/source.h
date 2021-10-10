// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once

#include <atomic>
#include <shared_mutex>
#include <stdint.h>
#include <thread>

#include "interface.h"
#include "error.h"
#include "gen.h"
#include "failure.h"
#include "picio.h"

class audio;
class controls;
class filter;
class ptz;
class resize;

class source : public interface
{
protected:
	int width, height;
	const double max_fps;
	resize *const r;
	const int resize_w, resize_h, loglevel;
	const double timeout;
	std::vector<filter *> *const filters;
	const std::string exec_failure;
	const int jpeg_quality;

	std::condition_variable cond;
        mutable std::mutex lock;

	video_frame *vf { nullptr };

	mutable std::mutex prev_frame_rgb_lock;
	uint8_t *prev_frame_rgb{ nullptr };

	std::atomic_int user_count;

	failure_t failure;
	uint8_t *failure_bitmap{ nullptr };
	int f_w{ -1 }, f_h{ -1 };

	void do_exec_failure();

	ptz *track { nullptr };

	std::shared_mutex controls_lock;
	controls *c { nullptr };

	std::thread *wd_th { nullptr };

	std::thread *exec_failure_th { nullptr };

	audio *a { nullptr };

	source(const std::string & id, const std::string & descr, const std::string & exec_failure, const int w, const int h, resize *const r, controls *const c, const int jpeg_quality);
	source(const std::string & id, const std::string & descr, const std::string & exec_failure, const int w, const int h, resize *const r, std::vector<filter *> *const filters, controls *const c, const int jpeg_quality);
	source(const std::string & id, const std::string & descr, const std::string & exec_failure, const double max_fps, const int w, const int h, const int loglevel, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality);

	void init();
	bool need_scale() const;

public:
	source(const std::string & id, const std::string & descr, const std::string & exec_failure, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters, const failure_t & failure, controls *const c, const int jpeg_quality);
	virtual ~source();

	virtual video_frame * get_frame(const bool handle_failure, const uint64_t after);
	virtual video_frame * get_failure_frame();
	void set_frame(const encoding_t pe, const uint8_t *const data, const size_t size, const bool do_duplicate = true);
	void set_scaled_frame(const uint8_t *const in, const int sourcew, const int sourceh);
	void set_size(const int w, const int h);

	virtual uint64_t get_current_ts() const;

	bool wait_for_meta();
	int get_width() const;
	int get_height() const;

	virtual bool has_pan_tilt() const { return track != nullptr; }
	virtual void pan_tilt(const double abs_pan, const double abs_tilt);
	virtual void get_pan_tilt(double *const pan, double *const tilt) const;

	virtual void operator()() { }

	void start_watchdog(const double interval);

	controls *get_controls() { return c; }

	void set_audio(audio *a) { this->a = a; }
	audio *get_audio() { return a; }

	virtual std::optional<double> get_fps() { return st->get_fps(); }
};
