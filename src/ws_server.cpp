// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#if HAVE_WEBSOCKETPP == 1
#include <jansson.h>
#include <stddef.h>
#include "gen.h"
#include "interface.h"
#include "ws_server.h"
#include "log.h"
#include "db.h"
#include "http_server.h"
#include "http_cookies.h"
#include "utils.h"

std::string publish_gstats(configuration_t *const cfg)
{
	json_t *json = json_object();

	json_object_set_new(json, "type", json_string("gstats"));

	struct rusage ru { 0 };
	getrusage(RUSAGE_SELF, &ru);
	json_object_set_new(json, "rss", json_integer(ru.ru_maxrss));

	json_object_set_new(json, "gbw", json_real(http_server::global_st.get_bw() / 1024.0));
	json_object_set_new(json, "gcc", json_real(http_server::global_st.get_cc()));

	json_object_set_new(json, "gcpu", json_real(cfg->st->get_cpu_usage() * 100.0));

	char *js = json_dumps(json, JSON_COMPACT);
	std::string msg = js;
	free(js);

	json_decref(json);

	return msg;
}

ws_server::ws_server(configuration_t *const cfg, const std::string & id, const std::string & descr, const listen_adapter_t & la, http_server *const hs, const bool use_auth, http_cookies *const c) : interface(id, descr), cfg(cfg), la(la), hs(hs), use_auth(use_auth), c(c)
{
	m_server.clear_access_channels(websocketpp::log::alevel::all); 
}

ws_server::~ws_server()
{
	stop();
}

void ws_server::ws_stop()
{
	m_server.stop();
}

void ws_server::operator()()
{
	m_server.init_asio();

	m_server.set_open_handler(bind(&ws_server::on_open, this, ::_1));
	m_server.set_close_handler(bind(&ws_server::on_close, this, ::_1));
	m_server.set_message_handler(bind(&ws_server::on_message, this, ::_1, ::_2));
	m_server.set_fail_handler(bind(&ws_server::on_fail, this, ::_1));
	m_server.set_validate_handler(bind(&ws_server::on_validate, this, ::_1));

	m_server.set_reuse_addr(true);
	m_server.listen(la.adapter, myformat("%d", la.port));
	m_server.start_accept();
	m_server.run();
}

bool ws_server::on_validate(connection_hdl hdl)
{
	if (!use_auth)
		return true;

	auto con = m_server.get_con_from_hdl(hdl);
	auto uri = con->get_uri();

	std::string query = uri->get_query();

	return c->get_cookie_user(query).empty() == false;
}

std::string ws_server::address_from_hdl(connection_hdl hdl)
{
	const auto con = m_server.get_con_from_hdl(hdl);

	return con->get_remote_endpoint();
}

void ws_server::on_open(connection_hdl hdl)
{
	m_connections.insert(hdl);

	log(LL_INFO, "websocket connection opened (%s)", address_from_hdl(hdl).c_str());

	const std::shared_lock lock(cache_lock);

	for(auto it : cache)
		m_server.send(hdl, it.second, websocketpp::frame::opcode::text);
}

void ws_server::on_close(connection_hdl hdl)
{
	m_connections.erase(hdl);

	log(LL_INFO, "websocket connection closed (%s)", address_from_hdl(hdl).c_str());
}

void ws_server::push(const std::string & msg, const std::string & cache_under)
{
	for(auto it : m_connections)
		m_server.send(it, msg, websocketpp::frame::opcode::text);

	if (!cache_under.empty()) {
		const std::lock_guard<std::shared_mutex> lock(cache_lock);

		cache.insert_or_assign(cache_under, msg);
	}
}

void ws_server::on_message(connection_hdl hdl, server::message_ptr msg)
{
	auto msg_str = msg->get_payload();

	json_error_t js_err { 0 };
	json_t *js = json_loadb(msg_str.c_str(), msg_str.size(), 0, &js_err);
	if (!js) {
		log(LL_WARNING, "JS decode error: %s", js_err.text);
		return;
	}

	json_t *js_obj = json_object_get(js, "type");
	if (!js_obj) {
		log(LL_WARNING, "JS: \"type\" missing");
		json_decref(js);
		return;
	}

	if (strcmp(json_string_value(js_obj), "gstats") == 0)
		push(publish_gstats(cfg), "gstats");

	json_decref(js);
}

void ws_server::on_fail(websocketpp::connection_hdl hdl)
{
	websocketpp::server<websocketpp::config::asio>::connection_ptr con = m_server.get_con_from_hdl(hdl);
	websocketpp::lib::error_code ec = con->get_ec();

	log(id, LL_WARNING, "ws-socket error: %s", ec.message().c_str());
}
#endif
