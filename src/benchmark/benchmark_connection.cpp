#include "catch.h"
#include "benchmark/fm_benchmark.h"
#include "config.h"
#include "network/connection.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/peerhandler.h"
#include "network/mtp/internal.h"
#include "porting.h"
#include "settings.h"

#if MINETEST_TRANSPORT
#include "network/mtp/impl.h"
#endif
#if USE_SCTP
#include "network/sctp/connection.h"
#endif
#if USE_ENET
#include "network/enet/connection.h"
#endif
#if USE_WEBSOCKET
#include "network/ws/impl.h"
#endif
#if USE_WEBSOCKET_SCTP
#include "network/ws_sctp/connection.h"
#endif
#if USE_MULTI
#include "network/multi/connection.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

static constexpr u32 CONNECTION_BENCHMARK_MAX_PACKET_SIZE = 1350;
#if USE_WEBSOCKET
static constexpr u32 CONNECTION_BENCHMARK_WS_MAX_PACKET_SIZE = 100000;
#endif
static constexpr float CONNECTION_BENCHMARK_TIMEOUT = 5.0f;
static constexpr u16 CONNECTION_BENCHMARK_COMMAND = 0x7ffe;
static constexpr u32 CONNECTION_BENCHMARK_CONNECT_TIMEOUT_MS = 5000;
static constexpr u32 CONNECTION_BENCHMARK_TRAFFIC_TIMEOUT_MS = 10000;
static constexpr u16 CONNECTION_BENCHMARK_EXTERNAL_PORT = 42000;
// Mark the requested response reliability in external benchmark packets.
static constexpr u8 CONNECTION_BENCHMARK_UNRELIABLE = 0x80;

enum class ExternalBenchmarkPacketMode : char
{
	SendAck = 1,
	ReceiveRequest = 2,
	Echo = 3,
	Ack = 4,
	Response = 5,
	Ready = 6,
	ReadyAck = 7,
};

static constexpr auto g_connection_client_counts = {
		1,
		8,
		32,
};

static constexpr auto g_connection_payload_sizes = {
		64,
		1024,
		32 * 1024,
};

enum class TrafficMode
{
	ClientToServer,
	ServerToClient,
	Both,
};

struct BenchmarkPeerHandler : public con::PeerHandler
{
	void peerAdded(session_t peer_id) override { peer_ids.push_back(peer_id); }

	void deletingPeer(session_t peer_id, bool) override
	{
		auto it = std::find(peer_ids.begin(), peer_ids.end(), peer_id);
		if (it != peer_ids.end())
			peer_ids.erase(it);
	}

	std::vector<session_t> peer_ids;
};

using ConnectionFactory =
		std::function<std::unique_ptr<con::IConnection>(con::PeerHandler *)>;

struct ConnectionBenchmarkBackend
{
	std::string name;
	ConnectionFactory factory;
	std::string remote_proto;
	std::string skip_reason;
	u16 connect_port_offset = 0;
	bool multi = false;
};

static std::vector<ConnectionBenchmarkBackend> getConnectionBenchmarkBackends()
{
	std::vector<ConnectionBenchmarkBackend> backends;

#if MINETEST_TRANSPORT
	backends.push_back({
			"mt",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::Connection>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"",
			"",
			0,
			false,
	});
#endif

#if USE_SCTP
	backends.push_back({
			"sctp",
			[](con::PeerHandler *handler) {
				return std::make_unique<con_sctp::Connection>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"",
			"",
			0,
			false,
	});
#endif

#if USE_ENET
	backends.push_back({
			"enet",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::ConnectionEnet>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"",
			"",
			0,
			false,
	});
#endif

#if USE_WEBSOCKET
	backends.push_back({
			"ws",
			[](con::PeerHandler *handler) {
				return std::make_unique<con_ws::Connection>(
						CONNECTION_BENCHMARK_WS_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"",
			"",
			0,
			false,
	});
#endif

#if USE_WEBSOCKET_SCTP
	backends.push_back({
			"WebSocketSCTP",
			[](con::PeerHandler *handler) {
				return std::make_unique<con_ws_sctp::Connection>(PROTOCOL_ID,
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"",
			"WebSocketSCTP currently uses process-global usrsctp state and is "
			"not stable in the single-process benchmark harness",
			0,
			false,
	});
#endif

#if USE_MULTI && MINETEST_TRANSPORT
	backends.push_back({
			"multi_mt",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::ConnectionMulti>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"mt",
			"",
			0,
			true,
	});
#endif

#if USE_MULTI && USE_SCTP
	backends.push_back({
			"multi_sctp",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::ConnectionMulti>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"sctp",
			"",
			100,
			true,
	});
#endif

#if USE_MULTI && USE_WEBSOCKET
	backends.push_back({
			"multi_ws",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::ConnectionMulti>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"ws",
			"",
			0,
			true,
	});
#endif

#if USE_MULTI && USE_WEBSOCKET_SCTP
	backends.push_back({
			"multi_ws_sctp",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::ConnectionMulti>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"ws_sctp",
			"WebSocketSCTP currently uses process-global usrsctp state and is "
			"not stable in the single-process benchmark harness",
			100,
			true,
	});
#endif

#if USE_MULTI && USE_ENET
	backends.push_back({
			"multi_enet",
			[](con::PeerHandler *handler) {
				return std::make_unique<con::ConnectionMulti>(
						CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
						CONNECTION_BENCHMARK_TIMEOUT, true, handler);
			},
			"enet",
			"",
			200,
			true,
	});
#endif

	return backends;
}

static u16 nextBenchmarkPort()
{
	static u16 next_port = static_cast<u16>(
			42000 + (std::chrono::steady_clock::now().time_since_epoch().count() % 8000));
	const u16 port = next_port;
	next_port += 7;
	return port;
}

static u16 getBenchmarkBasePort()
{
	const auto &options = get_connection_benchmark_options();
	if (options.port != 0)
		return options.port;

	if (options.mode == ConnectionBenchmarkMode::ClientServer)
		return nextBenchmarkPort();

	return CONNECTION_BENCHMARK_EXTERNAL_PORT;
}

static Address makeLoopbackAddress(u16 port)
{
	sockaddr_in6 addr = {};
	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_loopback;
	addr.sin6_port = htons(port);
	return Address(addr);
}

static Address makeAnyAddress(u16 port)
{
	sockaddr_in6 addr = {};
	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(port);
	return Address(addr);
}

static Address resolveBenchmarkServerAddress(const std::string &server, u16 port)
{
	Address address = makeLoopbackAddress(port);
	try {
		address.Resolve(server.c_str());
	} catch (const ResolveError &e) {
		throw std::runtime_error(
				"connection benchmark server resolve failed: " + std::string(e.what()));
	}

	if (address.isAny())
		address.setAddress(in6addr_loopback);
	address.setPort(port);
	return address;
}

static std::string formatPayloadSize(size_t bytes)
{
	if (bytes >= 1024 && bytes % 1024 == 0)
		return std::to_string(bytes / 1024) + " KiB";

	return std::to_string(bytes) + " B";
}

static std::string trafficModeName(TrafficMode mode)
{
	switch (mode) {
	case TrafficMode::ClientToServer:
		return "send";
	case TrafficMode::ServerToClient:
		return "receive";
	case TrafficMode::Both:
		return "both";
	}
	return "unknown";
}

// Label and report both reliability variants.
static std::string makeConnectionBenchmarkName(const ConnectionBenchmarkBackend &backend,
		TrafficMode mode, size_t clients, size_t payload_size, bool reliable,
		bool random_size)
{
	std::string name = "Conn_" + backend.name + "_" + trafficModeName(mode) + "_" +
					   std::to_string(clients) + "c_" + std::to_string(payload_size) +
					   "B";
	if (random_size)
		name += "_random";
	const auto window = get_connection_benchmark_options().window;
	if (window > 1)
		name += "_" + std::to_string(window) + "w";
	if (!reliable)
		name += "_unreliable";
	return name;
}

struct ConnectionBenchmarkMetadata
{
	std::string backend;
	std::string mode;
	size_t clients = 0;
	size_t payload_size = 0;
	size_t window = 1;
	bool reliable = true;
	bool random_size = false;
};

static bool parseNumberWithSuffix(std::string_view value, char suffix, size_t *result)
{
	if (value.size() < 2 || value.back() != suffix)
		return false;

	size_t parsed = 0;
	for (size_t i = 0; i + 1 < value.size(); ++i) {
		const unsigned char c = static_cast<unsigned char>(value[i]);
		if (!std::isdigit(c))
			return false;
		parsed = parsed * 10 + (c - '0');
	}

	*result = parsed;
	return parsed != 0;
}

static std::optional<ConnectionBenchmarkMetadata> parseConnectionBenchmarkName(
		const std::string &name)
{
	constexpr std::string_view prefix = "Conn_";
	if (name.compare(0, prefix.size(), prefix) != 0)
		return std::nullopt;

	// Decode fixed fields from the right; backend names may contain underscores.
	std::string_view remaining(name.data() + prefix.size(), name.size() - prefix.size());
	bool reliable = true;
	constexpr std::string_view unreliable_suffix = "_unreliable";
	if (remaining.size() >= unreliable_suffix.size() &&
			remaining.substr(remaining.size() - unreliable_suffix.size()) ==
					unreliable_suffix) {
		remaining.remove_suffix(unreliable_suffix.size());
		reliable = false;
	}
	const auto takeLast = [&]() {
		const size_t separator = remaining.rfind('_');
		if (separator == std::string_view::npos)
			return std::string_view{};
		const auto value = remaining.substr(separator + 1);
		remaining = remaining.substr(0, separator);
		return value;
	};
	size_t window = 1;
	if (!remaining.empty() && remaining.back() == 'w' &&
			!parseNumberWithSuffix(takeLast(), 'w', &window))
		return std::nullopt;
	bool random_size = false;
	constexpr std::string_view random_suffix = "_random";
	if (remaining.size() >= random_suffix.size() &&
			remaining.substr(remaining.size() - random_suffix.size()) == random_suffix) {
		remaining.remove_suffix(random_suffix.size());
		random_size = true;
	}
	const auto size = takeLast();
	const auto clients_part = takeLast();
	const auto mode = takeLast();
	size_t clients = 0;
	size_t payload_size = 0;
	if (remaining.empty() || (mode != "send" && mode != "receive" && mode != "both") ||
			!parseNumberWithSuffix(clients_part, 'c', &clients) ||
			!parseNumberWithSuffix(size, 'B', &payload_size))
		return std::nullopt;

	return ConnectionBenchmarkMetadata{std::string(remaining), std::string(mode), clients,
			payload_size, window, reliable, random_size};
}

struct ConnectionBenchmarkResult
{
	std::string backend;
	std::string mode;
	bool reliable = true;
	size_t clients = 0;
	size_t payload_size = 0;
	size_t window = 1;
	bool random_size = false;
	size_t packets_per_batch = 0;
	double mean_ns = 0.0;
	double packets_per_second = 0.0;
	double mib_per_second = 0.0;
	double latency_us_per_packet = 0.0;
};

class ConnectionBenchmarkListener : public Catch::EventListenerBase
{
public:
	using Catch::EventListenerBase::EventListenerBase;

	static std::string getDescription()
	{
		return "prints derived connection benchmark throughput and latency";
	}

	void benchmarkEnded(Catch::BenchmarkStats<> const &stats) override
	{
		const auto metadata = parseConnectionBenchmarkName(stats.info.name);
		if (!metadata)
			return;

		const size_t packets_per_batch =
				metadata->clients * metadata->window * (metadata->mode == "both" ? 2 : 1);
		const double mean_ns = stats.mean.point.count();
		if (mean_ns <= 0.0 || packets_per_batch == 0)
			return;

		const double batches_per_second = 1000000000.0 / mean_ns;
		const double packets_per_second =
				batches_per_second * static_cast<double>(packets_per_batch);
		const double mib_per_second =
				packets_per_second * static_cast<double>(metadata->payload_size) /
				(metadata->random_size ? 2.0 : 1.0) / (1024.0 * 1024.0);
		const double latency_us_per_packet =
				mean_ns / static_cast<double>(packets_per_batch) / 1000.0;

		m_results.push_back({
				metadata->backend,
				metadata->mode,
				metadata->reliable,
				metadata->clients,
				metadata->payload_size,
				metadata->window,
				metadata->random_size,
				packets_per_batch,
				mean_ns,
				packets_per_second,
				mib_per_second,
				latency_us_per_packet,
		});
	}

	void testRunEnded(Catch::TestRunStats const &) override
	{
		if (m_results.empty())
			return;

		auto &out = Catch::cerr();
		if (std::any_of(m_results.begin(), m_results.end(),
					[](const auto &result) { return result.random_size; }))
			out << "\nRandom-size MiB/sec uses the expected mean payload size.\n";
		const auto old_flags = out.flags();
		const auto old_precision = out.precision();
		const auto has_unreliable = [&](const auto &predicate) {
			return std::any_of(
					m_results.begin(), m_results.end(), [&](const auto &result) {
						return !result.reliable && predicate(result.backend);
					});
		};
		if (has_unreliable([](const auto &) { return true; })) {
			out << "\nUnreliable results require every packet to arrive; loss causes a timeout.\n";
			if (has_unreliable([](const auto &backend) {
					return backend == "enet" || backend == "multi_enet";
				}))
				out << "ENet can send oversized unreliable packets as reliable fragments.\n";
			if (has_unreliable([](const auto &backend) {
					return backend == "ws" || backend == "multi_ws" ||
						   backend == "WebSocketSCTP" || backend == "multi_ws_sctp";
				}))
				out << "WebSocket traffic uses TCP.\n";
		}

		out << "\nConnection total packet throughput and latency "
			<< "(iops = packets/sec, derived from Catch2 benchmark mean)\n";
		out << std::left << std::setw(11) << "backend" << std::setw(9) << "mode"
			<< std::setw(12) << "reliability" << std::right << std::setw(8) << "clients"
			<< std::setw(8) << "window" << std::setw(10) << "size" << std::setw(15)
			<< "iops" << std::setw(15) << "MiB/sec" << std::setw(15) << "lat us/pkt"
			<< std::setw(15) << "mean us/batch" << '\n';
		out << std::string(118, '-') << '\n';

		for (const auto &result : m_results) {
			out << std::left << std::setw(11) << result.backend << std::setw(9)
				<< result.mode << std::setw(12)
				<< (result.reliable ? "reliable" : "unreliable") << std::right
				<< std::setw(8) << result.clients << std::setw(8) << result.window
				<< std::setw(10)
				<< (result.random_size ? "0-" + formatPayloadSize(result.payload_size)
									   : formatPayloadSize(result.payload_size))
				<< std::setw(15) << std::fixed << std::setprecision(0)
				<< result.packets_per_second << std::setw(15) << std::fixed
				<< std::setprecision(2) << result.mib_per_second << std::setw(15)
				<< std::fixed << std::setprecision(2) << result.latency_us_per_packet
				<< std::setw(15) << std::fixed << std::setprecision(2)
				<< result.mean_ns / 1000.0 << '\n';
		}

		out.flags(old_flags);
		out.precision(old_precision);
	}

private:
	std::vector<ConnectionBenchmarkResult> m_results;
};

CATCH_REGISTER_LISTENER(ConnectionBenchmarkListener)

static std::string generatePayload(size_t size)
{
	std::string payload;
	payload.resize(size);
	for (size_t i = 0; i < size; ++i)
		payload[i] = static_cast<char>((i * 31 + 17) & 0xff);
	return payload;
}

static void writePayloadSize(std::string *payload, size_t payload_size)
{
	if (payload->size() < 5)
		payload->resize(5);

	const u32 size = static_cast<u32>(payload_size);
	(*payload)[1] = static_cast<char>((size >> 24) & 0xff);
	(*payload)[2] = static_cast<char>((size >> 16) & 0xff);
	(*payload)[3] = static_cast<char>((size >> 8) & 0xff);
	(*payload)[4] = static_cast<char>(size & 0xff);
}

static size_t readPayloadSize(const NetworkPacket &pkt)
{
	if (pkt.getSize() < 5)
		return 0;

	const auto data = pkt.getRemainingNoCopy();
	const auto *bytes = reinterpret_cast<const unsigned char *>(data.data());
	return (static_cast<size_t>(bytes[1]) << 24) | (static_cast<size_t>(bytes[2]) << 16) |
		   (static_cast<size_t>(bytes[3]) << 8) | static_cast<size_t>(bytes[4]);
}

// Preserve the reliability request in the external packet header.
static NetworkPacket makeBenchmarkPacket(ExternalBenchmarkPacketMode mode,
		size_t packet_size, size_t requested_payload_size, bool reliable = true)
{
	std::string payload = generatePayload(packet_size);
	payload[0] = static_cast<char>(
			static_cast<u8>(mode) | (reliable ? 0 : CONNECTION_BENCHMARK_UNRELIABLE));
	writePayloadSize(&payload, requested_payload_size);

	NetworkPacket pkt(CONNECTION_BENCHMARK_COMMAND, payload.size());
	pkt.putRawString(payload);
	return pkt;
}

static NetworkPacket makeBenchmarkPacket(
		ExternalBenchmarkPacketMode mode, size_t payload_size)
{
	return makeBenchmarkPacket(mode, payload_size, payload_size);
}

static NetworkPacket makeBenchmarkPacket(const std::string &payload)
{
	NetworkPacket pkt(CONNECTION_BENCHMARK_COMMAND, payload.size());
	pkt.putRawString(payload);
	return pkt;
}

// Configure the benchmark server for this case's client count.
static void configureBenchmarkSettings(
		const ConnectionBenchmarkBackend &backend, u16 port, size_t client_count)
{
	const size_t max_users = std::max<size_t>(256, client_count);
	g_settings->setU16("max_users", static_cast<u16>(max_users));
	g_settings->setU16("timeout_mul", 1);
	g_settings->setBool("enable_ipv6", true);
	g_settings->setBool("ipv6_server", true);
	if (backend.multi) {
		g_settings->setU16("port_enet", port + 200);
		g_settings->setU16("port_sctp", port + 100);
		g_settings->setU16("port_wss", port);
		g_settings->setU16("port_sctp_wss", port + 100);
		g_settings->set("remote_proto", backend.remote_proto);
	}
}

static bool receiveOne(con::IConnection &connection, NetworkPacket *pkt)
{
	pkt->clear();
	return connection.ReceiveTimeoutMs(pkt, 1);
}

static void drainPendingPackets(con::IConnection &connection)
{
	NetworkPacket pkt;
	for (size_t i = 0; i < 1024; ++i) {
		if (!receiveOne(connection, &pkt))
			break;
	}
}

class ConnectionBenchmarkCluster
{
public:
	ConnectionBenchmarkCluster(const ConnectionBenchmarkBackend &backend,
			size_t client_count) : m_backend(backend), m_port(getBenchmarkBasePort())
	{
		configureBenchmarkSettings(m_backend, m_port, client_count);

		m_server = m_backend.factory(&m_server_handler);

		Address bind_addr = makeLoopbackAddress(m_port);
		m_server->Serve(bind_addr);

		std::this_thread::sleep_for(std::chrono::milliseconds(20));

		for (size_t i = 0; i < client_count; ++i) {
			auto handler = std::make_unique<BenchmarkPeerHandler>();
			auto client = m_backend.factory(handler.get());
			m_client_handlers.push_back(std::move(handler));
			m_clients.push_back(std::move(client));
		}

		const u16 connect_port = m_port + m_backend.connect_port_offset;
		Address connect_addr = makeLoopbackAddress(connect_port);
		for (auto &client : m_clients)
			client->Connect(connect_addr);

		waitConnected(client_count);
		flushTraffic();
	}

	~ConnectionBenchmarkCluster()
	{
		for (auto &client : m_clients)
			client->Disconnect();
		if (m_server)
			m_server->Disconnect();
	}

	// Pass the selected delivery mode to both sides.
	size_t runBatch(TrafficMode mode, NetworkPacket &packet, bool reliable)
	{
		const size_t window = get_connection_benchmark_options().window;
		size_t expected_server_packets = 0;
		size_t expected_client_packets = 0;

		if (mode == TrafficMode::ClientToServer || mode == TrafficMode::Both) {
			for (size_t i = 0; i < window; ++i)
				for (auto &client : m_clients)
					client->Send(PEER_ID_SERVER, 0, &packet, reliable);
			expected_server_packets = m_clients.size() * window;
		}

		if (mode == TrafficMode::ServerToClient || mode == TrafficMode::Both) {
			for (size_t i = 0; i < window; ++i)
				for (session_t peer_id : m_server_handler.peer_ids)
					m_server->Send(peer_id, 0, &packet, reliable);
			expected_client_packets = m_clients.size() * window;
		}

		const auto deadline =
				std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_TRAFFIC_TIMEOUT_MS);

		const size_t server_packets = receivePackets(
				*m_server, expected_server_packets, deadline, packet.getSize());

		size_t client_packets = 0;
		if (expected_client_packets > 0) {
			for (auto &client : m_clients)
				client_packets +=
						receivePackets(*client, window, deadline, packet.getSize());
		}

		const size_t packets = server_packets + client_packets;
		if (packets != expected_server_packets + expected_client_packets) {
			throw std::runtime_error("connection benchmark did not receive all packets");
		}
		return packets;
	}

private:
	void waitConnected(size_t client_count)
	{
		const auto deadline =
				std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_CONNECT_TIMEOUT_MS);

		NetworkPacket pkt;
		while (std::chrono::steady_clock::now() < deadline) {
			receiveOne(*m_server, &pkt);
			for (auto &client : m_clients)
				receiveOne(*client, &pkt);

			bool clients_connected = true;
			for (const auto &client : m_clients) {
				if (!client->Connected()) {
					clients_connected = false;
					break;
				}
			}

			if (m_server_handler.peer_ids.size() >= client_count && clients_connected)
				return;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		throw std::runtime_error("connection benchmark setup timed out");
	}

	void flushTraffic()
	{
		for (size_t i = 0; i < 8; ++i) {
			drainPendingPackets(*m_server);
			for (auto &client : m_clients)
				drainPendingPackets(*client);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	static size_t receivePackets(con::IConnection &connection, size_t expected,
			std::chrono::steady_clock::time_point deadline, size_t expected_size)
	{
		size_t received = 0;
		NetworkPacket pkt;
		while (received < expected && std::chrono::steady_clock::now() < deadline) {
			if (!receiveOne(connection, &pkt)) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				continue;
			}
			if (pkt.getSize() != expected_size)
				throw std::runtime_error(
						"connection benchmark received wrong packet size");
			++received;
		}
		return received;
	}

	const ConnectionBenchmarkBackend &m_backend;
	u16 m_port;
	BenchmarkPeerHandler m_server_handler;
	std::unique_ptr<con::IConnection> m_server;
	std::vector<std::unique_ptr<BenchmarkPeerHandler>> m_client_handlers;
	std::vector<std::unique_ptr<con::IConnection>> m_clients;
};

class ConnectionBenchmarkExternalServer
{
public:
	ConnectionBenchmarkExternalServer(const ConnectionBenchmarkBackend &backend,
			size_t client_count) : m_backend(backend), m_port(getBenchmarkBasePort())
	{
		configureBenchmarkSettings(m_backend, m_port, client_count);

		m_server = m_backend.factory(&m_server_handler);
		m_server->Serve(makeAnyAddress(m_port));

		Catch::cerr() << "\nConnection benchmark server listening on [::]:" << m_port
					  << " for " << client_count << " " << m_backend.name
					  << " client(s)\n";
		Catch::cerr() << "Connection benchmark server ready\n";
	}

	~ConnectionBenchmarkExternalServer()
	{
		if (m_server)
			m_server->Disconnect();
	}

	void run()
	{
		NetworkPacket pkt;
		volatile auto *kill = porting::signal_handler_killstatus();
		while (!*kill) {
			if (!receiveOne(*m_server, &pkt)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			handlePacket(pkt);
		}
	}

private:
	// Apply the requested reliability to measured server responses.
	void handlePacket(NetworkPacket &pkt)
	{
		if (pkt.getCommand() != CONNECTION_BENCHMARK_COMMAND || pkt.getSize() == 0)
			return;

		const auto payload = pkt.getRemainingNoCopy();
		const u8 flags = static_cast<u8>(payload[0]);
		const bool reliable = (flags & CONNECTION_BENCHMARK_UNRELIABLE) == 0;
		const auto mode = static_cast<ExternalBenchmarkPacketMode>(
				flags & ~CONNECTION_BENCHMARK_UNRELIABLE);
		switch (mode) {
		case ExternalBenchmarkPacketMode::Ready:
		case ExternalBenchmarkPacketMode::SendAck: {
			const auto ack_mode = mode == ExternalBenchmarkPacketMode::Ready
										  ? ExternalBenchmarkPacketMode::ReadyAck
										  : ExternalBenchmarkPacketMode::Ack;
			NetworkPacket ack = makeBenchmarkPacket(ack_mode, 5, 0);
			m_server->Send(pkt.getPeerId(), 0, &ack, true);
			return;
		}
		case ExternalBenchmarkPacketMode::ReceiveRequest: {
			const size_t payload_size = readPayloadSize(pkt);
			NetworkPacket response = makeBenchmarkPacket(
					ExternalBenchmarkPacketMode::Response, payload_size);
			m_server->Send(pkt.getPeerId(), 0, &response, reliable);
			return;
		}
		case ExternalBenchmarkPacketMode::Echo:
			m_server->Send(pkt.getPeerId(), 0, &pkt, reliable);
			return;
		default:
			return;
		}
	}

	const ConnectionBenchmarkBackend &m_backend;
	u16 m_port;
	BenchmarkPeerHandler m_server_handler;
	std::unique_ptr<con::IConnection> m_server;
};

class ConnectionBenchmarkExternalClient
{
public:
	ConnectionBenchmarkExternalClient(const ConnectionBenchmarkBackend &backend,
			size_t client_count) : m_backend(backend), m_port(getBenchmarkBasePort())
	{
		configureBenchmarkSettings(m_backend, m_port, client_count);

		const auto &options = get_connection_benchmark_options();
		const u16 connect_port = m_port + m_backend.connect_port_offset;
		Address connect_addr =
				resolveBenchmarkServerAddress(options.server, connect_port);

		for (size_t i = 0; i < client_count; ++i) {
			auto handler = std::make_unique<BenchmarkPeerHandler>();
			auto client = m_backend.factory(handler.get());
			m_client_handlers.push_back(std::move(handler));
			m_clients.push_back(std::move(client));
		}

		for (auto &client : m_clients)
			client->Connect(connect_addr);

		waitConnected();
		waitReady();
		flushTraffic();
	}

	~ConnectionBenchmarkExternalClient()
	{
		for (auto &client : m_clients)
			client->Disconnect();
	}

	// Keep control requests reliable and apply the selected mode to data.
	size_t runBatch(TrafficMode mode, size_t payload_size, bool reliable)
	{
		NetworkPacket send_packet =
				makeBenchmarkPacket(ExternalBenchmarkPacketMode::SendAck, payload_size,
						payload_size, reliable);
		NetworkPacket receive_request = makeBenchmarkPacket(
				ExternalBenchmarkPacketMode::ReceiveRequest, 5, payload_size, reliable);
		NetworkPacket echo_packet = makeBenchmarkPacket(
				ExternalBenchmarkPacketMode::Echo, payload_size, payload_size, reliable);

		NetworkPacket *packet = &send_packet;
		size_t expected_response_size = 5;
		const size_t window = get_connection_benchmark_options().window;
		size_t measured_packets = m_clients.size() * window;

		if (mode == TrafficMode::ServerToClient) {
			packet = &receive_request;
			expected_response_size = payload_size;
		} else if (mode == TrafficMode::Both) {
			packet = &echo_packet;
			expected_response_size = payload_size;
			measured_packets *= 2;
		}

		for (size_t i = 0; i < window; ++i)
			for (auto &client : m_clients)
				client->Send(PEER_ID_SERVER, 0, packet,
						mode == TrafficMode::ServerToClient || reliable);

		const auto deadline =
				std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_TRAFFIC_TIMEOUT_MS);

		for (auto &client : m_clients) {
			const size_t packets =
					receivePackets(*client, window, deadline, expected_response_size);
			if (packets != window)
				throw std::runtime_error(
						"connection benchmark external client missed response");
		}

		return measured_packets;
	}

private:
	void waitConnected()
	{
		const auto deadline =
				std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_CONNECT_TIMEOUT_MS);

		NetworkPacket pkt;
		while (std::chrono::steady_clock::now() < deadline) {
			bool clients_connected = true;
			for (auto &client : m_clients) {
				receiveOne(*client, &pkt);
				if (!client->Connected())
					clients_connected = false;
			}

			if (clients_connected)
				return;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		throw std::runtime_error("connection benchmark client setup timed out");
	}

	void waitReady()
	{
		NetworkPacket probe =
				makeBenchmarkPacket(ExternalBenchmarkPacketMode::Ready, 5, 0);
		std::vector<bool> ready(m_clients.size(), false);
		size_t ready_count = 0;
		const auto deadline =
				std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_TRAFFIC_TIMEOUT_MS);

		while (ready_count < m_clients.size() &&
				std::chrono::steady_clock::now() < deadline) {
			for (size_t i = 0; i < m_clients.size(); ++i) {
				if (!ready[i])
					m_clients[i]->Send(PEER_ID_SERVER, 0, &probe, true);
			}

			for (size_t i = 0; i < m_clients.size(); ++i) {
				if (ready[i])
					continue;
				const auto response_deadline = std::min(deadline,
						std::chrono::steady_clock::now() + std::chrono::milliseconds(5));
				if (receivePackets(*m_clients[i], 1, response_deadline, 5, true) == 1) {
					ready[i] = true;
					++ready_count;
				}
			}

			if (ready_count < m_clients.size())
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		if (ready_count != m_clients.size())
			throw std::runtime_error(
					"connection benchmark external client/server handshake timed out");
	}

	void flushTraffic()
	{
		for (size_t i = 0; i < 8; ++i) {
			for (auto &client : m_clients)
				drainPendingPackets(*client);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	static size_t receivePackets(con::IConnection &connection, size_t expected,
			std::chrono::steady_clock::time_point deadline, size_t expected_size,
			bool readiness = false)
	{
		size_t received = 0;
		NetworkPacket pkt;
		while (received < expected && std::chrono::steady_clock::now() < deadline) {
			if (!receiveOne(connection, &pkt)) {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				continue;
			}
			if (pkt.getCommand() != CONNECTION_BENCHMARK_COMMAND)
				continue;
			// Setup retries can leave late acknowledgements in the reliable queue.
			// Keep them distinct from measured responses rather than timing a drain.
			const auto payload = pkt.getRemainingNoCopy();
			if (payload.empty())
				continue;
			const bool ready_ack = static_cast<ExternalBenchmarkPacketMode>(payload[0]) ==
								   ExternalBenchmarkPacketMode::ReadyAck;
			if (ready_ack != readiness)
				continue;
			if (pkt.getSize() != expected_size)
				throw std::runtime_error(
						"connection benchmark received wrong response size");
			++received;
		}
		return received;
	}

	const ConnectionBenchmarkBackend &m_backend;
	u16 m_port;
	std::vector<std::unique_ptr<BenchmarkPeerHandler>> m_client_handlers;
	std::vector<std::unique_ptr<con::IConnection>> m_clients;
};

// Measure the selected reliability mode.
static void benchmarkConnectionTraffic(const ConnectionBenchmarkBackend &backend,
		size_t client_count, int64_t size_spec, TrafficMode mode, bool reliable)
{
	const bool random_size = size_spec < 0;
	const size_t payload_size = static_cast<size_t>(random_size ? -size_spec : size_spec);
	std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<size_t> distribution(0, payload_size);
	const std::string payload = generatePayload(random_size ? 0 : payload_size);
	NetworkPacket packet = makeBenchmarkPacket(payload);
	const auto &options = get_connection_benchmark_options();
	std::unique_ptr<ConnectionBenchmarkCluster> cluster;
	std::unique_ptr<ConnectionBenchmarkExternalClient> external_client;

	if (options.mode == ConnectionBenchmarkMode::ClientOnly) {
		external_client = std::make_unique<ConnectionBenchmarkExternalClient>(
				backend, client_count);
	} else {
		cluster = std::make_unique<ConnectionBenchmarkCluster>(backend, client_count);
	}

	BENCHMARK_ADVANCED(makeConnectionBenchmarkName(
			backend, mode, client_count, payload_size, reliable, random_size))
	(Catch::Benchmark::Chronometer meter)
	{
		meter.measure([&](int) {
			if (random_size) {
				const size_t selected_size = distribution(rng);
				if (external_client)
					return external_client->runBatch(mode, selected_size + 5, reliable);
				auto random_packet = makeBenchmarkPacket(generatePayload(selected_size));
				return cluster->runBatch(mode, random_packet, reliable);
			}
			if (external_client)
				return external_client->runBatch(mode, payload_size, reliable);
			return cluster->runBatch(mode, packet, reliable);
		});
	};
}

} // namespace

TEST_CASE("benchmark_connection_operations")
{
	const auto backends = getConnectionBenchmarkBackends();
	const auto &options = get_connection_benchmark_options();
	// Command-line lists override the checkout's existing benchmark defaults.
	const auto client_counts =
			options.client_counts.empty()
					? std::vector<size_t>(g_connection_client_counts.begin(),
							  g_connection_client_counts.end())
					: options.client_counts;
	const auto payload_sizes =
			options.payload_sizes.empty()
					? std::vector<int64_t>(g_connection_payload_sizes.begin(),
							  g_connection_payload_sizes.end())
					: options.payload_sizes;
	REQUIRE(!backends.empty());
	for (const auto &selected : options.backends) {
		if (std::none_of(backends.begin(), backends.end(),
					[&](const auto &backend) { return backend.name == selected; })) {
			Catch::cerr() << "Unknown connection benchmark backend: " << selected
						  << ". Available backends:";
			for (const auto &backend : backends)
				Catch::cerr() << ' ' << backend.name;
			Catch::cerr() << '\n';
			FAIL("unknown connection benchmark backend");
		}
	}

	for (const auto &backend : backends) {
		if (!options.backends.empty() &&
				std::find(options.backends.begin(), options.backends.end(),
						backend.name) == options.backends.end()) {
			continue;
		}
		SECTION(backend.name)
		{
			if (options.mode == ConnectionBenchmarkMode::ClientServer &&
					!backend.skip_reason.empty()) {
				Catch::cerr() << "\nSkipping " << backend.name
							  << " connection benchmark: " << backend.skip_reason << '\n';
				return;
			}

			// Iterate over the selected client counts and payload sizes.
			for (const size_t client_count : client_counts) {
				SECTION(std::to_string(client_count) + " clients")
				{
					for (const int64_t payload_size : payload_sizes) {
						SECTION(payload_size < 0
										? "random 0-" + formatPayloadSize(-payload_size)
										: formatPayloadSize(payload_size))
						{
							if (options.mode == ConnectionBenchmarkMode::ServerOnly) {
								ConnectionBenchmarkExternalServer server(
										backend, *std::max_element(client_counts.begin(),
														 client_counts.end()));
								server.run();
								return;
							}

							// Run each requested delivery variant.
							for (const bool reliable : {true, false}) {
								if ((reliable && options.reliability ==
														 ConnectionBenchmarkReliability::
																 Unreliable) ||
										(!reliable &&
												options.reliability ==
														ConnectionBenchmarkReliability::
																Reliable))
									continue;
								SECTION(reliable ? "reliable" : "unreliable")
								{
									benchmarkConnectionTraffic(backend, client_count,
											payload_size, TrafficMode::ClientToServer,
											reliable);
									benchmarkConnectionTraffic(backend, client_count,
											payload_size, TrafficMode::ServerToClient,
											reliable);
									benchmarkConnectionTraffic(backend, client_count,
											payload_size, TrafficMode::Both, reliable);
								}
							}
						}
					}
				}
			}
		}
	}
}
