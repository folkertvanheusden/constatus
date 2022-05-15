#include "config.h"
#if HAVE_LIBMOSQUITTO == 1
#include <mosquitto.h>

#include "error.h"
#include "feed_mqtt.h"
#include "log.h"
#include "utils.h"


static void on_message(struct mosquitto *, void *p, const struct mosquitto_message *msg, const mosquitto_property *)
{
	feed_mqtt *fm = reinterpret_cast<feed_mqtt *>(p);

	std::string text = std::string(reinterpret_cast<const char *>(msg->payload), msg->payloadlen);

	log(LL_DEBUG, "Received MQTT text: %s", text.c_str());

	fm->set_text(text);
}

static void on_connect(struct mosquitto *, void *p, int)
{
	feed_mqtt *fm = reinterpret_cast<feed_mqtt *>(p);

	log(LL_INFO, "Connected to MQTT server");

	fm->subscribe_topics();
}

feed_mqtt::feed_mqtt(const std::string & host, const int port, const std::vector<std::string> & topics) :
	topics(topics)
{
	mosquitto_lib_init();

	mqtt = mosquitto_new(nullptr, true, this);
	if (!mqtt)
		error_exit(false, "Failed to create mqtt instance");

	int rc = mosquitto_connect(mqtt, host.c_str(), port, 30);

	if (rc != MOSQ_ERR_SUCCESS)
		error_exit(false, "Failed connecting to MQTT server on [%s]:%d", host.c_str(), port);

	mosquitto_connect_callback_set(mqtt, on_connect);

	mosquitto_message_v5_callback_set(mqtt, on_message);

	th = new std::thread(std::ref(*this));
}

feed_mqtt::~feed_mqtt()
{
	stop_flag = true;

	th->join();

	delete th;
}

void feed_mqtt::set_text(const std::string & text)
{
	std::lock_guard<std::mutex> lck(lock);

	latest_text = text;

	latest_ts = get_us();

	cond.notify_all();
}

void feed_mqtt::subscribe_topics()
{
	for(auto & topic : topics) {
		log(LL_DEBUG, "Subscribing to MQTT topic \"%s\"", topic.c_str());

		int rc = mosquitto_subscribe(mqtt, nullptr, topic.c_str(), 0);

		if (rc != MOSQ_ERR_SUCCESS)
			log(LL_WARNING, "Failed to subscribe to MQTT topic \"%s\": %s", topic.c_str(), mosquitto_strerror(rc));
	}
}

void feed_mqtt::operator()()
{
	set_thread_name("feed_mqtt");

	while(!stop_flag) {
		mosquitto_loop(mqtt, 500, 1);
	}
}
#endif
