// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
	const char *prefix;
}
my_struct_t;

void *init_plugin(const char *const argument)
{
	my_struct_t *a = (my_struct_t *)malloc(sizeof(my_struct_t));
	a -> prefix = NULL;

	return a;
}

void open_file(void *arg, const char *const fname_prefix, const double fps, const int quality)
{
	my_struct_t *a = (my_struct_t *)arg;

	a -> prefix = strdup(fname_prefix);
}

void write_frame(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame)
{
	my_struct_t *a = (my_struct_t *)arg;

	char *fname = NULL;
	asprintf(&fname, "%s%lu.pgm", a -> prefix, ts);

	FILE *fh = fopen(fname, "w");
	if (fh) {
		fprintf(fh, "P2\n%d %d\n255\n", w, h);

		int c = 0;
		for(int y=0; y<h; y++) {
			for(int x=0; x<w; x++) {
				if (c >= 70) {
					fprintf(fh, "\n");
					c = 0;
				}

				int o = y * w * 3 + x * 3;
				int g = (current_frame[o + 0] + current_frame[o + 1] + current_frame[o + 2]) / 3;

				c += fprintf(fh, "%d ", g);
			}
		}

		fclose(fh);
	}
	else {
		fprintf(stderr, "Failed creating %s: %s\n", fname, strerror(errno));
	}

	free(fname);
}

void close_file(void *arg)
{
	// not applicable for this PGM plugin
}

void uninit_plugin(void *arg)
{
	my_struct_t *a = (my_struct_t *)arg;

	free((void *)a -> prefix);
	free(a);
}
