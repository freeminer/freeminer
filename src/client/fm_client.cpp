#include "client.h"
#include "clientmap.h"
#include "map.h"
#include "network/networkpacket.h"
#include "util/directiontables.h"

void Client::updateMeshTimestampWithEdge(const v3bpos_t &blockpos)
{
	for (const auto &dir : g_7dirs) {
		auto *block = m_env.getMap().getBlockNoCreateNoEx(blockpos + dir);
		if (!block)
			continue;
		block->setTimestampNoChangedFlag(m_uptime);
	}

	/*int to = FARMESH_STEP_MAX;
	for (int step = 1; step <= to; ++step) {
		v3pos_t actualpos = getFarmeshActual(blockpos, step);
		auto *block = m_env.getMap().getBlockNoCreateNoEx(actualpos); // todo maybe update bp1 too if differ
		if(!block)
			continue;
		block->setTimestampNoChangedFlag(m_uptime);
	}*/
}

void Client::sendGetBlocks()
{
	NetworkPacket pkt(TOSERVER_GET_BLOCKS, 1);
	MSGPACK_PACKET_INIT((int)TOSERVER_GET_BLOCKS, 1);

	const auto &far_blocks = *m_env.getClientMap().m_far_blocks_use;
	PACK(TOSERVER_GET_BLOCKS_BLOCKS,
			static_cast<std::remove_reference_t<decltype(far_blocks)>::full_type>(
					far_blocks));
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}