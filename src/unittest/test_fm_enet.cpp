// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "network/enet/fm_enet_packet.h"
#include "network/enet/fm_enet_wakeup.h"
#include <cstring>
#include <future>

class TestFmEnet : public TestBase
{
public:
	TestFmEnet() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmEnet"; }
	void runTests(IGameDef *) override
	{
		TEST(testPacketLifetime);
		TEST(testWakeBeforeWait);
		TEST(testWakeDuringWait);
		TEST(testNetworkWake);
	}

	void testPacketLifetime()
	{
		const u8 payload[] = {0x7f, 0xfe, 0, 1, 2, 3, 255};
		for (const bool reliable : {false, true}) {
			auto command = con::ConnectionCommand::send(PEER_ID_SERVER, 0,
					SharedBuffer<u8>(payload, sizeof(payload)), reliable);
			std::weak_ptr<con::ConnectionCommand> weak = command;
			std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet(
					con::makeEnetPacket(command), enet_packet_destroy);
			UASSERT(packet);
			UASSERT(packet->data == *command->data);
			UASSERT(packet->flags & ENET_PACKET_FLAG_NO_ALLOCATE);
			UASSERT(bool(packet->flags & ENET_PACKET_FLAG_RELIABLE) == reliable);
			command.reset();
			UASSERT(!weak.expired());
			UASSERT(packet->dataLength == sizeof(payload));
			UASSERT(std::memcmp(packet->data, payload, sizeof(payload)) == 0);
			packet.reset();
			UASSERT(weak.expired());
		}
	}

	void testWakeBeforeWait()
	{
		con::EnetWakeup wakeup;
		wakeup.notify();
		UASSERT(wakeup.wait(ENET_SOCKET_NULL, 1000));
		wakeup.drain();
		UASSERT(!wakeup.wait(ENET_SOCKET_NULL, 0));
	}

	void testWakeDuringWait()
	{
		con::EnetWakeup wakeup;
		std::promise<void> started;
		auto ready = started.get_future();
		auto waiter = std::async(std::launch::async, [&] {
			started.set_value();
			return wakeup.wait(ENET_SOCKET_NULL, 1000);
		});
		ready.wait();
		wakeup.notify();
		UASSERT(waiter.get());
	}

	void testNetworkWake()
	{
		con::EnetWakeup wakeup;
		ENetAddress address{};
#if defined(ENET_IPV6)
		UASSERT(enet_address_set_host_ip(&address, "::1") == 0);
#else
		UASSERT(enet_address_set_host_ip(&address, "127.0.0.1") == 0);
#endif
		std::unique_ptr<ENetHost, decltype(&enet_host_destroy)> host(
				enet_host_create(&address, 1, 1, 0, 0), enet_host_destroy);
		UASSERT(host);
		UASSERT(enet_socket_get_address(host->socket, &address) == 0);
		char byte = 0;
		ENetBuffer buffer{};
		buffer.data = &byte;
		buffer.dataLength = sizeof(byte);
		UASSERT(enet_socket_send(host->socket, &address, &buffer, 1) == 1);
		// No command notification: only network readability can wake this wait.
		UASSERT(wakeup.wait(host->socket, 1000));
	}
};
static TestFmEnet g_test_instance;
