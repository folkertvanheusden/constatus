#include "feed.h"
#include "utils.h"


feed::feed()
{
}

feed::~feed()
{
}

std::optional<std::string> feed::wait_for_text(const uint64_t after, const uint64_t to)
{
	std::unique_lock<std::mutex> lck(lock);

	uint64_t stop_at = get_us() + to;

	while(latest_ts <= after || latest_ts == 0) {
		int64_t time_left = stop_at - get_us();

		if (time_left <= 0 || cond.wait_for(lck, std::chrono::microseconds(to)) == std::cv_status::timeout)
			return { };
	}

	return latest_text;
}
