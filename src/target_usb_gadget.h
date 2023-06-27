// (C) 2023 by folkert van heusden, released under the MIT license
#pragma once

#include "config.h"

#if HAVE_USBGADGET == 1
#include <optional>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <usbg/usbg.h>
#include <usbg/function/uvc.h>

#include "target.h"


class schedule;

class target_usbgadget : public target
{
private:
	const int quality     { 0 };

        usbg_state  *ug_state { nullptr };
        usbg_gadget *g        { nullptr };
        usbg_config *c        { nullptr };
        usbg_function *f_uvc  { nullptr };

	std::optional<std::string> setup();

public:
	target_usbgadget(const std::string & id, const std::string & descr, source *const s, const double interval, const std::vector<filter *> *const filters, const double override_fps, configuration_t *const cfg, const int quality, const bool handle_failure, schedule *const sched);
	virtual ~target_usbgadget();

	void operator()() override;
};
#endif
