// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"
#if USE_WEBSOCKET_SCTP
#include "transport.h"
#include "log.h"
#include "settings.h"
#include <atomic>
#include <cerrno>
#include <deque>
#include <map>
#include <mutex>
#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#include <emscripten/threading.h>
#else
#include "endpoint.h"
#include "http.h"
#ifndef USE_WS_SCTP_SSL
#define USE_WS_SCTP_SSL 0
#endif
#if USE_WS_SCTP_SSL
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#include "tls.h"
#else
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#endif
#endif

namespace ws_transport
{
namespace
{
std::string connectionUri(const Address &address, bool secure)
{
	std::string uri = makeUri(address, secure);
	// Allows a DNS name matching a TLS certificate, or a reverse-proxy path.
	std::string configured_uri;
	g_settings->getNoEx("ws_sctp_url", configured_uri);
	if (!configured_uri.empty())
		uri = configured_uri;
	return uri;
}
}
struct Transport::Impl
{
	std::mutex mutex;
	std::vector<Event> events;
	struct Send
	{
		Id id;
		std::string data;
	};
	std::deque<Send> outgoing;
	size_t outgoing_bytes = 0;
	std::atomic_bool running{false};
	size_t incoming_bytes = 0;
	bool overflow = false;
	void push(Event event)
	{
		std::lock_guard lock(mutex);
		if (event.type == Type::Message) {
			if (overflow)
				return;
			if (incoming_bytes + event.data.size() > 16 * 1024 * 1024) {
				overflow = true;
				events.push_back(
						{Type::Failed, event.id, event.client, {}, "receive queue full"});
				return;
			}
			incoming_bytes += event.data.size();
		}
		events.push_back(std::move(event));
	}
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_WEBSOCKET_T socket = 0;
	uintptr_t key = 0;
	// Callbacks carry an integer generation, never an owning object's address.
	// Removing this weak registration makes already queued callbacks harmless.
	static std::mutex registry_mutex;
	static std::map<uintptr_t, std::weak_ptr<Impl>> registry;
	static std::atomic<uintptr_t> next_key;
	static std::shared_ptr<Impl> lookup(void *data)
	{
		std::lock_guard lock(registry_mutex);
		auto it = registry.find(reinterpret_cast<uintptr_t>(data));
		return it == registry.end() ? nullptr : it->second.lock();
	}
#else
#if USE_WS_SCTP_SSL
	using Server = websocketpp::server<websocketpp::config::asio_tls>;
	using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
#else
	using Server = websocketpp::server<websocketpp::config::asio>;
	using Client = websocketpp::client<websocketpp::config::asio_client>;
#endif
	Server server;
	Client client;
	Id next_id = 1;
	struct Peer
	{
		websocketpp::connection_hdl handle;
		bool client;
	};
	std::map<Id, Peer> peers;
	std::map<websocketpp::connection_hdl, Id,
			std::owner_less<websocketpp::connection_hdl>>
			ids;
	Id add(websocketpp::connection_hdl hdl, bool outbound, Address address = {})
	{
		const Id id = next_id++;
		peers.emplace(id, Peer{hdl, outbound});
		ids.emplace(hdl, id);
		push({Type::Open, id, outbound, address, {}});
		return id;
	}
	void finish(websocketpp::connection_hdl hdl, bool outbound, bool failed)
	{
		auto it = ids.find(hdl);
		if (it == ids.end()) {
			if (failed)
				push({Type::Failed, 0, outbound, {}, {}});
			return;
		}
		const auto id = it->second;
		ids.erase(it);
		peers.erase(id);
		push({failed ? Type::Failed : Type::Closed, id, outbound, {}, {}});
	}
	Impl()
	{
		initServer(server);
		initEndpoint(client);
		server.set_http_handler([this](auto hdl) { serveHttp(server, hdl); });
		server.set_open_handler([this](auto hdl) {
			boost::system::error_code error;
			auto endpoint =
					server.get_con_from_hdl(hdl)->get_raw_socket().remote_endpoint(error);
			if (error) {
				websocketpp::lib::error_code close_error;
				server.close(hdl, websocketpp::close::status::internal_endpoint_error,
						"endpoint error", close_error);
				return;
			}
			Address address = endpoint.address().is_v6()
									  ? Address(*reinterpret_cast<const sockaddr_in6 *>(
												endpoint.data()))
									  : Address(*reinterpret_cast<const sockaddr_in *>(
												endpoint.data()));
			add(hdl, false, address);
		});
		client.set_open_handler([this](auto hdl) { add(hdl, true); });
		server.set_close_handler([this](auto hdl) { finish(hdl, false, false); });
		client.set_close_handler([this](auto hdl) { finish(hdl, true, false); });
		server.set_fail_handler([this](auto hdl) { finish(hdl, false, true); });
		client.set_fail_handler([this](auto hdl) { finish(hdl, true, true); });
		const auto message = [this](auto hdl, auto msg) {
			auto it = ids.find(hdl);
			if (it == ids.end())
				return;
			const auto &peer = peers.at(it->second);
			if (msg->get_opcode() != websocketpp::frame::opcode::binary) {
				websocketpp::lib::error_code error;
				if (peer.client)
					client.close(hdl, websocketpp::close::status::unsupported_data,
							"binary required", error);
				else
					server.close(hdl, websocketpp::close::status::unsupported_data,
							"binary required", error);
				return;
			}
			push({Type::Message, it->second, peer.client, {}, msg->get_payload()});
		};
		server.set_message_handler(message);
		client.set_message_handler(message);
#if USE_WS_SCTP_SSL
		server.set_tls_init_handler([](auto) { return serverTlsContext(); });
		client.set_tls_init_handler([](auto) { return clientTlsContext(); });
#endif
	}
#endif
};
#ifdef __EMSCRIPTEN__
std::mutex Transport::Impl::registry_mutex;
std::map<uintptr_t, std::weak_ptr<Transport::Impl>> Transport::Impl::registry;
std::atomic<uintptr_t> Transport::Impl::next_key{1};
#endif
Transport::Transport() : m_impl(std::make_shared<Impl>())
{
}
Transport::~Transport()
{
	stop();
}
bool Transport::connect(const Address &address)
{
	auto &state = *m_impl;
#ifdef __EMSCRIPTEN__
	if (!emscripten_websocket_is_supported() || state.socket)
		return false;
	const bool secure =
			MAIN_THREAD_EM_ASM_INT({ return location.protocol == = 'https:'; });
	const auto uri = connectionUri(address, secure);
	EmscriptenWebSocketCreateAttributes attributes = {};
	attributes.url = uri.c_str();
	attributes.createOnMainThread = true;
	state.socket = emscripten_websocket_new(&attributes);
	if (state.socket <= 0) {
		state.socket = 0;
		return false;
	}
	state.key = Impl::next_key.fetch_add(1);
	{
		std::lock_guard lock(Impl::registry_mutex);
		Impl::registry.emplace(state.key, m_impl);
	}
	void *key = reinterpret_cast<void *>(state.key);
	// Deliver on the browser thread: the connection worker runs a blocking loop.
	const auto thread = emscripten_main_runtime_thread_id();
	int registration = EMSCRIPTEN_RESULT_SUCCESS;
	registration |= emscripten_websocket_set_onopen_callback_on_thread(
			state.socket, key,
			[](int, const EmscriptenWebSocketOpenEvent *, void *key) -> EM_BOOL {
				if (auto state = Impl::lookup(key))
					state->push({Type::Open, 1, true, {}, {}});
				return true;
			},
			thread);
	registration |= emscripten_websocket_set_onmessage_callback_on_thread(
			state.socket, key,
			[](int, const EmscriptenWebSocketMessageEvent *event, void *key) -> EM_BOOL {
				if (auto state = Impl::lookup(key)) {
					if (event->isText)
						state->push({Type::Failed, 1, true, {}, "binary required"});
					else
						state->push({Type::Message, 1, true, {},
								std::string(reinterpret_cast<const char *>(event->data),
										event->numBytes)});
				}
				return true;
			},
			thread);
	registration |= emscripten_websocket_set_onerror_callback_on_thread(
			state.socket, key,
			[](int, const EmscriptenWebSocketErrorEvent *, void *key) -> EM_BOOL {
				if (auto state = Impl::lookup(key))
					state->push({Type::Failed, 1, true, {}, {}});
				return true;
			},
			thread);
	registration |= emscripten_websocket_set_onclose_callback_on_thread(
			state.socket, key,
			[](int, const EmscriptenWebSocketCloseEvent *, void *key) -> EM_BOOL {
				if (auto state = Impl::lookup(key))
					state->push({Type::Closed, 1, true, {}, {}});
				return true;
			},
			thread);
	if (registration != EMSCRIPTEN_RESULT_SUCCESS) {
		stop();
		return false;
	}
#else
	state.client.reset();
	websocketpp::lib::error_code error;
	auto connection =
			state.client.get_connection(connectionUri(address, USE_WS_SCTP_SSL), error);
	if (error) {
		errorstream << "WebSocket connect: " << error.message() << std::endl;
		return false;
	}
	state.client.connect(connection);
#endif
	state.running = true;
	return true;
}
bool Transport::listen(const Address &address)
{
#ifdef __EMSCRIPTEN__
	return false; // Browsers expose outbound WebSockets only.
#else
	auto &state = *m_impl;
	state.server.reset();
	websocketpp::lib::error_code error;
	boost::system::error_code address_error;
	auto ip = websocketpp::lib::asio::ip::make_address(
			address.serializeString(), address_error);
	if (address_error)
		return false;
	state.server.listen(
			websocketpp::lib::asio::ip::tcp::endpoint(ip, address.getPort()), error);
	if (!error)
		state.server.start_accept(error);
	if (error) {
		errorstream << "WebSocket listen: " << error.message() << std::endl;
		return false;
	}
	state.running = true;
	return true;
#endif
}
int Transport::send(Id id, const void *data, size_t size)
{
	if (size > 16 * 1024 * 1024)
		return EMSGSIZE;
	auto &state = *m_impl;
	std::lock_guard lock(state.mutex);
	if (!state.running.load(std::memory_order_acquire))
		return EINVAL;
	if (state.outgoing_bytes + size > 16 * 1024 * 1024)
		return EWOULDBLOCK;
	state.outgoing.push_back({id, std::string(static_cast<const char *>(data), size)});
	state.outgoing_bytes += size;
	return 0;
}
void Transport::close(Id id)
{
#ifdef __EMSCRIPTEN__
	if (m_impl->socket)
		emscripten_websocket_close(m_impl->socket, 1000, "peer removed");
#else
	auto &state = *m_impl;
	auto peer = state.peers.find(id);
	if (peer == state.peers.end())
		return;
	websocketpp::lib::error_code error;
	if (peer->second.client)
		state.client.close(peer->second.handle, websocketpp::close::status::going_away,
				"peer removed", error);
	else
		state.server.close(peer->second.handle, websocketpp::close::status::going_away,
				"peer removed", error);
#endif
}
std::vector<Transport::Event> Transport::poll()
{
	auto &state = *m_impl;
	std::deque<Impl::Send> outgoing;
	{
		std::lock_guard lock(state.mutex);
		outgoing.swap(state.outgoing);
		state.outgoing_bytes = 0;
	}
	for (auto &packet : outgoing) {
#ifdef __EMSCRIPTEN__
		if (state.socket &&
				emscripten_websocket_send_binary(state.socket, packet.data.data(),
						packet.data.size()) != EMSCRIPTEN_RESULT_SUCCESS)
			state.push({Type::Failed, packet.id, true, {}, "send failed"});
#else
		const auto peer = state.peers.find(packet.id);
		if (peer == state.peers.end())
			continue;
		websocketpp::lib::error_code error;
		if (peer->second.client)
			state.client.send(peer->second.handle, packet.data,
					websocketpp::frame::opcode::binary, error);
		else
			state.server.send(peer->second.handle, packet.data,
					websocketpp::frame::opcode::binary, error);
		if (error)
			state.push(
					{Type::Failed, packet.id, peer->second.client, {}, error.message()});
#endif
	}
#ifndef __EMSCRIPTEN__
	try {
		for (unsigned i = 0; i < 64; ++i) {
			const auto server_work = state.server.poll_one();
			const auto client_work = state.client.poll_one();
			if (!server_work && !client_work)
				break;
		}
	} catch (const std::exception &error) {
		errorstream << "WebSocket poll: " << error.what() << std::endl;
		if (state.peers.empty())
			state.push({Type::Failed, 0, true, {}, error.what()});
		else
			for (const auto &[id, peer] : state.peers)
				state.push({Type::Failed, id, peer.client, {}, error.what()});
	}
#endif
	std::lock_guard lock(state.mutex);
	std::vector<Event> result;
	result.swap(state.events);
	state.incoming_bytes = 0;
	state.overflow = false;
	return result;
}
void Transport::stop()
{
	auto &state = *m_impl;
	state.running.store(false, std::memory_order_release);
#ifdef __EMSCRIPTEN__
	{
		std::lock_guard lock(Impl::registry_mutex);
		Impl::registry.erase(state.key);
	}
	if (state.socket) {
		emscripten_websocket_close(state.socket, 1000, "disconnect");
		emscripten_websocket_delete(state.socket);
		state.socket = 0;
	}
#else
	websocketpp::lib::error_code error;
	state.server.stop_listening(error);
	for (const auto &[id, peer] : state.peers) {
		error.clear();
		if (peer.client)
			state.client.close(peer.handle, websocketpp::close::status::going_away,
					"disconnect", error);
		else
			state.server.close(peer.handle, websocketpp::close::status::going_away,
					"disconnect", error);
	}
	state.server.stop();
	state.client.stop();
	state.peers.clear();
	state.ids.clear();
#endif
	state.running = false;
	std::lock_guard lock(state.mutex);
	state.events.clear();
	state.outgoing.clear();
	state.outgoing_bytes = 0;
	state.incoming_bytes = 0;
	state.overflow = false;
}
}
#endif
