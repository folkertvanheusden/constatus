// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include <string>

#include "filter.h"

void *find_symbol(void *const dl, const std::string & filename, const std::string & function, const bool is_fatal=true);
void *load_library(const std::string & filename);

typedef void * (*init_filter_t)(const char *const par);
typedef void (*apply_filter_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result);
typedef void (*uninit_filter_t)(void *arg);

class filter_plugin : public filter
{
private:
	init_filter_t init_filter { nullptr };
	apply_filter_t apply_filter { nullptr };
	uninit_filter_t uninit_filter { nullptr };

protected:
	void *library { nullptr };
	void *arg { nullptr };

	filter_plugin() { }

public:
	filter_plugin(const std::string & filter_filename, const std::string & parameter);
	~filter_plugin();

	virtual bool uses_in_out() const override { return true; }
	virtual void apply_io(instance *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out) override;
};
