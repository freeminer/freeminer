// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "nodedef.h"
#include "network/networkpacket.h"

class TestFmNodeDef : public TestBase
{
public:
	TestFmNodeDef() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmNodeDef"; }

	void runTests(IGameDef *) override
	{
		TEST(testSkipsReservedNodeId);
		TEST(testNodeChangeMessagePack);
	}

	void testSkipsReservedNodeId();
	void testNodeChangeMessagePack();
};

static TestFmNodeDef g_test_instance;

void TestFmNodeDef::testSkipsReservedNodeId()
{
	NodeDefManager manager;
	const std::string name = "fm_test:reserved_id";
	const content_t id = manager.allocateDummy(name);

	UASSERT(id != CONTENT_IGNORE);
	UASSERTEQ(content_t, manager.getId(name), id);
	UASSERTEQ(std::string, manager.get(id).name, name);
}

void TestFmNodeDef::testNodeChangeMessagePack()
{
	const v3pos_t pos(31, -47, 113);
	const MapNode node(42, 8, 9);
	{
		MSGPACK_PACKET_INIT((int)TOCLIENT_REMOVENODE, 1);
		PACK(TOCLIENT_REMOVENODE_POS, pos);
		NetworkPacket packet;
		packet.putRawPacket(
				reinterpret_cast<const u8 *>(buffer.data()), buffer.size(), 1);
		UASSERT(packet.packet_unpack());
		UASSERTEQ(int, packet.getCommand(), TOCLIENT_REMOVENODE);
		UASSERT(packet.packet->at(TOCLIENT_REMOVENODE_POS).as<v3pos_t>() == pos);
	}

	{
		MSGPACK_PACKET_INIT((int)TOCLIENT_ADDNODE, 3);
		PACK(TOCLIENT_ADDNODE_POS, pos);
		PACK(TOCLIENT_ADDNODE_NODE, node);
		PACK(TOCLIENT_ADDNODE_REMOVE_METADATA, true);
		NetworkPacket packet;
		packet.putRawPacket(
				reinterpret_cast<const u8 *>(buffer.data()), buffer.size(), 1);
		UASSERT(packet.packet_unpack());
		UASSERTEQ(int, packet.getCommand(), TOCLIENT_ADDNODE);
		UASSERT(packet.packet->at(TOCLIENT_ADDNODE_POS).as<v3pos_t>() == pos);
		const MapNode decoded = packet.packet->at(TOCLIENT_ADDNODE_NODE).as<MapNode>();
		UASSERT(decoded.param0 == node.param0);
		UASSERT(decoded.param1 == node.param1);
		UASSERT(decoded.param2 == node.param2);
		UASSERT(packet.packet->at(TOCLIENT_ADDNODE_REMOVE_METADATA).as<bool>());
	}
}
