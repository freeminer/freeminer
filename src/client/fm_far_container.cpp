#include "fm_far_container.h"
#include <unordered_map>
#include "client.h"
#include "client/clientmap.h"
#include "client/fm_far_calc.h"
#include "database/database.h"
#include "irr_v3d.h"
#include "mapblock.h"
#include "mapgen/mapgen.h"
#include "mapnode.h"
#include "server.h"

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
	const auto step = getFarStep(m_client->getEnv().getClientMap().getControl(),
			getNodeBlockPos(m_client->getEnv().getClientMap().far_blocks_last_cam_pos),
			bpos);
	const auto &shift = step; // + cell_size_pow;
	const v3bpos_t bpos_aligned((bpos.X >> shift) << shift, (bpos.Y >> shift) << shift,
			(bpos.Z >> shift) << shift);

	MapBlockP block;

	if (block_cache && bpos_aligned == block_cache_p) {
		block = block_cache;
	}

	if (!block && step < FARMESH_STEP_MAX) {
		const auto &storage = m_client->getEnv().getClientMap().far_blocks_storage[step];

		block = storage.get(bpos_aligned);
	}

	const auto loadBlock = [this](const auto &bpos, const auto step) -> MapBlockP {
		auto *dbase =
				GetFarDatabase({}, m_client->far_dbases, m_client->m_world_path, step);
		if (!dbase) {
			return {};
		}
		MapBlockP block{m_client->getEnv().getClientMap().createBlankBlockNoInsert(bpos)};

		std::string blob;
		dbase->loadBlock(bpos, &blob);
		if (!blob.length()) {
			return {};
		}

		std::istringstream is(blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char *)&version, 1);

		if (is.fail()) {
			return {};
		}

		// Read basic data
		if (!block->deSerialize(is, version, true)) {
			return {};
		}
		return block;
	};

	if (!block && !m_client->m_simple_singleplayer_mode) {
		thread_local static std::unordered_set<v3bpos_t> miss_cache;
		if (!miss_cache.contains(bpos)) {
			block = loadBlock(bpos, step);
			if (!block) {
				miss_cache.emplace(bpos);
			}
		}
	}

	if (block) {
		v3pos_t relpos = pos - bpos_aligned * MAP_BLOCKSIZE;

		const auto &relpos_shift = step; // + 1;
		const auto relpos_shifted = v3pos_t(relpos.X >> relpos_shift,
				relpos.Y >> relpos_shift, relpos.Z >> relpos_shift);
		const auto &n = block->getNodeNoLock(relpos_shifted);
		if (n.getContent() != CONTENT_IGNORE) {
			return n;
		}

		block_cache_p = bpos_aligned;
		block_cache = block;
	}

	if (const auto &v = m_mg->visible_content(pos, use_weather); v.getContent()) {
		return v;
	}

	return m_mg->visible_transparent;
};
