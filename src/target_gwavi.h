// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"

#if HAVE_LIBGWAVI == 1
extern "C" {
#include <libgwavi/gwavi.h>
}

#include "target.h"

class target_gwavi : public target
{
private:
	const int   quality { 99      };
	gwavi_t    *gwavi   { nullptr };
	int         f_nr    { 0       };
	std::string name;
	int         fps     { 25      };

	void put_frame(video_frame *const f);
	void open_file();
	void close_file();

public:
	target_gwavi(const std::string & id, const std::string & descr, source *const s, const std::string & store_path, const std::string & prefix, const std::string & fmt, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const double override_fps, configuration_t *const cfg, const bool is_view_proxy, const bool handle_failure, schedule *const sched);
	virtual ~target_gwavi();

	void operator()();
};

#endif
