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
#if USE_ENET
#include "network/enet/connection.h"
#endif
#if USE_SCTP
#include "network/sctp/connection.h"
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

enum class ExternalBenchmarkPacketMode : char
{
	SendAck = 1,
	ReceiveRequest = 2,
	Echo = 3,
	Ack = 4,
	Response = 5,
};

static constexpr auto g_connection_client_counts = {
	1,
	2,
	4,
//	8,
	//16,
	// 32,
	// 64,
	// 128,
};

static constexpr auto g_connection_payload_sizes = {
	64,
	256,
	1024,
//	4 * 1024,
//	16 * 1024,
	//64 * 1024,
};

enum class TrafficMode
{
	ClientToServer,
	ServerToClient,
	Both,
};

struct BenchmarkPeerHandler : public con::PeerHandler
{
	void peerAdded(session_t peer_id) override
	{
		peer_ids.push_back(peer_id);
	}

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
		"MTP",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::Connection>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
		},
		"",
		"",
		0,
		false,
	});
#endif

#if USE_ENET
	backends.push_back({
		"ENet",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::ConnectionEnet>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
		},
		"",
		"",
		0,
		false,
	});
#endif

#if USE_SCTP
	backends.push_back({
		"SCTP",
		[](con::PeerHandler *handler) {
			return std::make_unique<con_sctp::Connection>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
		},
		"",
		"raw SCTP uses process-global usrsctp UDP encapsulation; "
		"the single-process benchmark cannot create independent "
		"server/client SCTP stacks",
		0,
		false,
	});
#endif

#if USE_WEBSOCKET
	backends.push_back({
		"WebSocket",
		[](con::PeerHandler *handler) {
			return std::make_unique<con_ws::Connection>(
					CONNECTION_BENCHMARK_WS_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
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
			return std::make_unique<con_ws_sctp::Connection>(
					PROTOCOL_ID,
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
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
		"MultiMTP",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::ConnectionMulti>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
		},
		"mt",
		"",
		0,
		true,
	});
#endif

#if USE_MULTI && USE_SCTP
	backends.push_back({
		"MultiSCTP",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::ConnectionMulti>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
		},
		"sctp",
		"raw SCTP uses process-global usrsctp UDP encapsulation; "
		"the single-process benchmark cannot create independent "
		"server/client SCTP stacks",
		100,
		true,
	});
#endif

#if USE_MULTI && USE_WEBSOCKET
	backends.push_back({
		"MultiWebSocket",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::ConnectionMulti>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
		},
		"ws",
		"",
		0,
		true,
	});
#endif

#if USE_MULTI && USE_WEBSOCKET_SCTP
	backends.push_back({
		"MultiWebSocketSCTP",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::ConnectionMulti>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
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
		"MultiENet",
		[](con::PeerHandler *handler) {
			return std::make_unique<con::ConnectionMulti>(
					CONNECTION_BENCHMARK_MAX_PACKET_SIZE,
					CONNECTION_BENCHMARK_TIMEOUT,
					true,
					handler);
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
	static u16 next_port = static_cast<u16>(42000 +
			(std::chrono::steady_clock::now().time_since_epoch().count() % 8000));
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
		throw std::runtime_error("connection benchmark server resolve failed: " +
				std::string(e.what()));
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

static std::string makeConnectionBenchmarkName(const ConnectionBenchmarkBackend &backend,
		TrafficMode mode, size_t clients, size_t payload_size)
{
	return "Conn_" + backend.name + "_" + trafficModeName(mode) + "_" +
			std::to_string(clients) + "c_" + std::to_string(payload_size) + "B";
}

struct ConnectionBenchmarkMetadata
{
	std::string backend;
	std::string mode;
	size_t clients = 0;
	size_t payload_size = 0;
};

static bool parseNumberWithSuffix(
		std::string_view value, char suffix, size_t *result)
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

	std::array<std::string_view, 4> parts;
	size_t part = 0;
	size_t start = prefix.size();
	while (part < parts.size()) {
		const size_t separator = name.find('_', start);
		const size_t end = separator == std::string::npos ? name.size() : separator;
		parts[part++] = std::string_view(name).substr(start, end - start);
		if (separator == std::string::npos) {
			start = name.size();
			break;
		}
		start = separator + 1;
	}

	if (part != parts.size() || start < name.size())
		return std::nullopt;

	size_t clients = 0;
	size_t payload_size = 0;
	if (!parseNumberWithSuffix(parts[2], 'c', &clients) ||
			!parseNumberWithSuffix(parts[3], 'B', &payload_size)) {
		return std::nullopt;
	}

	return ConnectionBenchmarkMetadata{
		std::string(parts[0]),
		std::string(parts[1]),
		clients,
		payload_size,
	};
}

struct ConnectionBenchmarkResult
{
	std::string backend;
	std::string mode;
	size_t clients = 0;
	size_t payload_size = 0;
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

		const size_t packets_per_batch = metadata->clients *
				(metadata->mode == "both" ? 2 : 1);
		const double mean_ns = stats.mean.point.count();
		if (mean_ns <= 0.0 || packets_per_batch == 0)
			return;

		const double batches_per_second = 1000000000.0 / mean_ns;
		const double packets_per_second =
				batches_per_second * static_cast<double>(packets_per_batch);
		const double mib_per_second = packets_per_second *
				static_cast<double>(metadata->payload_size) / (1024.0 * 1024.0);
		const double latency_us_per_packet =
				mean_ns / static_cast<double>(packets_per_batch) / 1000.0;

		m_results.push_back({
			metadata->backend,
			metadata->mode,
			metadata->clients,
			metadata->payload_size,
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
		const auto old_flags = out.flags();
		const auto old_precision = out.precision();

		out << "\nConnection total packet throughput and latency "
			<< "(iops = packets/sec, derived from Catch2 benchmark mean)\n";
		out << std::left << std::setw(11) << "backend"
			<< std::setw(9) << "mode"
			<< std::right << std::setw(8) << "clients"
			<< std::setw(10) << "size"
			<< std::setw(15) << "iops"
			<< std::setw(15) << "MiB/sec"
			<< std::setw(15) << "lat us/pkt"
			<< std::setw(15) << "mean us/batch" << '\n';
		out << std::string(98, '-') << '\n';

		for (const auto &result : m_results) {
			out << std::left << std::setw(11) << result.backend
				<< std::setw(9) << result.mode
				<< std::right << std::setw(8) << result.clients
				<< std::setw(10) << formatPayloadSize(result.payload_size)
				<< std::setw(15) << std::fixed << std::setprecision(0)
				<< result.packets_per_second
				<< std::setw(15) << std::fixed << std::setprecision(2)
				<< result.mib_per_second
				<< std::setw(15) << std::fixed << std::setprecision(2)
				<< result.latency_us_per_packet
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

	const auto *data = reinterpret_cast<const unsigned char *>(pkt.getString(0));
	return (static_cast<size_t>(data[1]) << 24) |
			(static_cast<size_t>(data[2]) << 16) |
			(static_cast<size_t>(data[3]) << 8) |
			static_cast<size_t>(data[4]);
}

static NetworkPacket makeBenchmarkPacket(ExternalBenchmarkPacketMode mode,
		size_t packet_size, size_t requested_payload_size)
{
	std::string payload = generatePayload(packet_size);
	payload[0] = static_cast<char>(mode);
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

static void configureBenchmarkSettings(
		const ConnectionBenchmarkBackend &backend, u16 port)
{
	g_settings->setU16("max_users", 256);
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
			size_t client_count) :
		m_backend(backend),
		m_port(getBenchmarkBasePort())
	{
		configureBenchmarkSettings(m_backend, m_port);

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

	size_t runBatch(TrafficMode mode, NetworkPacket &packet)
	{
		size_t expected_server_packets = 0;
		size_t expected_client_packets = 0;

		if (mode == TrafficMode::ClientToServer || mode == TrafficMode::Both) {
			for (auto &client : m_clients)
				client->Send(PEER_ID_SERVER, 0, &packet, true);
			expected_server_packets = m_clients.size();
		}

		if (mode == TrafficMode::ServerToClient || mode == TrafficMode::Both) {
			for (session_t peer_id : m_server_handler.peer_ids)
				m_server->Send(peer_id, 0, &packet, true);
			expected_client_packets = m_clients.size();
		}

		const auto deadline = std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_TRAFFIC_TIMEOUT_MS);

		const size_t server_packets = receivePackets(
				*m_server, expected_server_packets, deadline, packet.getSize());

		size_t client_packets = 0;
		if (expected_client_packets > 0) {
			for (auto &client : m_clients)
				client_packets += receivePackets(*client, 1, deadline, packet.getSize());
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
		const auto deadline = std::chrono::steady_clock::now() +
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
				throw std::runtime_error("connection benchmark received wrong packet size");
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
			size_t client_count) :
		m_backend(backend),
		m_port(getBenchmarkBasePort())
	{
		configureBenchmarkSettings(m_backend, m_port);

		m_server = m_backend.factory(&m_server_handler);
		m_server->Serve(makeAnyAddress(m_port));

		Catch::cerr() << "\nConnection benchmark server listening on [::]:"
					  << m_port << " for " << client_count << " "
					  << m_backend.name << " client(s)\n";
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
	void handlePacket(NetworkPacket &pkt)
	{
		if (pkt.getCommand() != CONNECTION_BENCHMARK_COMMAND || pkt.getSize() == 0)
			return;

		const auto mode = static_cast<ExternalBenchmarkPacketMode>(pkt.getString(0)[0]);
		switch (mode) {
		case ExternalBenchmarkPacketMode::SendAck: {
			NetworkPacket ack = makeBenchmarkPacket(
					ExternalBenchmarkPacketMode::Ack, 5, 0);
			m_server->Send(pkt.getPeerId(), 0, &ack, true);
			return;
		}
		case ExternalBenchmarkPacketMode::ReceiveRequest: {
			const size_t payload_size = readPayloadSize(pkt);
			NetworkPacket response = makeBenchmarkPacket(
					ExternalBenchmarkPacketMode::Response, payload_size);
			m_server->Send(pkt.getPeerId(), 0, &response, true);
			return;
		}
		case ExternalBenchmarkPacketMode::Echo:
			m_server->Send(pkt.getPeerId(), 0, &pkt, true);
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
			size_t client_count) :
		m_backend(backend),
		m_port(getBenchmarkBasePort())
	{
		configureBenchmarkSettings(m_backend, m_port);

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

	size_t runBatch(TrafficMode mode, size_t payload_size)
	{
		NetworkPacket send_packet = makeBenchmarkPacket(
				ExternalBenchmarkPacketMode::SendAck, payload_size);
		NetworkPacket receive_request = makeBenchmarkPacket(
				ExternalBenchmarkPacketMode::ReceiveRequest, 5, payload_size);
		NetworkPacket echo_packet = makeBenchmarkPacket(
				ExternalBenchmarkPacketMode::Echo, payload_size);

		NetworkPacket *packet = &send_packet;
		size_t expected_response_size = 5;
		size_t measured_packets = m_clients.size();

		if (mode == TrafficMode::ServerToClient) {
			packet = &receive_request;
			expected_response_size = payload_size;
		} else if (mode == TrafficMode::Both) {
			packet = &echo_packet;
			expected_response_size = payload_size;
			measured_packets *= 2;
		}

		for (auto &client : m_clients)
			client->Send(PEER_ID_SERVER, 0, packet, true);

		const auto deadline = std::chrono::steady_clock::now() +
				std::chrono::milliseconds(CONNECTION_BENCHMARK_TRAFFIC_TIMEOUT_MS);

		for (auto &client : m_clients) {
			const size_t packets = receivePackets(
					*client, 1, deadline, expected_response_size);
			if (packets != 1)
				throw std::runtime_error(
						"connection benchmark external client missed response");
		}

		return measured_packets;
	}

private:
	void waitConnected()
	{
		const auto deadline = std::chrono::steady_clock::now() +
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
		NetworkPacket probe = makeBenchmarkPacket(
				ExternalBenchmarkPacketMode::SendAck, 5, 0);
		std::vector<bool> ready(m_clients.size(), false);
		size_t ready_count = 0;
		const auto deadline = std::chrono::steady_clock::now() +
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
						std::chrono::steady_clock::now() +
								std::chrono::milliseconds(5));
				if (receivePackets(*m_clients[i], 1, response_deadline, 5) == 1) {
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
			std::chrono::steady_clock::time_point deadline, size_t expected_size)
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

static void benchmarkConnectionTraffic(const ConnectionBenchmarkBackend &backend,
		size_t client_count, size_t payload_size, TrafficMode mode)
{
	const std::string payload = generatePayload(payload_size);
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
			backend, mode, client_count, payload_size))(
			Catch::Benchmark::Chronometer meter)
	{
		meter.measure([&](int) {
			if (external_client)
				return external_client->runBatch(mode, payload_size);
			return cluster->runBatch(mode, packet);
		});
	};
}

} // namespace

TEST_CASE("benchmark_connection_operations")
{
	const auto backends = getConnectionBenchmarkBackends();
	const auto &options = get_connection_benchmark_options();
	REQUIRE(!backends.empty());

	for (const auto &backend : backends) {
		SECTION(backend.name)
		{
			if (options.mode == ConnectionBenchmarkMode::ClientServer &&
					!backend.skip_reason.empty()) {
				Catch::cerr() << "\nSkipping " << backend.name
							  << " connection benchmark: " << backend.skip_reason
							  << '\n';
				return;
			}

			for (const size_t client_count : g_connection_client_counts) {
				SECTION(std::to_string(client_count) + " clients")
				{
					for (const size_t payload_size : g_connection_payload_sizes) {
						SECTION(formatPayloadSize(payload_size))
						{
							if (options.mode == ConnectionBenchmarkMode::ServerOnly) {
								ConnectionBenchmarkExternalServer server(
										backend, client_count);
								server.run();
								return;
							}

							benchmarkConnectionTraffic(backend, client_count, payload_size,
									TrafficMode::ClientToServer);
							benchmarkConnectionTraffic(backend, client_count, payload_size,
									TrafficMode::ServerToClient);
							benchmarkConnectionTraffic(
									backend, client_count, payload_size, TrafficMode::Both);
						}
					}
				}
			}
		}
	}
}
