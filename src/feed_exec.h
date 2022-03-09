#include "feed.h"


class feed_exec : public feed
{
private:
	const std::string commandline;
	const int         interval_ms;

public:
	feed_exec(const std::string & commandline, const int interval_ms);
	virtual ~feed_exec();

	void operator()() override;
};
