// (C) 2021 by folkert van heusden, this file is released in the public domain
#include <atomic>
#include <mosquitto.h>
#include <pthread.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <cairo/cairo.h>
#include "../error.h"
#include "../picio.h"

extern "C" {

typedef struct {
	const char *host;
	int port;
	const char *topic;
	struct mosquitto *mi;
	std::atomic_bool overlay_on;
	int w, h;
	uint8_t *pixels;
} pars_t;

void * thread(void *p)
{
	struct mosquitto *mi = (struct mosquitto *)p;

	for(;;)
		mosquitto_loop(mi, 11000, 1);
}

void on_message(struct mosquitto *, void *pIn, const struct mosquitto_message *msg, const mosquitto_property *)
{
	pars_t *p = (pars_t *)pIn;

	char *buffer = (char *)malloc(msg->payloadlen + 1);
	memcpy(buffer, msg->payload, msg->payloadlen);
	buffer[msg->payloadlen] = 0x00;

	bool temp = p->overlay_on = strcmp(buffer, "1") == 0 || strcasecmp(buffer, "on") == 0 || strcasecmp(buffer, "true") == 0;

	printf("Received MQTT message: %s, new overlay status: %d\n", buffer, temp);

	free(buffer);
}

/* TODO error handling */
void * init_filter(const char *const parameter)
{
	mosquitto_lib_init();

	char *temp = strdup(parameter);
	pars_t *p = new pars_t();
	char *saveptr = nullptr;

	p->host = strdup(strtok_r(temp, ":", &saveptr));
	p->port = atoi(strtok_r(nullptr, ":", &saveptr));
	p->topic = strdup(strtok_r(nullptr, ":", &saveptr));
	std::string file = strdup(strtok_r(nullptr, ":", &saveptr));

	p->mi = mosquitto_new("constatus", true, p);

	int rc = 0;
	if ((rc = mosquitto_connect(p->mi, p->host, p->port, 30)) != MOSQ_ERR_SUCCESS)
		printf("mosquitto_connect failed %d (%s)\n", rc, strerror(errno));

	mosquitto_message_v5_callback_set(p->mi, on_message);

	if ((rc = mosquitto_subscribe(p->mi, 0, p->topic, 0)) != MOSQ_ERR_SUCCESS)
		printf("mosquitto_subscribe failed %d (%s)\n", rc, strerror(errno));

	pthread_t th;
	pthread_create(&th, nullptr, thread, p->mi);

	free(temp);

	FILE *fh = fopen(file.c_str(), "rb");
	if (!fh)
		error_exit(true, "%s failed to read", file.c_str());

	read_PNG_file_rgba(fh, &p->w, &p->h, &p->pixels);

	fclose(fh);

	return p;
}

void uninit_filter(void *arg)
{
	/* TODO clean-up */
}

void apply_filter(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	pars_t *p = (pars_t *)arg;

	memcpy(result, current_frame, w * h * 3);

	if (p->overlay_on) {
		int cw = std::min(p->w, w);
		int ch = std::min(p->h, h);

		if (cw <= 0 || ch <= 0)
			return;

		for(int y=0; y<ch; y++) {
			for(int x=0; x<ch; x++) {
				const int out_offset = y * w * 3 + x * 3;
				const int pic_offset = y * p->w * 4 + x * 4;

				uint8_t alpha = p->pixels[pic_offset + 3], ialpha = 255 - alpha;
				result[out_offset + 0] = (int(p->pixels[pic_offset + 0]) * alpha + (current_frame[out_offset + 0] * ialpha)) / 256;
				result[out_offset + 1] = (int(p->pixels[pic_offset + 1]) * alpha + (current_frame[out_offset + 1] * ialpha)) / 256;
				result[out_offset + 2] = (int(p->pixels[pic_offset + 2]) * alpha + (current_frame[out_offset + 2] * ialpha)) / 256;
			}
		}
	}
}

}
