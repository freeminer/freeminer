/*
Copyright (C) 2022 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "fm_far_calc.h"
#include "constants.h"
#include "server/clientiface.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "map.h"
#include "mapblock.h"
#include "profiler.h"
#include "remoteplayer.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "server.h"
#include "emerge.h"
#include "face_position_cache.h"
#include "servermap.h"
#include "threading/lock.h"
#include "util/directiontables.h"
#include "util/numeric.h"
#include "util/unordered_map_hash.h"

#include <algorithm>
#include <cmath>
#include <limits>

struct BlockSendPrediction
{
	// Blocks intersecting the movement line, starting at the player's block.
	std::vector<v3bpos_t> path;
	pos_t max_distance = 0;

	std::vector<v3bpos_t> getLayer(pos_t layer) const;
};

BlockSendPrediction predictBlockSendCorridor(
		const v3opos_t &playerpos, const v3opos_t &velocity, pos_t max_distance);

std::vector<v3bpos_t> BlockSendPrediction::getLayer(pos_t layer) const
{
	if (layer == 0)
		return path;
	std::vector<v3bpos_t> result;
	std::unordered_set<v3bpos_t, v3bposHash> visited;
	const auto neighbors = FacePositionCache::getFacePositions(layer);
	for (const auto &center : path) {
		for (const auto &neighbor : neighbors) {
			const auto p = center + neighbor;
			if (radius_box(p, v3bpos_t()) > unsigned(max_distance) ||
					!visited.insert(p).second)
				continue;
			// Overlapping shells must not reintroduce a block from a closer layer,
			// even when the caller resumes scanning after skipping earlier layers.
			if (std::any_of(path.begin(), path.end(), [&](const v3bpos_t &q) {
					return radius_box(p, q) < unsigned(layer);
				}))
				continue;
			result.push_back(p);
		}
	}
	return result;
}

BlockSendPrediction predictBlockSendCorridor(
		const v3opos_t &playerpos, const v3f &velocity, pos_t max_distance)
{
	BlockSendPrediction result;
	result.max_distance = std::max<pos_t>(0, max_distance);
	result.path.emplace_back(0, 0, 0);
	if (max_distance <= 0)
		return result;

	constexpr opos_t block_size = MAP_BLOCKSIZE * BS;
	const auto center = getNodeBlockPos(floatToInt(playerpos, BS));
	const auto speed = velocity.getLength();
	if (!(speed > 0) || !std::isfinite(speed))
		return result;
	// Two seconds for emergence, transfer and mesh preparation, with bounded work.
	const auto distance =
			std::min<opos_t>({speed * 2 / block_size, 15, opos_t(max_distance)});
	const v3f movement = velocity / speed * (block_size * distance);
	const auto end =
			getNodeBlockPos(floatToInt(playerpos + v3fToOpos(movement), BS)) - center;

	// Traverse block boundaries in time order. Work relative to the current block
	// to retain precision at large world coordinates. Node centers lie on integers,
	// so the lower face of a block is half a node below its first node's center.
	const auto local = playerpos - intToFloat(center * MAP_BLOCKSIZE, BS);
	v3opos_t next;
	v3opos_t increment;
	v3bpos_t step;
	for (unsigned axis = 0; axis < 3; ++axis) {
		if (movement[axis] == 0) {
			next[axis] = increment[axis] = std::numeric_limits<opos_t>::infinity();
			continue;
		}
		step[axis] = movement[axis] > 0 ? 1 : -1;
		const opos_t boundary = step[axis] > 0 ? block_size - BS / 2 : -BS / 2;
		next[axis] = (boundary - local[axis]) / movement[axis];
		increment[axis] = block_size / std::abs(movement[axis]);
	}
	v3bpos_t offset;
	while (offset != end) {
		unsigned axis = 0;
		opos_t earliest = std::numeric_limits<opos_t>::infinity();
		for (unsigned i = 0; i < 3; ++i) {
			if (offset[i] != end[i] && next[i] < earliest) {
				axis = i;
				earliest = next[i];
			}
		}
		offset[axis] += step[axis];
		next[axis] += increment[axis];
		if (radius_box(offset, v3bpos_t()) <= unsigned(max_distance))
			result.path.push_back(offset);
	}
	return result;
}

int RemoteClient::GetNextBlocksFm(ServerEnvironment *env, EmergeManager *emerge,
		float dtime, std::vector<PrioritySortedBlockTransfer> &dest, double m_uptime,
		u64 max_ms)
{
	const auto lock = try_lock_unique_rec();
	if (!lock->owns_lock())
		return 0;

	auto end_ms = porting::getTimeMs() + max_ms;

	// Increment timers
	m_nothing_to_send_pause_timer -= dtime;
	m_nearest_unsent_reset_timer += dtime;
	m_time_from_building += dtime;

	/*
	if (m_nearest_unsent_reset) {
		m_nearest_unsent_reset = 0;
		m_nearest_unsent_reset_timer = 999;
		m_nothing_to_send_pause_timer = 0;
		m_nearest_unsent_d = 0;
	}
*/
	RemotePlayer *player = env->getPlayer(peer_id);
	// This can happen sometimes; clients and players are not in perfect sync.
	if (player == NULL)
		return 0;

	PlayerSAO *sao = player->getPlayerSAO();
	if (sao == NULL)
		return 0;

	/*
		// Won't send anything if already sending
		if(m_blocks_sending.size() >= g_settings->getU16
				("max_simultaneous_block_sends_per_client"))
		{
			//infostream<<"Not sending any blocks, Queue full."<<std::endl;
			return;
		}
	*/

	auto playerpos = sao->getBasePosition();

	v3f playerspeed = player->getSpeed();
	ServerActiveObject *movement_source = sao;
	while (movement_source->getParent())
		movement_source = movement_source->getParent();
	if (auto *entity = dynamic_cast<LuaEntitySAO *>(movement_source))
		playerspeed = entity->getVelocity();
	const auto reported_speed = playerspeed.getLength();
	if (!std::isfinite(reported_speed))
		playerspeed = v3f();
	else if (reported_speed > 120.0 * BS)
		playerspeed *= (120.0 * BS / reported_speed);
	const auto speed = playerspeed.getLength();
	const auto speed_in_blocks = speed / (MAP_BLOCKSIZE * BS);
	const bool predict_movement = speed_in_blocks > 0.8;
	v3f playerspeeddir;
	if (speed > 1.0 * BS)
		playerspeeddir = playerspeed / speed;
	// Keep fast-movement scans centered on the player; prefetch follows below.
	v3opos_t playerpos_predicted =
			predict_movement ? playerpos
							 : playerpos + v3fToOpos(playerspeeddir) * MAP_BLOCKSIZE * BS;

	v3pos_t center_nodepos = floatToInt(playerpos_predicted, BS);

	auto center = getNodeBlockPos(center_nodepos);

	// Camera position and direction
	auto camera_pos = sao->getEyePosition();
	v3f camera_dir = v3f(0, 0, 1);
	camera_dir.rotateYZBy(sao->getLookPitch());
	camera_dir.rotateXZBy(sao->getRotation().Y);

	// infostream<<"camera_dir=("<<camera_dir<<")"<< "
	// camera_pos="<<camera_pos<<std::endl;

	/*
		Get the starting value of the block finder radius.
	*/

	if (m_last_center != center ||
			m_last_direction.getDistanceFrom(playerspeeddir) > 0.4) {
		m_last_center = center;
		m_last_direction = playerspeeddir;
		m_nearest_unsent_reset_timer = 999;
		m_nothing_to_send_pause_timer = -1;
	}

	/*
	if (m_last_direction.getDistanceFrom(camera_dir) > 0.4) { // 1 = 90degm_nothing_to_send_pause_timer
		m_last_direction = camera_dir;
		m_nearest_unsent_reset_timer = 999;
	}
    */

	/*infostream<<"m_nearest_unsent_reset_timer="
			<<m_nearest_unsent_reset_timer<<std::endl;*/

	// Reset periodically to workaround for some bugs or stuff
	if (m_nearest_unsent_reset_timer > 120.0) {
		m_nearest_unsent_reset_timer = 0;
		m_nearest_unsent_d = 0;
		//m_nearest_unsent_reset = 0;
		m_nothing_to_send_pause_timer = -1;
		// infostream<<"Resetting m_nearest_unsent_d for "<<peer_id<<std::endl;
	}

	if (m_nothing_to_send_pause_timer >= 0 && !predict_movement) {
		return 0;
	}

	// s16 last_nearest_unsent_d = m_nearest_unsent_d;
	short d_start = m_nearest_unsent_d; //.load();

	// infostream<<"d_start="<<d_start<<std::endl;

	thread_local static const u16 max_simul_sends_setting =
			g_settings->getU16("max_simultaneous_block_sends_per_client");
	thread_local static const u16 max_simul_sends_usually = max_simul_sends_setting;

	/*
		Check the time from last addNode/removeNode.

		Decrease send rate if player is building stuff.
	*/

#if 0
	thread_local static const auto full_block_send_enable_min_time_from_building =
			g_settings->getFloat("full_block_send_enable_min_time_from_building");
	if (m_time_from_building < full_block_send_enable_min_time_from_building) {
		/*
		max_simul_sends_usually
			= LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS;
		*/
		if (d_start <= 1)
			d_start = 2;
		++m_nearest_unsent_reset_want;
	} else if (m_nearest_unsent_reset_want) {
		m_nearest_unsent_reset_want = 0;
		m_nearest_unsent_reset_timer = 999; // magical number more than ^ other number 120
											// - need to reset d on next iteration
	}
#endif
	/*
		Number of blocks sending + number of blocks selected for sending
	*/
	u32 num_blocks_selected = 0;
	u32 num_blocks_sending = 0;

	/*
		next time d will be continued from the d from which the nearest
		unsent block was found this time.

		This is because not necessarily any of the blocks found this
		time are actually sent.
	*/
	s32 new_nearest_unsent_d = -1;

	// get view range and camera fov from the client
	auto wanted_range = sao->getWantedRange();
	float camera_fov = sao->getFov();
	// if FOV, wanted_range are not available (old client), fall back to old default
	/*
	if (wanted_range <= 0) wanted_range = 140;
	*/
	if (camera_fov <= 0)
		camera_fov = ((fov + 5) * M_PI / 180) * 4. / 3.;

	thread_local static const auto max_block_send_distance =
			g_settings->getS16("max_block_send_distance");
	s16 full_d_max = max_block_send_distance;
	if (wanted_range) {
		s16 wanted_blocks = wanted_range; // /* / MAP_BLOCKSIZE */ + 1;
		if (wanted_blocks < full_d_max)
			full_d_max = wanted_blocks;
	}

	/*
		const s16 full_d_max = MYMIN(g_settings->getS16("max_block_send_distance"),
	   wanted_range); const s16 d_opt =
	   MYMIN(g_settings->getS16("block_send_optimize_distance"), wanted_range);
	*/

	const opos_t d_blocks_in_sight = full_d_max * BS * MAP_BLOCKSIZE;
	// infostream << "Fov from client " << camera_fov << " full_d_max " << full_d_max <<
	// std::endl;

	s16 d_max = full_d_max;
	thread_local static const pos_t d_max_gen_s =
			g_settings->getS16("max_block_generate_distance");
	const pos_t d_max_gen =
			wanted_range > 0 ? std::min<pos_t>(d_max_gen_s, wanted_range) : d_max_gen_s;

	// Don't loop very much at a time
	s16 max_d_increment_at_time = 10;
	if (d_max > d_start + max_d_increment_at_time)
		d_max = d_start + max_d_increment_at_time;
	/*if(d_max_gen > d_start+2)
		d_max_gen = d_start+2;*/

	// infostream<<"Starting from "<<d_start<<std::endl;

	s32 nearest_emerged_d = -1;
	s32 nearest_emergefull_d = -1;
	s32 nearest_sent_d = -1;
	// bool queue_is_full = false;

	BlockSendPrediction prediction;
	if (predict_movement)
		prediction = predictBlockSendCorridor(playerpos, playerspeed, full_d_max);

	int num_blocks_air = 0;
	int blocks_occlusion_culled = 0;
	thread_local static const bool server_occlusion =
			g_settings->getBool("server_occlusion");
	bool occlusion_culling_enabled = server_occlusion;

	auto cam_pos_nodes = floatToInt(playerpos, BS);

	//const auto *nodemgr = env->getGameDef()->getNodeDefManager();

	unordered_map_v3pos<bool> occlude_cache;
	s16 d;
	size_t block_skip_retry = 0;
	s16 first_skipped_d = 0;
	constexpr auto always_first_ds = 1;
	for (d = 0; d <= d_max;
			(d_start > always_first_ds && d == always_first_ds) ? d = d_start : ++d) {
		/*errorstream<<"checking d="<<d<<" for "
				<<server->getPlayerName(peer_id)<<std::endl;*/
		// infostream<<"RemoteClient::SendBlocks(): d="<<d<<" d_start="<<d_start<<"
		// d_max="<<d_max<<" d_max_gen="<<d_max_gen<<std::endl;

		std::vector<v3pos_t> list;
		/*
		if (d > 2 && d == d_start && !m_nearest_unsent_reset_want &&
				m_nearest_unsent_reset_timer !=
						999) { // oops, again magic number from up ^
			list.emplace_back(0, 0, 0);
		}
        */

		const bool can_skip = d > 1;
		if (predict_movement)
			list = prediction.getLayer(d);
		else
			list = FacePositionCache::getFacePositions(d);

		for (auto li = list.begin(); li != list.end(); ++li) {
			// A layer around the path can be longer than an ordinary shell.
			if (predict_movement && porting::getTimeMs() > end_ms)
				goto queue_full_break;
			const auto p = *li + center;
			const pos_t block_distance =
					predict_movement ? radius_box(*li, v3bpos_t()) : d;

			/*
				Send throttling
				- Don't allow too many simultaneous transfers
				- EXCEPT when the blocks are very close

				Also, don't send blocks that are already flying.
			*/

			// Start with the usual maximum
			u16 max_simul_dynamic = max_simul_sends_usually;

			// If block is very close, allow full maximum
			if (d <= BLOCK_ALWAYS_SEND_MAX_D)
				max_simul_dynamic = max_simul_sends_setting;

			// Don't select too many blocks for sending
			if (num_blocks_selected + num_blocks_sending >= max_simul_dynamic) {
				// queue_is_full = true;
				goto queue_full_break;
			}

			/*
				Do not go over-limit
			*/
			if (blockpos_over_max_limit(p)) {
				continue;
			}

			// If this is true, inexistent block will be made from scratch
			bool generate = block_distance <= d_max_gen;

			// infostream<<"d="<<d<<std::endl;

			double block_sent = 0;
			{
				const auto lock = m_blocks_sent.lock_shared_rec();
				if (const auto it = m_blocks_sent.find(p); it != m_blocks_sent.end()) {
					block_sent = it->second;
				}
			}

			/*
				Don't generate or send if not in sight
				FIXME This only works if the client uses a small enough
				FOV setting. The default of 72 degrees is fine.
			*/
			if (block_sent > 0 && can_skip &&
					isBlockInSight(p, camera_pos, camera_dir, camera_fov,
							d_blocks_in_sight) == false) {
				// DUMP(p, can_skip, block_sent, "nosight");
				continue;
			}
			/*
				Don't send already sent blocks
			*/
			{
				auto dspd = d / (speed_in_blocks ? speed_in_blocks : 1);
				if (block_sent > 0 && (/* (block_overflow && d>1) || */ block_sent + 1 +
													  (d <= 2 ? 0 : dspd * dspd) >
											  m_uptime)) {
					continue;
				}
			}

			if (d >= 2 && can_skip && occlusion_culling_enabled) {
				const auto visible = [&](const v3pos_t &p) {
					ScopeProfiler sp(g_profiler, "SMap: Occusion calls");
					auto cpn = p * MAP_BLOCKSIZE;

					cpn += v3pos_t(
							MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2);

					v3pos_t spn = cam_pos_nodes + v3pos_t(0, 0, 0);
					if (env->getMap().isBlockOccluded(p * MAP_BLOCKSIZE, spn)) {
						g_profiler->add("SMap: Occlusion skip", 1);
						++blocks_occlusion_culled;
						return false;
					}
					return true;
				};
				if (!visible(p)) {
					continue;
				}
			}

			/*
				Check if map has this block
			*/

			MapBlockPtr block;
			if (0) {
				const auto lock = env->getMap().m_blocks.try_lock_shared_rec();
				if (!lock->owns_lock()) {
					++block_skip_retry;
					if (!first_skipped_d && d > always_first_ds)
						first_skipped_d = d;
					continue;
				}
				block = env->getMap().getBlock(p);
			}
			block = env->getMap().getBlock(p);

			// bool surely_not_found_on_disk = false;
			// bool block_is_invalid = false;
			if (block) {
				const auto lock = block->lock_shared_rec();
				if (d >= 2 && block->m_is_mono_block &&
						block->data[0].param0 == CONTENT_AIR) {
					uint8_t not_air = 0;
					for (const auto &dir : g_6dirs) {
						if (const auto block_near = env->getMap().getBlock(p + dir)) {
							const auto lock = block_near->lock_shared_rec();

							if (!block_near->isAir()) {
								++not_air;
								break;
							}
						}
					}
					if (!not_air) {
						continue;
					}
				}

				if (block_sent > 0 && block_sent >= block->m_changed_timestamp) {
					// DUMP(p, block_sent, block->m_changed_timestamp,
					// block->getDiskTimestamp(), block->getActualTimestamp(), "ch");
					continue;
				}

				if (!block->isGenerated()) {
					// DUMP(p, block->isGenerated());
					continue;
				}

				if (!block->getLightingComplete()) {
					if (!block->isAir()) {
						env->getServerMap().lighting_modified_add(p, d);
					}
					if (block_sent && can_skip) {
						continue;
					}
				}

				// Reset usage timer, this block will be of use in the future.
				block->resetUsageTimer();

				// if (block->lighting_broken > 0 && (block_sent || can_skip))
				//	continue;

				// Block is valid if lighting is up-to-date and data exists
				/*
								if(block->is isValid() == false)
								{
									block_is_invalid = true;
								}
				*/

				/*
					If block is not close, don't send it unless it is near
					ground level.

					Block is near ground level if night-time mesh
					differs from day-time mesh.
				*/
				/*
								if(d >= d_opt)
								{
									if(block->getDayNightDiff() == false)
										continue;
								}
				*/
			}

			/*
				If block has been marked to not exist on disk (dummy)
				and generating new ones is not wanted, skip block.
			*/
			/*
			if(generate == false && surely_not_found_on_disk == true)
			{
				// get next one.
				continue;
			}
			*/

			/*
				Add inexistent block to emerge queue.
			*/
			if (!block /*|| surely_not_found_on_disk || block_is_invalid*/) {
				// infostream<<"start gen d="<<d<<" p="<<p<<"
				// notfound="<<surely_not_found_on_disk<<" invalid="<< block_is_invalid<<"
				// block="<<block<<" generate="<<generate<<std::endl;
				if (generate || !env->getServerMap().m_db_miss.contains(p)) {

					if (emerge->enqueueBlockEmerge(peer_id, p, generate)) {
						if (nearest_emerged_d == -1)
							nearest_emerged_d = d;
					} else {
						if (nearest_emergefull_d == -1)
							nearest_emergefull_d = d;
						goto queue_full_break;
					}
				} else {
					// infostream << "skip tryload " << p << "\n";
				}

				// DUMP(p, nearest_emerged_d,nearest_emergefull_d, "go generate");

				// get next one.
				continue;
			}

			if (nearest_sent_d == -1 && d >= d_start)
				nearest_sent_d = d;

			/*
				Add block to send queue
			*/

			// Preserve line-first, then layer-by-layer order in the global send queue.
			const float priority =
					predict_movement
							? float(d) + float(li - list.begin()) / (list.size() + 1)
							: float(d);
			PrioritySortedBlockTransfer q(priority, p, peer_id);

			dest.push_back(q);

			if (block->m_is_mono_block && block->data[0].param0 == CONTENT_AIR)
				++num_blocks_air;
			else
				num_blocks_selected += 1;
		}

		// Finish emerging the movement line before spending work on its surroundings.
		if (predict_movement && d == 0 && nearest_emerged_d == 0)
			goto queue_full_break;

		if (porting::getTimeMs() > end_ms) {
			break;
		}
	}
queue_full_break:

	// infostream<<"Stopped at "<<d<<" d_start="<<d_start<< " d_max="<<d_max<<"
	// nearest_emerged_d="<<nearest_emerged_d<<"
	// nearest_emergefull_d="<<nearest_emergefull_d<< "
	// new_nearest_unsent_d="<<new_nearest_unsent_d<< " sel="<<num_blocks_selected<<
	// "+"<<num_blocks_sending << " air="<<num_blocks_air<< " culled=" <<
	// blocks_occlusion_culled <<" cEN="<<occlusion_culling_enabled<<std::endl;
	num_blocks_selected += num_blocks_sending;
	if (block_skip_retry) {
		if (first_skipped_d) {
			m_nearest_unsent_d = first_skipped_d;
		}
		if (d >= d_max) {
			m_nothing_to_send_pause_timer = 1;
		}
	} else {
		if (!num_blocks_selected && !num_blocks_air && d_start <= d) {
			// new_nearest_unsent_d = 0;
			m_nothing_to_send_pause_timer = 1.0;
		}

		// If nothing was found for sending and nothing was queued for
		// emerging, continue next time browsing from here
		if (nearest_emerged_d != -1 &&
				(nearest_emergefull_d == -1 ||
						nearest_emerged_d <= nearest_emergefull_d)) {
			new_nearest_unsent_d = nearest_emerged_d;
		} else if (nearest_emergefull_d != -1) {
			new_nearest_unsent_d = nearest_emergefull_d;
		} else {
			if (d > full_d_max) {
				new_nearest_unsent_d = 0;
				m_nothing_to_send_pause_timer = 10.0;
			} else {
				if (nearest_sent_d != -1)
					new_nearest_unsent_d = nearest_sent_d;
				else
					new_nearest_unsent_d = d;
			}
		}

		if (new_nearest_unsent_d != -1) {
			m_nearest_unsent_d = new_nearest_unsent_d;
		}
	}

	return num_blocks_selected - num_blocks_sending;
}

uint32_t RemoteClient::SendFarBlocks(
		const int32_t uptime, const far_blocks_ready_t &new_far_blocks)
{
	TimeTaker time("Server: Send far [ms]");

	const static thread_local int32_t retry_interval = static_cast<int32_t>(
			std::max(1.0f, g_settings->getFloat("client_unload_unused_data_timeout")));
	std::multimap<int32_t, MapBlockPtr> ordered;
	uint16_t sent_cnt{};
	constexpr uint16_t send_max{100};
	bool disk_budget_exhausted{};
	WITH_UNIQUE_LOCK(far_blocks_requested_mutex)
	{
		const auto queue_block = [&ordered, &sent_cnt](
										 const MapBlockPtr &block, block_step_t step) {
			block->far_step = step;
			// Reverse traversal below sends the smallest far step first.
			ordered.emplace(-static_cast<int32_t>(step), block);
			++sent_cnt;
		};

		thread_local static const pos_t setting_farmesh_all_changed =
				g_settings->getU32("farmesh_all_changed");
		const auto farmesh_range = farmesh.load();
		const auto client_changed_range = farmesh_all_changed.load();
		const auto quality = farmesh_quality.load();
		const bool send_changed = farmesh_range && have_farmesh_quality.load() &&
								  client_changed_range && setting_farmesh_all_changed;
		const auto changed_range =
				send_changed ? std::min(setting_farmesh_all_changed, client_changed_range)
							 : 0;
		const auto quality_pow = farmesh::rangeToStep(quality);
		const auto cell_size_pow = farmesh::rangeToStep(1); // FMTODO from remoteclient

		bool checked_player_block_pos{};
		bool have_player_block_pos{};
		v3bpos_t player_block_pos;
		const auto changed_block_wanted = [&](const v3bpos_t &bpos, block_step_t step) {
			if (!send_changed)
				return false;
			if (!checked_player_block_pos) {
				auto *player = m_env->getPlayer(peer_id);
				auto *sao = player ? player->getPlayerSAO() : nullptr;
				if (sao) {
					player_block_pos =
							floatToInt(sao->getBasePosition(), BS * MAP_BLOCKSIZE);
					have_player_block_pos = true;
				}
				checked_player_block_pos = true;
			}
			if (!have_player_block_pos)
				return false;

			// TODO: use block center, consistently with the old full-grid scan.
			const auto bdist = radius_box(player_block_pos, bpos);
			const auto bdist_nodes = static_cast<uint64_t>(bdist) << MAP_BLOCKP;
			if (bdist_nodes > static_cast<uint64_t>(changed_range))
				return false;

			const auto params = farmesh::getFarParams(player_block_pos, cell_size_pow,
					farmesh_range, quality_pow, bpos, true);
			return params && params->pos == bpos && params->step == step;
		};

		// A merge notification carries the already-built block, so requested blocks
		// avoid a database retry and changed blocks avoid a full far-grid scan.
		for (block_step_t step = 0; step < new_far_blocks.size(); ++step) {
			for (const auto &[bpos, block] : new_far_blocks[step]) {
				if (!block)
					continue;

				auto requested = far_blocks_requested[step].find(bpos);
				if (requested != far_blocks_requested[step].end()) {
					far_blocks_requested[step].erase(requested);
				} else if (!changed_block_wanted(bpos, step)) {
					continue;
				}
				far_blocks_ready[step].insert_or_assign(bpos, block);
			}
		}

		const auto queue_ready = [&](uint16_t limit) {
			for (block_step_t step = 0;
					step < far_blocks_ready.size() && sent_cnt < limit; ++step) {
				auto &ready = far_blocks_ready[step];
				for (auto it = ready.begin(); it != ready.end() && sent_cnt < limit;) {
					// A repeated request may arrive while a ready block is queued.
					far_blocks_requested[step].erase(it->first);
					queue_block(it->second, step);
					it = ready.erase(it);
				}
			}
		};
		// Reserve half the batch for disk requests, then reuse any spare capacity.
		queue_ready(send_max / 2);
		const auto disk_deadline = porting::getTimeMs() + 10;

		// Ordinary requests stay lazy. A database miss gets one infrequent safety
		// retry; a merge notification above makes it immediately ready.
		for (block_step_t step = 0; step < far_blocks_requested.size() &&
									sent_cnt < send_max && !disk_budget_exhausted;
				++step) {
			auto &requested = far_blocks_requested[step];
			MapDatabase *dbase{};
			bool checked_database{};
			for (auto it = requested.begin();
					it != requested.end() && sent_cnt < send_max;) {
				if (far_blocks_ready[step].contains(it->first)) {
					it = requested.erase(it);
					continue;
				}
				if (it->second.retry_after > uptime) {
					++it;
					continue;
				}

				if (porting::getTimeMs() >= disk_deadline) {
					disk_budget_exhausted = true;
					break;
				}

				if (!checked_database) {
					dbase = GetFarDatabase(m_env->m_map->m_db.dbase,
							m_env->m_server->far_dbases, m_env->m_map->m_savedir, step);
					checked_database = true;
				}
				const auto block =
						dbase ? loadBlockNoStore(m_env->m_map.get(), dbase, it->first)
							  : nullptr;
				if (!block) {
					it->second.retry_after = uptime + retry_interval;
					++it;
					continue;
				}

				queue_block(block, step);
				it = requested.erase(it);
			}
		}
		queue_ready(send_max);
	}

	if (ordered.empty())
		// Continue unfinished lookups promptly even if this slice only found misses.
		return disk_budget_exhausted ? 1 : 0;

	g_profiler->add("Server: Far blocks sent", sent_cnt);
	std::vector<MapBlockPtr> blocks;
	const auto send = [&]() {
		if (!blocks.empty())
			m_env->m_server->SendBlocksFm(
					peer_id, blocks, serialization_version, net_proto_version);
	};
	for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
		if (net_proto_version_fm < 3) {
			m_env->m_server->SendBlockFm(
					peer_id, it->second, serialization_version, net_proto_version);
		} else {
			blocks.emplace_back(it->second);
			if (blocks.size() >= send_max) {
				send();
				blocks.clear();
			}
		}
	}
	send();

	return sent_cnt;
}
/*
RemoteClientVector ClientInterface::getClientList()
{
	const auto lock = m_clients.lock_unique_rec();
	RemoteClientVector clients;
	for (const auto &ir : m_clients) {
		const auto &c = ir.second;
		if (!c)
			continue;
		clients.emplace_back(c);
	}
	return clients;
}
*/
