// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "target.h"

typedef void *(* tp_init_plugin_t)(const char *const argument);
typedef void (* open_file_t)(void *arg, const char *const fname_prefix, const double fps, const int quality);
typedef void (* write_frame_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame);
typedef void (* close_file_t)(void *arg);
typedef void (* tp_uninit_plugin_t)(void *arg);

typedef struct {
	std::string par;

	tp_init_plugin_t init_plugin;
	open_file_t   open_file;
	write_frame_t write_frame;
	close_file_t  close_file;
	tp_uninit_plugin_t uninit_plugin;

	void *arg;
} stream_plugin_t;

class target_plugin : public target
{
private:
	const int quality;
	stream_plugin_t *sp { nullptr };
	bool is_start { true };
	bool is_open { false };

	void open_plugin();

public:
	target_plugin(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, stream_plugin_t *const sp, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_plugin();

	void operator()() override;
};
