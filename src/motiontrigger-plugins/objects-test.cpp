// (C) 2021 by folkert van heusden, license: Apache License v2.0
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../picio.h"
#include "../utils.h"

extern "C" {

int min_object_size = 5;

void * init_motion_trigger(const char *const parameter)
{
	if (parameter[0])
		min_object_size = atoi(parameter);

	return NULL;
}

int flood_fill(const int *const diff, int *const objects, const int cur_nr, const int w, const int h, const int x, const int y, const int threshold)
{
	int cnt = 0;

	std::stack<std::pair<int, int> > stack;
	stack.push({ x, y });

	while(!stack.empty())
	{
		auto p = stack.top();
		stack.pop();

		int cur_x = p.first;
		int cur_y = p.second;

		if (cur_y < 0 || cur_y >= h || cur_x < 0 || x >= w)
			continue;

		cnt++;

		const int o = cur_y * w + cur_x;

		if (diff[o] > threshold && objects[o] == 0) {
			objects[o] = cur_nr;

			stack.push({ cur_x + 1, cur_y });
			stack.push({ cur_x - 1, cur_y });
			stack.push({ cur_x, cur_y + 1 });
			stack.push({ cur_x, cur_y - 1 });
		}
	}

	return cnt;
}

static int file_nr = 0;

void dump_state(const int *const objects, const int nr, const int w, const int h, const uint8_t *const org)
{
	const int n_pixels = w * h;
	uint8_t *out = new uint8_t[n_pixels * 3]();

	for(int i=0; i<n_pixels; i++) {
		if (objects[i] == nr) {
			out[i * 3 + 0] = org[i * 3 + 0];
			out[i * 3 + 1] = org[i * 3 + 1];
			out[i * 3 + 2] = org[i * 3 + 2];
		}
	}

	FILE *fh = fopen(myformat("m-state-%05d.png", file_nr++).c_str(), "wb");
	if (fh) {
		write_PNG_file(fh, w, h, out);
		fclose(fh);
	}

	delete [] out;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	const int n_pixels = w * h;

	// find difference and average of that diffence
	long int avg = 0;

	int *diff = new int[n_pixels];
	for(int i=0; i<n_pixels; i++) {
		int r_diff = abs(current_frame[i * 3 + 0] - prev_frame[i * 3 + 0]);
		int g_diff = abs(current_frame[i * 3 + 1] - prev_frame[i * 3 + 1]);
		int b_diff = abs(current_frame[i * 3 + 2] - prev_frame[i * 3 + 2]);

		avg += diff[i] = (r_diff + b_diff + g_diff) / 3;
	}

	avg /= n_pixels;

	const int min_obj_size_pixels = n_pixels * min_object_size / 100;

	// flood-fill over diff, where differences > avg
	// are considerd to be one object
	int *objects = new int[n_pixels]();
	int object_nr = 1;

	bool rc = false;

	for(int y=0; y<h && !rc; y++) {
		for(int x=0; x<w; x++) {
			const int i = y * w + x;

			if (diff[i] > avg && objects[i] == 0) {
				int pixel_count = flood_fill(diff, objects, object_nr, w, h, x, y, avg);

				if (pixel_count >= min_obj_size_pixels) {
					rc = true;
					break;
				}

				object_nr++;
			}
		}
	}

	if (rc)
		dump_state(objects, object_nr, w, h, current_frame);

	delete [] objects;
	delete [] diff;

	return rc;
}

void uninit_motion_trigger(void *arg)
{
}
}
