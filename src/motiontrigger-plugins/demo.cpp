// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

extern "C" {

void * init_motion_trigger(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever

	// what you return here, will be given as a parameter
	// to detect_motion

	return NULL;
}

// ts is current timestamp in microseconds
// w,h are the dimensions of the pixel-data in prev_frame and current_frame
// (both are RGB data, so 3 bytes per pixel)
// selection_bitmap is a bitmap informing which pixels to look at and which
// too ignore
// meta can contain a pointer to a text-string with meta data. e.g. when a
// moving object is recognized, you can add a pointer to a free()-able text
// in it. you can then print it in-line in the video-feed via a text-filter:
//   {
//   	type = "text";
//   	text = "$motion-meta$";
//   	position = "lower-right"
//   }
// '$motion-meta$' will be replaced by the text in *meta.

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	return false;
}

void uninit_motion_trigger(void *arg)
{
	// free memory etc
}

}
