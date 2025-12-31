// (C) 2021-2023 by folkert van heusden, this file is released under the MIT license
#include <algorithm>
#include <atomic>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../error.h"
#include "../utils.h"

extern "C" {

typedef struct {
	std::atomic_uint64_t triggered;
	int fd, port;
	pthread_t th;
} arg_t;

void * listen_thread(void *arg)
{
	arg_t *a = (arg_t *)arg;

	char buffer[8];
	for(;;) {
		int rc = recv(a->fd, buffer, sizeof buffer, 0);

		if (rc >= 0) {
			a->triggered = get_us();
			printf("network triggered\n");
		}
		else {
			break;
		}
	}

	return nullptr;
}

void * init_motion_trigger(const char *const parameter)
{
	arg_t *a = new arg_t;

	a->port = atoi(parameter);

	a->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (a->fd == -1)
		error_exit(true, "Failed to create datagram socket");

	struct sockaddr_in address{ 0 };
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(a->port);
	if (bind(a->fd, (struct sockaddr *)&address, sizeof(address)) == -1)
		error_exit(true, "Failed to bind socket to port %d", a->port);

	pthread_create(&a->th, nullptr, listen_thread, a);

	return a;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap, char **const meta)
{
	arg_t *a = (arg_t *)arg;

	bool rc = a -> triggered.exchange(0) != 0;

	return rc;
}

void uninit_motion_trigger(void *arg)
{
	arg_t *a = (arg_t *)arg;

	close(a->fd);

	pthread_join(a->th, nullptr);

	delete a;
}

}
