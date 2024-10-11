#include "fm_far_container.h"
#include "client.h"
#include "client/clientmap.h"
#include "client/fm_far_calc.h"
#include "mapblock.h"
#include "mapgen/mapgen.h"
#include "mapnode.h"

FarContainer::FarContainer(Client *client) : m_client{client}
{
}

const MapNode &FarContainer::getNodeRefUnsafe(const v3pos_t &pos)
{
	auto bpos = getNodeBlockPos(pos);

	int fmesh_step = getFarStep(m_client->getEnv().getClientMap().getControl(),
			getNodeBlockPos(m_client->getEnv().getClientMap().m_far_blocks_last_cam_pos),
			bpos);

	const auto &shift = fmesh_step; // + cell_size_pow;
	v3bpos_t bpos_aligned((bpos.X >> shift) << shift, (bpos.Y >> shift) << shift,
			(bpos.Z >> shift) << shift);

	const auto &storage =
			m_client->getEnv().getClientMap().far_blocks_storage[fmesh_step];
	if (const auto &it = storage.find(bpos_aligned); it != storage.end()) {
		const auto &block = it->second;
		v3pos_t relpos = pos - bpos_aligned * MAP_BLOCKSIZE;

		const auto &relpos_shift = fmesh_step; // + 1;
		auto relpos_shifted = v3pos_t(relpos.X >> relpos_shift, relpos.Y >> relpos_shift,
				relpos.Z >> relpos_shift);
		const auto &n = block->getNodeNoLock(relpos_shifted);
		if (n.getContent() != CONTENT_IGNORE) {
			return n;
		}
	}

	const auto &v = m_mg->visible_content(pos);
	if (v.getContent()) {
		return v;
	}
	return m_mg->visible_transparent;
};
