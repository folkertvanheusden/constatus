// (C) 2019 by folkert van heusden, this file is released in the public domain
#include <atomic>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../log.h"

extern "C" {

typedef struct {
	pthread_t th;
	int pin;
	std::atomic_bool motion_detected, terminate;
} my_data_t;

void * gpio_check(void *par)
{
	my_data_t *md = new my_data_t;

	char pin[16];
	int len = snprintf(pin, sizeof pin, "%d", md->pin);

	int fd_export = open("/sys/class/gpio/export", O_WRONLY | O_SYNC);
	if (fd_export == -1)
		log(LL_ERR, "Failed to export GPIO pin %d", md->pin);
	write(fd_export, pin, len);
	close(fd_export);

	char name[128];
	snprintf(name, sizeof name, "/sys/class/gpio/gpio%d/direction", md->pin);

	int fd_direction = open(name, O_WRONLY | O_SYNC);
	write(fd_direction, "in", 2);
	close(fd_direction);

	snprintf(name, sizeof name, "/sys/class/gpio/gpio%d/value", md->pin);
	int fd_value = open(name, O_RDONLY);

	char dummy[64];
	read(fd_value, dummy, 1);

	struct pollfd fds[1] = { { fd_value, POLLPRI, 0 } };

	for(;!md->terminate;) {
		if (poll(fds, 1, 1000) != 1)
			continue;

		md->motion_detected = true;

		read(fd_value, dummy, sizeof dummy);

		fds[0].revents = 0;
	}

	close(fd_value);

	return nullptr;
}

void * init_motion_trigger(const char *const parameter)
{
	my_data_t *md = new my_data_t;
	md->motion_detected = md->terminate = false;
	md->pin = atoi(parameter);

	pthread_create(&md->th, nullptr, gpio_check, md);

	return md;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	my_data_t *md = (my_data_t *)arg;

	return md -> motion_detected.exchange(false);
}

void uninit_motion_trigger(void *arg)
{
	my_data_t *md = (my_data_t *)arg;

	md->terminate = true;

	pthread_join(md->th, nullptr);

	delete md;
}

}
