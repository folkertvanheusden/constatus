#include "exec.h"
#include "feed_exec.h"
#include "utils.h"


feed_exec::feed_exec(const std::string & commandline, const int interval_ms) :
	commandline(commandline),
	interval_ms(interval_ms)
{
	th = new std::thread(std::ref(*this));
}

feed_exec::~feed_exec()
{
	stop_flag = true;

	th->join();

	delete th;
}

void feed_exec::operator()()
{
	while(!stop_flag) {
		uint64_t before_ts = get_us();

		FILE    *fh        = exec(commandline);
		if (fh) {
			std::lock_guard<std::mutex> lck(lock);

			latest_text.clear();

			while(!feof(fh)) {
                                char buffer[1024] = { 0 };

                                if (fgets(buffer, sizeof buffer, fh) == nullptr)
					break;

				latest_text += buffer;
			}

			pclose(fh);

			if (latest_text.empty() == false) {
				latest_ts = before_ts;

				cond.notify_all();
			}
		}

		uint64_t after_ts       = get_us();

		int64_t  sleep_duration = interval_ms * 1000 - (after_ts - before_ts);

		mysleep(sleep_duration, &stop_flag, nullptr);
	}
}
