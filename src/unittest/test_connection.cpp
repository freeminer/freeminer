/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "test.h"

#include "log.h"
#include "socket.h"
#include "settings.h"
#include "util/serialize.h"
#include "network/connection.h"

#include "config.h"

class TestConnection : public TestBase {
public:
	TestConnection()
	{
		if (INTERNET_SIMULATOR == false)
			TestManager::registerTestModule(this);
	}

	const char *getName() { return "TestConnection"; }

	void runTests(IGameDef *gamedef);

	void testHelpers();
	void testConnectSendReceive();
};

static TestConnection g_test_instance;

void TestConnection::runTests(IGameDef *gamedef)
{
#if MINETEST_PROTO
	TEST(testHelpers);
	TEST(testConnectSendReceive);
#endif
}

#if MINETEST_PROTO

////////////////////////////////////////////////////////////////////////////////

struct Handler : public con::PeerHandler
{
	Handler(const char *a_name)
	{
		count = 0;
		last_id = 0;
		name = a_name;
	}

	void peerAdded(u16 peer_id)
	{
		infostream << "Handler(" << name << ")::peerAdded(): "
			"id=" << peer_id << std::endl;
		last_id = peer_id;
		count++;
	}

	void deletingPeer(u16 peer_id, bool timeout)
	{
		infostream << "Handler(" << name << ")::deletingPeer(): "
			"id=" << peer_id << ", timeout=" << timeout << std::endl;
		last_id = peer_id;
		count--;
	}

	s32 count;
	u16 last_id;
	const char *name;
};

void TestConnection::testHelpers()
{
	// Some constants for testing
	u32 proto_id = 0x12345678;
	u16 peer_id = 123;
	u8 channel = 2;
	SharedBuffer<u8> data1(1);
	data1[0] = 100;
	Address a(127,0,0,1, 10);
	const u16 seqnum = 34352;

	con::BufferedPacket p1 = con::makePacket(a, data1,
			proto_id, peer_id, channel);
	/*
		We should now have a packet with this data:
		Header:
			[0] u32 protocol_id
			[4] u16 sender_peer_id
			[6] u8 channel
		Data:
			[7] u8 data1[0]
	*/
	UASSERT(readU32(&p1.data[0]) == proto_id);
	UASSERT(readU16(&p1.data[4]) == peer_id);
	UASSERT(readU8(&p1.data[6]) == channel);
	UASSERT(readU8(&p1.data[7]) == data1[0]);

	//infostream<<"initial data1[0]="<<((u32)data1[0]&0xff)<<std::endl;

	SharedBuffer<u8> p2 = con::makeReliablePacket(data1, seqnum);

	/*infostream<<"p2.getSize()="<<p2.getSize()<<", data1.getSize()="
			<<data1.getSize()<<std::endl;
	infostream<<"readU8(&p2[3])="<<readU8(&p2[3])
			<<" p2[3]="<<((u32)p2[3]&0xff)<<std::endl;
	infostream<<"data1[0]="<<((u32)data1[0]&0xff)<<std::endl;*/

	UASSERT(p2.getSize() == 3 + data1.getSize());
	UASSERT(readU8(&p2[0]) == TYPE_RELIABLE);
	UASSERT(readU16(&p2[1]) == seqnum);
	UASSERT(readU8(&p2[3]) == data1[0]);
}


void TestConnection::testConnectSendReceive()
{
	DSTACK("TestConnection::Run");

	/*
		Test some real connections

		NOTE: This mostly tests the legacy interface.
	*/

	u32 proto_id = 0xad26846a;

	Handler hand_server("server");
	Handler hand_client("client");

	Address address(0, 0, 0, 0, 30001);
	Address bind_addr(0, 0, 0, 0, 30001);
	/*
	 * Try to use the bind_address for servers with no localhost address
	 * For example: FreeBSD jails
	 */
	std::string bind_str = g_settings->get("bind_address");
	try {
		bind_addr.Resolve(bind_str.c_str());

		if (!bind_addr.isIPv6()) {
			address = bind_addr;
		}
	} catch (ResolveError &e) {
	}

	infostream << "** Creating server Connection" << std::endl;
	con::Connection server(proto_id, 512, 5.0, false, &hand_server);
	server.Serve(address);

	infostream << "** Creating client Connection" << std::endl;
	con::Connection client(proto_id, 512, 5.0, false, &hand_client);

	UASSERT(hand_server.count == 0);
	UASSERT(hand_client.count == 0);

	sleep_ms(50);

	Address server_address(127, 0, 0, 1, 30001);
	if (address != Address(0, 0, 0, 0, 30001)) {
		server_address = bind_addr;
	}

	infostream << "** running client.Connect()" << std::endl;
	client.Connect(server_address);

	sleep_ms(50);

	// Client should not have added client yet
	UASSERT(hand_client.count == 0);

	try {
		NetworkPacket pkt;
		infostream << "** running client.Receive()" << std::endl;
		client.Receive(&pkt);
		infostream << "** Client received: peer_id=" << pkt.getPeerId()
			<< ", size=" << pkt.getSize() << std::endl;
	} catch (con::NoIncomingDataException &e) {
	}

	// Client should have added server now
	UASSERT(hand_client.count == 1);
	UASSERT(hand_client.last_id == 1);
	// Server should not have added client yet
	UASSERT(hand_server.count == 0);

	sleep_ms(100);

	try {
		NetworkPacket pkt;
		infostream << "** running server.Receive()" << std::endl;
		server.Receive(&pkt);
		infostream << "** Server received: peer_id=" << pkt.getPeerId()
				<< ", size=" << pkt.getSize()
				<< std::endl;
	} catch (con::NoIncomingDataException &e) {
		// No actual data received, but the client has
		// probably been connected
	}

	// Client should be the same
	UASSERT(hand_client.count == 1);
	UASSERT(hand_client.last_id == 1);
	// Server should have the client
	UASSERT(hand_server.count == 1);
	UASSERT(hand_server.last_id == 2);

	//sleep_ms(50);

	while (client.Connected() == false) {
		try {
			NetworkPacket pkt;
			infostream << "** running client.Receive()" << std::endl;
			client.Receive(&pkt);
			infostream << "** Client received: peer_id=" << pkt.getPeerId()
				<< ", size=" << pkt.getSize() << std::endl;
		} catch (con::NoIncomingDataException &e) {
		}
		sleep_ms(50);
	}

	sleep_ms(50);

	try {
		NetworkPacket pkt;
		infostream << "** running server.Receive()" << std::endl;
		server.Receive(&pkt);
		infostream << "** Server received: peer_id=" << pkt.getPeerId()
				<< ", size=" << pkt.getSize()
				<< std::endl;
	} catch (con::NoIncomingDataException &e) {
	}

	/*
		Simple send-receive test
	*/
	{
		NetworkPacket pkt;
		pkt.putRawPacket((u8*) "Hello World !", 14, 0);

		Buffer<u8> sentdata = pkt.oldForgePacket();

		infostream<<"** running client.Send()"<<std::endl;
		client.Send(PEER_ID_SERVER, 0, &pkt, true);

		sleep_ms(50);

		NetworkPacket recvpacket;
		infostream << "** running server.Receive()" << std::endl;
		server.Receive(&recvpacket);
		infostream << "** Server received: peer_id=" << pkt.getPeerId()
				<< ", size=" << pkt.getSize()
				<< ", data=" << (const char*)pkt.getU8Ptr(0)
				<< std::endl;

		Buffer<u8> recvdata = pkt.oldForgePacket();

		UASSERT(memcmp(*sentdata, *recvdata, recvdata.getSize()) == 0);
	}

	u16 peer_id_client = 2;
	/*
		Send a large packet
	*/
	{
		const int datasize = 30000;
		NetworkPacket pkt(0, datasize);
		for (u16 i=0; i<datasize; i++) {
			pkt << (u8) i/4;
		}

		infostream << "Sending data (size=" << datasize << "):";
		for (int i = 0; i < datasize && i < 20; i++) {
			if (i % 2 == 0)
				infostream << " ";
			char buf[10];
			snprintf(buf, 10, "%.2X",
				((int)((const char *)pkt.getU8Ptr(0))[i]) & 0xff);
			infostream<<buf;
		}
		if (datasize > 20)
			infostream << "...";
		infostream << std::endl;

		Buffer<u8> sentdata = pkt.oldForgePacket();

		server.Send(peer_id_client, 0, &pkt, true);

		//sleep_ms(3000);

		Buffer<u8> recvdata;
		infostream << "** running client.Receive()" << std::endl;
		u16 peer_id = 132;
		u16 size = 0;
		bool received = false;
		u32 timems0 = porting::getTimeMs();
		for (;;) {
			if (porting::getTimeMs() - timems0 > 5000 || received)
				break;
			try {
				NetworkPacket pkt;
				client.Receive(&pkt);
				size = pkt.getSize();
				peer_id = pkt.getPeerId();
				recvdata = pkt.oldForgePacket();
				received = true;
			} catch (con::NoIncomingDataException &e) {
			}
			sleep_ms(10);
		}
		UASSERT(received);
		infostream << "** Client received: peer_id=" << peer_id
			<< ", size=" << size << std::endl;

		infostream << "Received data (size=" << size << "): ";
		for (int i = 0; i < size && i < 20; i++) {
			if (i % 2 == 0)
				infostream << " ";
			char buf[10];
			snprintf(buf, 10, "%.2X", ((int)(recvdata[i])) & 0xff);
			infostream << buf;
		}
		if (size > 20)
			infostream << "...";
		infostream << std::endl;

		UASSERT(memcmp(*sentdata, *recvdata, recvdata.getSize()) == 0);
		UASSERT(peer_id == PEER_ID_SERVER);
	}

	// Check peer handlers
	UASSERT(hand_client.count == 1);
	UASSERT(hand_client.last_id == 1);
	UASSERT(hand_server.count == 1);
	UASSERT(hand_server.last_id == 2);
}

#endif
