// (C) 2017-2023 by folkert van heusden, released under the MIT license
#pragma once
#include "config.h"
#if HAVE_WEBSOCKETPP == 1
#include <set>
#include "cfg.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class http_server;
class http_cookies;

class ws_server : public interface
{
private:
	server m_server;

protected:
	configuration_t *const cfg;
	const listen_adapter_t la;
	http_server *const hs;
	const bool use_auth;
	http_cookies *const c;

	typedef std::set<connection_hdl,std::owner_less<connection_hdl>> con_list;
	con_list m_connections;

	std::shared_mutex cache_lock;
	std::map<std::string, std::string> cache;

public:
	ws_server(configuration_t *const cfg, const std::string & id, const std::string & descr, const listen_adapter_t & la, http_server *const hs, const bool use_auth, http_cookies *c);
	virtual ~ws_server();

	int get_port() const { return la.port; }

	void on_open(connection_hdl hdl);
	void on_close(connection_hdl hdl);
	void on_message(connection_hdl hdl, server::message_ptr msg);
	void on_fail(websocketpp::connection_hdl hdl);
	virtual bool on_validate(connection_hdl hdl);

	virtual std::string address_from_hdl(connection_hdl hdl);

	virtual void push(const std::string & msg, const std::string & cache_under);

	virtual void ws_stop();

	virtual void operator()() override;
};
#endif
