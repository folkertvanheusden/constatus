// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "config.h"
#if HAVE_WEBSOCKETPP == 1
#include "webservices.h"
#include "ws_server.h"

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio_tls> server_tls;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class ws_server_tls : public ws_server
{
private:
	server_tls m_server_tls;
	const ssl_pars_t sp;

public:
	ws_server_tls(configuration_t *const cfg, const std::string & id, const std::string & descr, const listen_adapter_t & la, http_server *const hs, const ssl_pars_t & sp, const bool use_auth, http_cookies *const c);
	virtual ~ws_server_tls();

	virtual void push(const std::string & msg, const std::string & cache_under) override;

	bool on_validate(connection_hdl hdl) override;

	std::string address_from_hdl(connection_hdl hdl) override;

	void ws_stop() override;

	void operator()() override;

	websocketpp::lib::shared_ptr<boost::asio::ssl::context> on_tls_init(websocketpp::connection_hdl);
};
#endif
