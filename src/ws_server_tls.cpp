// (C) 2017-2023 by folkert van heusden, released under the MIT license
#include "config.h"
#if HAVE_WEBSOCKETPP == 1
#include <stddef.h>
#include "gen.h"
#include "webservices.h"
#include "interface.h"
#include "ws_server_tls.h"
#include "http_server.h"
#include "http_cookies.h"
#include "utils.h"
#include "db.h"

ws_server_tls::ws_server_tls(configuration_t *const cfg, const std::string & id, const std::string & descr, const listen_adapter_t & la, http_server *const hs, const ssl_pars_t & sp, const bool use_auth, http_cookies *const c) : ws_server(cfg, id, descr, la, hs, use_auth, c), sp(sp)
{
}

ws_server_tls::~ws_server_tls()
{
}

std::string ws_server_tls::address_from_hdl(connection_hdl hdl)
{
	const auto con = m_server_tls.get_con_from_hdl(hdl);

	return con->get_remote_endpoint();
}

bool ws_server_tls::on_validate(connection_hdl hdl)
{
	if (!use_auth)
		return true;

	auto con = m_server_tls.get_con_from_hdl(hdl);
	auto uri = con->get_uri();

	std::string query = uri->get_query();

	return c->get_cookie_user(query).empty() == false;
}

void ws_server_tls::push(const std::string & msg, const std::string & cache_under)
{
	for(auto it : m_connections)
		m_server_tls.send(it, msg, websocketpp::frame::opcode::text);

	if (!cache_under.empty()) {
		const std::lock_guard<std::shared_mutex> lock(cache_lock);

		cache.insert_or_assign(cache_under, msg);
	}
}

void ws_server_tls::ws_stop()
{
	m_server_tls.stop();
}

void ws_server_tls::operator()()
{
	m_server_tls.init_asio();

	m_server_tls.set_open_handler(bind(&ws_server_tls::on_open, this, ::_1));
	m_server_tls.set_close_handler(bind(&ws_server_tls::on_close, this, ::_1));
	m_server_tls.set_message_handler(bind(&ws_server_tls::on_message, this, ::_1, ::_2));
	m_server_tls.set_fail_handler(bind(&ws_server_tls::on_fail, this, ::_1));
	m_server_tls.set_tls_init_handler(bind(&ws_server_tls::on_tls_init, this, ::_1));

	m_server_tls.set_reuse_addr(true);
	m_server_tls.listen(la.adapter, myformat("%d", la.port));
	m_server_tls.start_accept();
	m_server_tls.run();
}

namespace asio = websocketpp::lib::asio;

websocketpp::lib::shared_ptr<asio::ssl::context> ws_server_tls::on_tls_init(websocketpp::connection_hdl)
{
	auto ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

	try {
		ctx->set_options(asio::ssl::context::default_workarounds |
				asio::ssl::context::no_sslv2 |
				asio::ssl::context::no_sslv3 |
				asio::ssl::context::single_dh_use);
	}
	catch (std::exception& e) {
		std::cout << "Error in context pointer: " << e.what() << std::endl;
	}

	ctx->use_certificate_chain_file(sp.certificate_file);
	ctx->use_private_key_file(sp.key_file, asio::ssl::context::pem);
	ctx->set_verify_mode(asio::ssl::verify_none);

	return ctx;
}
#endif
