// (C) 2020-2023 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <mosquitto.h>
#include <pthread.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <cairo/cairo.h>

#include "cairo.h"
#include "../filter_add_text.h"
#include "../utils.h"

extern "C" {

typedef struct {
	const char *host;
	int port;
	const char *topic;
	struct mosquitto *mi;
	std::vector<std::string> lines;
	size_t n_lines;
} pars_t;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void * thread(void *p)
{
	struct mosquitto *mi = (struct mosquitto *)p;

	for(;;) {
		if (mosquitto_loop(mi, 11000, 1)) {
			sleep(1);
			mosquitto_reconnect(mi);
		}
	}
}

void on_message(struct mosquitto *, void *p, const struct mosquitto_message *msg, const mosquitto_property *)
{
	pars_t *pars = (pars_t *)p;

	pthread_mutex_lock(&lock);

	pars->lines.push_back(std::string((const char *)msg->payload, msg->payloadlen));

	while(pars->lines.size() > pars->n_lines)
		pars->lines.erase(pars->lines.begin());

	pthread_mutex_unlock(&lock);
}

/* FIXME error handling */
void * init_filter(const char *const parameter)
{
	mosquitto_lib_init();

	char *temp = strdup(parameter);
	pars_t *p = new pars_t;

	std::vector<std::string> *parts = split(parameter, ":");

	p -> host = strdup(parts->at(0).c_str());
	p -> port = atoi(parts->at(1).c_str());
	p -> topic = strdup(parts->at(2).c_str());
	p -> n_lines = atoi(parts->at(3).c_str());

	delete parts;

	p -> mi = mosquitto_new("constatus", true, p);

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
	pars_t *p = (pars_t *)arg;

	std::string text_out;

	pthread_mutex_lock(&lock);
	for(auto line : p->lines) {
		if (text_out.empty() == false)
			text_out += "\\n";

		text_out += line;
	}
	pthread_mutex_unlock(&lock);

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
