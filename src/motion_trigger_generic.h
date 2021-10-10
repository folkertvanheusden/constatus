// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <atomic>
#include <shared_mutex>
#include <string>

#include "motion_trigger.h"

class filter;
class parameter;
class selection_mask;
class source;
class target;

typedef void * (*init_motion_trigger_t)(const char *const par);
typedef bool (*detect_motion_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta);
typedef void (*uninit_motion_trigger_t)(void *arg);

typedef struct
{
	void *library;

	init_motion_trigger_t init_motion_trigger;
	detect_motion_t detect_motion;
	uninit_motion_trigger_t uninit_motion_trigger;

	std::string par;

	void *arg;
} ext_trigger_t;

class motion_trigger_generic : public motion_trigger
{
private:
	source *const s;

	std::atomic_bool motion_triggered;

	const int camera_warm_up;

	const std::vector<filter *> *const filters;

	ext_trigger_t *const et;
	selection_mask *const pixel_select_bitmap;
	std::string exec_start, exec_end;

	instance *const inst;

public:
	motion_trigger_generic(const std::string & id, const std::string & descr, source *const s, const int camera_warm_up, const std::vector<filter *> *const filters, std::vector<target *> *const targets, selection_mask *const pixel_select_bitmap, ext_trigger_t *const et, const std::string & e_start, const std::string & e_end, instance *const inst, const std::map<std::string, parameter *> & detection_parameters, schedule *const sched);
	virtual ~motion_trigger_generic();

	bool check_motion() override { return motion_triggered; }

	void operator()() override;
};
