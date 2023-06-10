// (C) 2022-2023 by folkert van heusden, MIT license
#include <atomic>
#include <mosquitto.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#include "../log.h"
#include "../source.h"
#include "../picio.h"
#include "../resize.h"
#include "../utils.h"


typedef struct
{
	source     *s;
	int         w;
        int         h;
	std::string topic;
	std::atomic_bool stop_flag;
	pthread_t   th;

	std::mutex  lock;
	uint8_t    *pixels;
} my_data_t;

typedef struct {
	uint8_t *data;
	size_t   len;
} curl_recv_t;

static size_t curl_add_to_memory(void *new_data, size_t size, size_t nmemb, void *userp)
{
	size_t total_size  = size * nmemb;
	curl_recv_t       *mem = (curl_recv_t *)userp;

	if (mem->len + total_size > 16 * 1024 * 1024)  // sanity limit
		return 0;

	mem->data = (uint8_t *)realloc(mem->data, mem->len + total_size);

	memcpy(&mem->data[mem->len], new_data, total_size);

	mem->len += total_size;

	return total_size;
}

static void connect_callback(struct mosquitto *mi, void *arg, int result)
{
	my_data_t *md = (my_data_t *)arg;

	log(LL_INFO, "subscribe to %s", md->topic.c_str());

	int rc = 0;
	if ((rc = mosquitto_subscribe(mi, nullptr, md->topic.c_str(), 0)) != MOSQ_ERR_SUCCESS)
		log(LL_ERR, "mosquitto_subscribe failed %d (%s)", rc, strerror(errno));
}

static void on_message(struct mosquitto *, void *arg, const struct mosquitto_message *msg, const mosquitto_property *)
{
	my_data_t *md = (my_data_t *)arg;

	std::string url = std::string((const char *)msg->payload, msg->payloadlen);

	CURL *curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	log(LL_INFO, "Will retrieve %s", url.c_str());

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_add_to_memory);

	curl_recv_t data { 0 };
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

	resize r;

	if (curl_easy_perform(curl) == CURLE_OK) {
		int w = 0, h = 0;

		std::string mime_type = "image/jpeg";  // default assumption
		char *ct = nullptr;

		if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct)
			mime_type = ct;

		uint8_t *temp = nullptr;
		bool     ok   = false;

		mime_type = str_tolower(mime_type);

		if (mime_type == "image/jpeg")
			ok = my_jpeg.read_JPEG_memory(data.data, data.len, &w, &h, &temp);
		else if (mime_type == "image/png") {
			FILE *fh = fmemopen(data.data, data.len, "rb");

			ok = read_PNG_file_rgba(false, fh, &w, &h, &temp);

			fclose(fh);
		}
		// TODO other mime-types
		else {
			log(LL_INFO, "Ignoring unknown mime-type %s", mime_type.c_str());
		}

		if (ok) {
			uint8_t *out = (uint8_t *)calloc(3, md->w * md->h);

			int perc = std::min(md->w * 100 / w, md->h * 100 / h);
			log(LL_INFO, "Resize to %d%%", perc);

			pos_t p { center_center, 0, 0 };

			picture_in_picture(&r, out, md->w, md->h, temp, w, h, perc, p);

			{
				std::lock_guard<std::mutex> lck(md->lock);
				free(md->pixels);
				md->pixels = out;
			}
		}
		else {
			log(LL_INFO, "Url not shown");
		}

		free(temp);
	}

	curl_easy_cleanup(curl);

	free(data.data);
}

void * pixel_thread(void *arg)
{
	log(LL_INFO, "source plugin thread started");

	my_data_t *md = (my_data_t *)arg;

	for(;;) {
		if (md->pixels) {
			std::lock_guard<std::mutex> lck(md->lock);

			// send RGB frame to constatus
			md->s->set_frame(E_RGB, md->pixels, md->w * md->h * 3);
		}

		usleep(499000);
	}

	log(LL_INFO, "source plugin thread ending");

	return NULL;
}

static void * mqtt_thread(void *p)
{
	struct mosquitto *mi = (struct mosquitto *)p;

	for(;;) {
		mosquitto_loop_forever(mi, -1, 1);

		log(LL_INFO, "Reconnect MQTT");

		sleep(1);

		mosquitto_reconnect(mi);
	}
}

extern "C" void *init_plugin(source *const s, const char *const argument)
{
	my_data_t *md = new my_data_t();
	md->s         = s;
	md->w         = s->get_width();
	md->h         = s->get_height();
	md->stop_flag = false;

	mosquitto_lib_init();

	std::vector<std::string> *parts = split(argument, " ");

	std::string host  = parts->at(0).c_str();
	int         port  = atoi(parts->at(1).c_str());
	std::string topic = parts->at(2).c_str();
	md -> topic       = topic;

	delete parts;

	log(LL_INFO, "Connecting to MQTT host [%s]:%d", host.c_str(), port);

	struct mosquitto *mi = mosquitto_new("plugin-constatus", true, md);
	if (!mi)
		log(LL_ERR, "mosquitto_new failed");

	int rc = 0;
	if ((rc = mosquitto_connect(mi, host.c_str(), port, 30)) != MOSQ_ERR_SUCCESS)
		log(LL_ERR, "mosquitto_connect failed %d (%s)", rc, strerror(errno));

	mosquitto_message_v5_callback_set(mi, on_message);

	mosquitto_connect_callback_set(mi, connect_callback);

	pthread_t th1;
	pthread_create(&th1, nullptr, pixel_thread, md);

	pthread_t th2;
	pthread_create(&th2, nullptr, mqtt_thread, mi);

	return md;
}

extern "C" void uninit_plugin(void *arg)
{
	my_data_t *md = (my_data_t *)arg;

	md -> stop_flag = true;

	pthread_join(md -> th, NULL);

	delete md;
}
