// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "network/multi/connection.h"
#include "network/networkpacket.h"
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <vector>

class TestFmMulti : public TestBase
{
	using Multi = con::ConnectionMulti;
	using Protocol = Multi::proto_name;
	class Backend : public con::IConnection
	{
	public:
		explicit Backend(session_t id) : id(id) {}
		void Serve(Address) override {}
		void Connect(Address) override {}
		bool Connected() override { return true; }
		void Disconnect() override {}
		void DisconnectPeer(session_t) override {}
		void Send(session_t, u8, NetworkPacket *, bool) override {}
		Address GetPeerAddress(session_t) override { return {}; }
		float getPeerStat(session_t, con::rtt_stat_type) override { return 0; }
		float getLocalStat(con::rate_stat_type) override { return 0; }
		bool ReceiveTimeoutMs(NetworkPacket *packet, u32 timeout) override
		{
			timeouts.push_back(timeout);
			if (on_poll) {
				auto callback = std::move(on_poll);
				callback();
			}
			std::lock_guard lock(mutex);
			if (!queued)
				return false;
			--queued;
			const u8 data[] = {0, 5, 42};
			packet->putRawPacket(data, sizeof(data), id);
			return true;
		}
		void enqueue(unsigned count = 1)
		{
			{
				std::lock_guard lock(mutex);
				queued += count;
			}
			notifyReceive();
		}
		session_t id;
		std::vector<u32> timeouts;
		std::function<void()> on_poll;

	private:
		std::mutex mutex;
		unsigned queued = 0;
	};

	std::shared_ptr<Backend> add(Multi &multi, Protocol protocol, session_t id)
	{
		auto backend = std::make_shared<Backend>(id);
		backend->setReceiveSignal(multi.m_receive_signal);
		multi.m_connections[static_cast<size_t>(protocol)] = backend;
		return backend;
	}

public:
	TestFmMulti() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmMulti"; }
	void runTests(IGameDef *) override
	{
		TEST(testLazyConstruction);
		TEST(testClientDelegation);
		TEST(testPendingAfterDisconnect);
		TEST(testWakeFromAnotherBackend);
		TEST(testFairness);
		TEST(testTimeout);
		TEST(testCoalescing);
	}

	void testLazyConstruction()
	{
		Multi multi(1350, 5.0f, true);
		for (const auto &backend : multi.m_connections)
			UASSERT(!backend);
		UASSERT(!multi.Connected());
		NetworkPacket packet;
		UASSERT(!multi.ReceiveTimeoutMs(&packet, 0));
	}

	void testClientDelegation()
	{
		Multi multi(1350, 5.0f, true);
		auto idle = add(multi, Protocol::sctp, 30002);
		auto selected = add(multi, Protocol::enet, 20002);
		multi.connected_to = Protocol::enet;
		selected->enqueue();
		NetworkPacket packet;
		UASSERT(multi.ReceiveTimeoutMs(&packet, 37));
		UASSERT(packet.getPeerId() == selected->id);
		UASSERT(selected->timeouts == std::vector<u32>{37});
		UASSERT(idle->timeouts.empty());
	}

	void testPendingAfterDisconnect()
	{
		Multi multi(1350, 5.0f, true);
		auto backend = add(multi, Protocol::enet, 20002);
		multi.connected_to = Protocol::enet;
		backend->enqueue();
		multi.Disconnect();
		NetworkPacket packet;
		UASSERT(multi.ReceiveTimeoutMs(&packet, 0));
		UASSERT(packet.getPeerId() == backend->id);
	}

	void testWakeFromAnotherBackend()
	{
		Multi multi(1350, 5.0f, true);
		multi.m_serving = true;
		auto idle = add(multi, Protocol::sctp, 30002);
		auto ready = add(multi, Protocol::enet, 20002);
		std::promise<void> scanned;
		auto scanning = scanned.get_future();
		ready->on_poll = [&] { scanned.set_value(); };
		auto receiver = std::async(std::launch::async, [&] {
			NetworkPacket packet;
			return multi.ReceiveTimeoutMs(&packet, 1000) &&
				   packet.getPeerId() == ready->id;
		});
		scanning.wait();
		ready->enqueue();
		UASSERT(receiver.get());
		for (u32 timeout : idle->timeouts)
			UASSERT(timeout == 0);
		for (u32 timeout : ready->timeouts)
			UASSERT(timeout == 0);
	}

	void testFairness()
	{
		Multi multi(1350, 5.0f, true);
		multi.m_serving = true;
		auto first = add(multi, Protocol::sctp, 30002);
		auto second = add(multi, Protocol::enet, 20002);
		first->enqueue(2);
		second->enqueue();
		NetworkPacket packet;
		for (session_t expected : {first->id, second->id, first->id}) {
			UASSERT(multi.ReceiveTimeoutMs(&packet, 0));
			UASSERT(packet.getPeerId() == expected);
		}
		UASSERT(!multi.ReceiveTimeoutMs(&packet, 0));
	}

	void testTimeout()
	{
		Multi multi(1350, 5.0f, true);
		multi.m_serving = true;
		auto idle = add(multi, Protocol::enet, 20002);
		NetworkPacket packet;
		const auto start = std::chrono::steady_clock::now();
		UASSERT(!multi.ReceiveTimeoutMs(&packet, 15));
		UASSERT(std::chrono::steady_clock::now() - start >=
				std::chrono::milliseconds(10));
		for (u32 timeout : idle->timeouts)
			UASSERT(timeout == 0);
	}

	void testCoalescing()
	{
		con::ReceiveSignal signal;
		for (unsigned i = 0; i < 100; ++i)
			signal.notify();
		UASSERT(signal.wait(0));
		UASSERT(!signal.wait(0));
		signal.notify();
		UASSERT(signal.wait(0));
	}
};
static TestFmMulti g_test_instance;
