// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../source.h"
#include "../log.h"

typedef struct
{
	source *s;
	int w, h;
	std::atomic_bool stop_flag;
	pthread_t th;
} my_data_t;

void * thread(void *arg)
{
	log(LL_INFO, "source plugin thread started");

	my_data_t *md = (my_data_t *)arg;

	size_t bytes = md -> w * md -> h * 3;
	uint8_t *pic = (uint8_t *)malloc(bytes);
	memset(pic, 0x00, bytes);

	int x = md -> w / 2, y = md -> h / 2;

	while(!md -> stop_flag)
	{
		int o = y * md -> w * 3 + x * 3;
		pic[o + 0] ^= 255;
		pic[o + 1] ^= 255;
		pic[o + 2] ^= 255;

		x += (rand() % 3) - 1;
		y += (rand() % 3) - 1;

		if (x < 0)
			x = md -> w - 1;
		else if (x >= md -> w)
			x = 0;

		if (y < 0)
			y = md -> h - 1;
		else if (y >= md -> h)
			y = 0;

		// send RGB frame to constatus
		md -> s -> set_frame(E_RGB, pic, bytes);

		usleep(10000);
	}

	free(pic);

	log(LL_INFO, "source plugin thread ending");

	return NULL;
}

extern "C" void *init_plugin(source *const s, const char *const argument)
{
	my_data_t *md = new my_data_t;
	md -> s = s;
	md -> w = 352;
	md -> h = 288;
	md -> stop_flag = false;

	s -> set_size(md -> w, md -> h);

	pthread_create(&md -> th, NULL, thread, md);

	return md;
}

extern "C" void uninit_plugin(void *arg)
{
	my_data_t *md = (my_data_t *)arg;

	md -> stop_flag = true;

	pthread_join(md -> th, NULL);

	delete md;
}
