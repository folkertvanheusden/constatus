// (C) 2020 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <mosquitto.h>
#include <pthread.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <cairo/cairo.h>
#include "../cairo.h"
#include "../filter_add_text.h"
#include "../utils.h"

extern "C" {

typedef struct {
	const char *host;
	int port;
	const char *topic;
	struct mosquitto *mi;
} pars_t;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
char *buffer1 = nullptr;
char *buffer2 = nullptr;
char *buffer3 = nullptr;

void * thread(void *p)
{
	struct mosquitto *mi = (struct mosquitto *)p;

	for(;;)
		mosquitto_loop(mi, 11000, 1);
}

void on_message(struct mosquitto *, void *, const struct mosquitto_message *msg, const mosquitto_property *)
{
	pthread_mutex_lock(&lock);

	free(buffer1);
	buffer1 = buffer2;
	buffer2 = buffer3;

	buffer3 = (char *)malloc(msg -> payloadlen + 1);
	memcpy(buffer3, msg -> payload, msg -> payloadlen);
	buffer3[msg -> payloadlen] = 0x00;

	printf("Received MQTT message: %s\n", buffer3);

	pthread_mutex_unlock(&lock);
}

/* FIXME error handling */
void * init_filter(const char *const parameter)
{
	mosquitto_lib_init();

	char *temp = strdup(parameter);
	pars_t *p = new pars_t;
	char *saveptr = nullptr;

	p -> host = strdup(strtok_r(temp, ":", &saveptr));
	p -> port = atoi(strtok_r(nullptr, ":", &saveptr));
	p -> topic = strdup(strtok_r(nullptr, ":", &saveptr));

	p -> mi = mosquitto_new("constatus", true, nullptr);

	int rc = 0;
	if ((rc = mosquitto_connect(p -> mi, p -> host, p -> port, 30)) != MOSQ_ERR_SUCCESS)
		printf("mosquitto_connect failed %d (%s)\n", rc, strerror(errno));

	mosquitto_message_v5_callback_set(p -> mi, on_message);

	if ((rc = mosquitto_subscribe(p -> mi, nullptr, p -> topic, 0)) != MOSQ_ERR_SUCCESS)
		printf("mosquitto_subscribe failed %d (%s)\n", rc, strerror(errno));

	pthread_t th;
	pthread_create(&th, nullptr, thread, p -> mi);

	free(temp);

	return p;
}

void apply_filter(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	pthread_mutex_lock(&lock);
	std::string text1 = buffer1 ? buffer1 : "";
	std::string text2 = buffer2 ? buffer2 : "";
	std::string text3 = buffer3 ? buffer3 : "";
	pthread_mutex_unlock(&lock);
	std::string text_out = text1 + "\\n" + text2 + "\\n" + text3;

	uint32_t *temp = NULL;
	cairo_surface_t *const cs = rgb_to_cairo(current_frame, w, h, &temp);
	cairo_t *const cr = cairo_create(cs);

	///
	cairo_select_font_face(cr, "Sans serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 32);

	int n_lines = 0, n_cols = 0;
	find_text_dim(text_out.c_str(), &n_lines, &n_cols);

	std::vector<std::string> *parts = split(text_out.c_str(), "\\n");

	double R = 123 / 255.0, G = 96 / 255.0, B = 208 / 255.0;

	int work_y = 32;
	for(std::string cl : *parts) {
		cairo_set_source_rgb(cr, 1.0 - R, 1.0 - G, 1.0 - B); 
		cairo_move_to(cr, 0, work_y);
		cairo_show_text(cr, cl.c_str());

		cairo_set_source_rgb(cr, R, G, B);
		cairo_move_to(cr, 0 + 1, work_y + 1);

		cairo_show_text(cr, cl.c_str());

		work_y += 32 + 1;
	}
	///

	delete parts;

	cairo_to_rgb(cs, w, h, result);

	cairo_destroy(cr);
	cairo_surface_destroy(cs);
	free(temp);
}

void uninit_filter(void *arg)
{
	/* FIXME clean-up */
}

}
