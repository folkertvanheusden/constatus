#include "config.h"
#if HAVE_LIBMOSQUITTO == 1
#include <mosquitto.h>

#include <string>
#include <vector>

#include "feed.h"


class feed_mqtt : public feed
{
private:
	const std::vector<std::string> topics;
	struct mosquitto *mqtt { nullptr };

public:
	feed_mqtt(const std::string & host, const int port, const std::vector<std::string> & topics);
	virtual ~feed_mqtt();

	void set_text(const std::string & text);
	void subscribe_topics();

	void operator()() override;
};
#endif
