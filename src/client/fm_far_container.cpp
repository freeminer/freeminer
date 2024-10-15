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

namespace
{
thread_local MapBlockP block_cache{};
thread_local v3bpos_t block_cache_p;
}

const MapNode &FarContainer::getNodeRefUnsafe(const v3pos_t &pos)
{
	const auto bpos = getNodeBlockPos(pos);
	const auto fmesh_step = getFarStep(m_client->getEnv().getClientMap().getControl(),
			getNodeBlockPos(m_client->getEnv().getClientMap().far_blocks_last_cam_pos),
			bpos);
	const auto &shift = fmesh_step; // + cell_size_pow;
	const v3bpos_t bpos_aligned((bpos.X >> shift) << shift, (bpos.Y >> shift) << shift,
			(bpos.Z >> shift) << shift);

	MapBlockP block;

	if (block_cache && bpos_aligned == block_cache_p) {
		block = block_cache;
	}

	if (!block && fmesh_step < FARMESH_STEP_MAX) {
		const auto &storage =
				m_client->getEnv().getClientMap().far_blocks_storage[fmesh_step];

		block = storage.get(bpos_aligned);
	}

	if (block) {
		v3pos_t relpos = pos - bpos_aligned * MAP_BLOCKSIZE;

		const auto &relpos_shift = fmesh_step; // + 1;
		const auto relpos_shifted = v3pos_t(relpos.X >> relpos_shift,
				relpos.Y >> relpos_shift, relpos.Z >> relpos_shift);
		const auto &n = block->getNodeNoLock(relpos_shifted);
		if (n.getContent() != CONTENT_IGNORE) {
			return n;
		}

		block_cache_p = bpos_aligned;
		block_cache = block;
	}

	if (const auto &v = m_mg->visible_content(pos); v.getContent()) {
		return v;
	}

	return m_mg->visible_transparent;
};
