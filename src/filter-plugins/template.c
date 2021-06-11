// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <stdint.h>

void * init_filter(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever

	// what you return here, will be given as a parameter
	// to apply_filter

	return NULL;
}

void apply_filter(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
}

void uninit_filter(void *arg)
{
	// free memory etc
}
