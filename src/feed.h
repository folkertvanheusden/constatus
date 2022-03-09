#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <string>
#include <thread>


class feed
{
protected:
	std::atomic_bool        stop_flag { false   };
	std::thread            *th        { nullptr };
	std::condition_variable cond;
        mutable std::mutex      lock;
	std::string             latest_text;
	uint64_t                latest_ts { 0       };

public:
	feed();
	virtual ~feed();

	std::optional<std::pair<std::string, uint64_t> > wait_for_text(const uint64_t after, const uint64_t to);

	virtual void operator()() = 0;
};
