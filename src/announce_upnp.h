// (C) 2017-2021 by folkert van heusden, released under Apache License v2.0
#pragma once
#include "config.h"
#if HAVE_RYGEL == 1
#include <stddef.h>

#include <gio/gio.h>
#include <rygel-server.h>
#include <rygel-core.h>

#include "gen.h"
#include "cfg.h"
#include "interface.h"

class http_server;

class announce_upnp : public interface
{
private:
	configuration_t *const cfg;
	const std::vector<std::string> announce_ids;
	const std::vector<std::string> interfaces;

	GMainLoop *loop { nullptr };

public:
	announce_upnp(configuration_t *const cfg, const std::string & id, const std::string & descr, const std::vector<std::string> & announce_ids, const std::vector<std::string> &interfaces);
	virtual ~announce_upnp();

	virtual void stop();

	virtual void operator()() override;
};
#endif
