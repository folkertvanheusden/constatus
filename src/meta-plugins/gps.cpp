// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <atomic>
#include <gps.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../meta.h"
#include "../log.h"
#include "../utils.h"

typedef struct
{
	meta *m;
	char *par;
	std::atomic_bool stop_flag;
	pthread_t th;
} my_data_t;

void * thread(void *arg)
{
	log(LL_INFO, "GPS meta plugin thread started");

	my_data_t *md = (my_data_t *)arg;

	const char *address = "localhost", *port = "2947";

	if (md -> par) {
		address = md -> par;

		char *sp = strchr(md -> par, ' ');
		if (sp) {
			*sp = 0x00;

			port = sp + 1;
		}
	}

	log(LL_DEBUG, "GPS meta plugin connecting to %s:%s", address, port);

	int rc = 0;
	struct gps_data_t gps_data;
	if ((rc = gps_open(address, port, &gps_data)) == -1) {
		log(LL_ERR, "libgps open failed, code: %d, reason: %s", rc, gps_errstr(rc));
		return NULL;
	}

	gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);

	while(!md -> stop_flag) {
		if (gps_waiting(&gps_data, 1000000)) {
			if ((rc = gps_read(&gps_data)) == -1)
				log(LL_ERR, "error occured reading gps data. code: %d, reason: %s", rc, gps_errstr(rc));
			else if (gps_data.status == STATUS_FIX && (gps_data.fix.mode == MODE_2D || gps_data.fix.mode == MODE_3D) && !isnan(gps_data.fix.latitude) && !isnan(gps_data.fix.longitude)) {
				log(LL_DEBUG, "latitude: %f, longitude: %f, speed: %f, timestamp: %f", gps_data.fix.latitude, gps_data.fix.longitude, gps_data.fix.speed, gps_data.fix.time);

				const uint64_t discard_ts = get_us() + 1000000;

				md -> m -> set_double("$latitude$", std::pair<uint64_t, double>(discard_ts, gps_data.fix.latitude));
				md -> m -> set_double("$longitude$", std::pair<uint64_t, double>(discard_ts, gps_data.fix.longitude));
				md -> m -> set_double("$altitude$", std::pair<uint64_t, double>(discard_ts, gps_data.fix.altitude));
			}
		}

		usleep(10000);
	}

	log(LL_INFO, "GPS meta plugin thread ending");

	return NULL;
}

extern "C" void *init_plugin(meta *const m, const char *const argument)
{
	my_data_t *md = new my_data_t;
	md -> m = m;
	md -> par = argument ? strdup(argument) : NULL;
	md -> stop_flag = false;

	pthread_create(&md -> th, NULL, thread, md);

	return md;
}

extern "C" void uninit_plugin(void *arg)
{
	my_data_t *md = (my_data_t *)arg;

	md -> stop_flag = true;

	pthread_join(md -> th, NULL);

	free(md -> par);

	delete md;
}
