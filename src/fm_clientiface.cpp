#include "clientiface.h"
#include "irr_v3d.h"
#include "map.h"
#include "profiler.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "server.h"
#include "emerge.h"
#include "face_position_cache.h"
#include "util/numeric.h"

// VERY BAD COPYPASTE FROM clientmap.cpp!
static bool isOccluded(Map *map, v3pos_t p0, v3pos_t p1, float step, float stepfac,
		float start_off, float end_off, u32 needed_count, const NodeDefManager *nodemgr,
		unordered_map_v3pos<bool> &occlude_cache)
{
	float d0 = (float)1 * p0.getDistanceFrom(p1);
	v3pos_t u0 = p1 - p0;
	v3f uf = v3f(u0.X, u0.Y, u0.Z);
	uf.normalize();
	v3f p0f = v3f(p0.X, p0.Y, p0.Z);
	u32 count = 0;
	for (float s = start_off; s < d0 + end_off; s += step) {
		v3f pf = p0f + uf * s;
		v3pos_t p = floatToInt(pf, 1);
		bool is_transparent = false;
		bool cache = true;
		if (occlude_cache.count(p)) {
			cache = false;
			is_transparent = occlude_cache[p];
		} else {
			MapNode n = map->getNodeTry(p);
			if (!n) {
				return true; // ONE DIFFERENCE FROM clientmap.cpp
			}
			const ContentFeatures &f = nodemgr->get(n);
			if (f.solidness == 0)
				is_transparent = (f.visual_solidness != 2);
			else
				is_transparent = (f.solidness != 2);
		}
		if (cache)
			occlude_cache[p] = is_transparent;
		if (!is_transparent) {
			if (count == needed_count)
				return true;
			count++;
		}
		step *= stepfac;
	}
	return false;
}

int RemoteClient::GetNextBlocks(ServerEnvironment *env, EmergeManager *emerge,
		float dtime, std::vector<PrioritySortedBlockTransfer> &dest, double m_uptime)
{
	auto lock = try_lock_unique_rec();
	if (!lock->owns_lock())
		return 0;

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

	v3opos_t playerpos = sao->getBasePosition();
	v3f playerspeed = player->getSpeed();
	if (playerspeed.getLength() > 120.0 * BS) // cheater or bug, ignore him
		return 0;
	v3f playerspeeddir(0, 0, 0);
	if (playerspeed.getLength() > 1.0 * BS)
		playerspeeddir = playerspeed / playerspeed.getLength();
	// Predict to next block
	v3opos_t playerpos_predicted =
			playerpos + v3fToOpos(playerspeeddir) * MAP_BLOCKSIZE * BS;

	v3pos_t center_nodepos = floatToInt(playerpos_predicted, BS);

	v3pos_t center = getNodeBlockPos(center_nodepos);

	// Camera position and direction
	//v3opos_t camera_pos = sao->getEyePosition();
	v3f camera_dir = v3f(0, 0, 1);
	camera_dir.rotateYZBy(sao->getLookPitch());
	camera_dir.rotateXZBy(sao->getRotation().Y);

	// infostream<<"camera_dir=("<<camera_dir<<")"<< "
	// camera_pos="<<camera_pos<<std::endl;

	/*
		Get the starting value of the block finder radius.
	*/

	if (m_last_center != center) {
		m_last_center = center;
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

	if (m_nothing_to_send_pause_timer >= 0) {
		return 0;
	}

	// s16 last_nearest_unsent_d = m_nearest_unsent_d;
	auto d_start = m_nearest_unsent_d.load();

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
	s16 wanted_range = sao->getWantedRange();
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
		s16 wanted_blocks = wanted_range /* / MAP_BLOCKSIZE */ + 1;
		if (wanted_blocks < full_d_max)
			full_d_max = wanted_blocks;
	}

	/*
		const s16 full_d_max = MYMIN(g_settings->getS16("max_block_send_distance"),
	   wanted_range); const s16 d_opt =
	   MYMIN(g_settings->getS16("block_send_optimize_distance"), wanted_range);
	*/

	const s16 d_blocks_in_sight = full_d_max * BS * MAP_BLOCKSIZE;
	// infostream << "Fov from client " << camera_fov << " full_d_max " << full_d_max <<
	// std::endl;

	s16 d_max = full_d_max;
	thread_local static const s16 d_max_gen_s =
			g_settings->getS16("max_block_generate_distance");
	s16 d_max_gen = MYMIN(d_max_gen_s, wanted_range);

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

	const f32 speed_in_blocks = (playerspeed / (MAP_BLOCKSIZE * BS)).getLength();

	int num_blocks_air = 0;
	int blocks_occlusion_culled = 0;
	thread_local static const bool server_occlusion =
			g_settings->getBool("server_occlusion");
	bool occlusion_culling_enabled = server_occlusion;

	auto cam_pos_nodes = floatToInt(playerpos, BS);

	auto nodemgr = env->getGameDef()->getNodeDefManager();
	MapNode n;
	{
#if !ENABLE_THREADS
		auto lock = env->getServerMap().m_nothread_locker.lock_shared_rec();
#endif
		n = env->getMap().getNodeTry(cam_pos_nodes);
	}

	if (n && nodemgr->get(n).solidness == 2)
		occlusion_culling_enabled = false;

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

		bool can_skip = d > 1;
		// Fast fall/move optimize. speed_in_blocks now limited to 6.4
		if (speed_in_blocks > 0.8 && d <= 2) {
			can_skip = false;
			if (d == 0) {
				for (s16 addn = 0; addn < (speed_in_blocks + 1) * 2; ++addn)
					list.push_back(floatToInt(playerspeeddir * addn, 1));
			} else if (d == 1) {
				for (s16 addn = 0; addn < (speed_in_blocks + 1) * 1.5; ++addn) {
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(0, 0, 1)); // back
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(-1, 0, 0)); // left
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(1, 0, 0)); // right
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(0, 0, -1)); // front
				}
			} else if (d == 2) {
				for (s16 addn = 0; addn < (speed_in_blocks + 1) * 1.5; ++addn) {
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(-1, 0, 1)); // back left
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(1, 0, 1)); // left right
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(-1, 0, -1)); // right left
					list.push_back(floatToInt(playerspeeddir * addn, 1) +
								   v3pos_t(1, 0, -1)); // front right
				}
			}
		} else {
			/*
				Get the border/face dot coordinates of a "d-radiused"
				box
			*/
			list = FacePositionCache::getFacePositions(d);
		}

		for (auto li = list.begin(); li != list.end(); ++li) {
			v3pos_t p = *li + center;

			/*
				Send throttling
				- Don't allow too many simultaneous transfers
				- EXCEPT when the blocks are very close

				Also, don't send blocks that are already flying.
			*/

			// Start with the usual maximum
			u16 max_simul_dynamic = max_simul_sends_usually;

			// If block is very close, allow full maximum
			if (d <= BLOCK_SEND_DISABLE_LIMITS_MAX_D)
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
			bool generate = d <= d_max_gen;

			// infostream<<"d="<<d<<std::endl;

			/*
				Don't generate or send if not in sight
				FIXME This only works if the client uses a small enough
				FOV setting. The default of 72 degrees is fine.
			*/
			/*
				if (can_skip && isBlockInSight(p, camera_pos, camera_dir, camera_fov,
										d_blocks_in_sight) == false) {
					// DUMP(p, can_skip, "nosight");
					continue;
				}
			*/
			/*
				Don't send already sent blocks
			*/
			double block_sent = 0;
			{
				auto lock = m_blocks_sent.lock_shared_rec();
				block_sent = m_blocks_sent.contains(p) ? m_blocks_sent.get(p) : 0;
			}

			if (block_sent > 0 &&
					(/* (block_overflow && d>1) || */ block_sent + (d <= 2 ? 1 : d * d) >
										  m_uptime)) {
				// DUMP(p, block_sent, d, "ddd");
				continue;
			}

			/*
				Check if map has this block
			*/

			MapBlock *block;
			if (0) {
				auto lock = env->getMap().m_blocks.try_lock_shared_rec();
				if (!lock->owns_lock()) {
					++block_skip_retry;
					if (!first_skipped_d && d > always_first_ds)
						first_skipped_d = d;
					continue;
				}
				block = env->getMap().getBlockNoCreateNoEx(p);
			}
			block = env->getMap().getBlockNoCreateNoEx(p);

			// bool surely_not_found_on_disk = false;
			// bool block_is_invalid = false;
			if (block) {

				/*if (d > 3 && block->content_only == CONTENT_AIR) {
					continue;
				}*/

				if (block_sent > 0 && block_sent >= block->m_changed_timestamp) {
					// DUMP(p, block_sent, block->m_changed_timestamp,
					// block->getDiskTimestamp(), block->getActualTimestamp(), "ch");
					continue;
				}

				if (occlusion_culling_enabled) {
					ScopeProfiler sp(g_profiler, "SMap: Occusion calls");
					// Occlusion culling
					auto cpn = p * MAP_BLOCKSIZE;

					// No occlusion culling when free_move is on and camera is
					// inside ground
					cpn += v3pos_t(
							MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2, MAP_BLOCKSIZE / 2);

					float step = 1;
					float stepfac = 1.3;
					float startoff = 5;
					float endoff = -MAP_BLOCKSIZE;
					v3pos_t spn = cam_pos_nodes + v3pos_t(0, 0, 0);
					s16 bs2 = MAP_BLOCKSIZE / 2 + 1;
					u32 needed_count = 1;
#if !ENABLE_THREADS
					auto lock = env->getServerMap().m_nothread_locker.lock_shared_rec();
#endif
					// VERY BAD COPYPASTE FROM clientmap.cpp!
					if (can_skip && occlusion_culling_enabled &&
							isOccluded(&env->getMap(), spn, cpn + v3pos_t(0, 0, 0), step,
									stepfac, startoff, endoff, needed_count, nodemgr,
									occlude_cache) &&
							isOccluded(&env->getMap(), spn, cpn + v3pos_t(bs2, bs2, bs2),
									step, stepfac, startoff, endoff, needed_count,
									nodemgr, occlude_cache) &&
							isOccluded(&env->getMap(), spn, cpn + v3pos_t(bs2, bs2, -bs2),
									step, stepfac, startoff, endoff, needed_count,
									nodemgr, occlude_cache) &&
							isOccluded(&env->getMap(), spn, cpn + v3pos_t(bs2, -bs2, bs2),
									step, stepfac, startoff, endoff, needed_count,
									nodemgr, occlude_cache) &&
							isOccluded(&env->getMap(), spn,
									cpn + v3pos_t(bs2, -bs2, -bs2), step, stepfac,
									startoff, endoff, needed_count, nodemgr,
									occlude_cache) &&
							isOccluded(&env->getMap(), spn, cpn + v3pos_t(-bs2, bs2, bs2),
									step, stepfac, startoff, endoff, needed_count,
									nodemgr, occlude_cache) &&
							isOccluded(&env->getMap(), spn,
									cpn + v3pos_t(-bs2, bs2, -bs2), step, stepfac,
									startoff, endoff, needed_count, nodemgr,
									occlude_cache) &&
							isOccluded(&env->getMap(), spn,
									cpn + v3pos_t(-bs2, -bs2, bs2), step, stepfac,
									startoff, endoff, needed_count, nodemgr,
									occlude_cache) &&
							isOccluded(&env->getMap(), spn,
									cpn + v3pos_t(-bs2, -bs2, -bs2), step, stepfac,
									startoff, endoff, needed_count, nodemgr,
									occlude_cache)) {
						// infostream<<" occlusion player="<<cam_pos_nodes<<" d="<<d<<"
						// block="<<cpn<<"
						// total="<<blocks_occlusion_culled<<"/"<<num_blocks_selected<<std::endl;
						g_profiler->add("SMap: Occlusion skip", 1);
						blocks_occlusion_culled++;
						continue;
					}
				}

				// Reset usage timer, this block will be of use in the future.
				block->resetUsageTimer();

				const auto complete = block->getLightingComplete();
				if (!complete) {
					env->getServerMap().lighting_modified_add(p, d);
					if (block_sent && can_skip) {
						continue;
					}
				}

				// if (block->lighting_broken > 0 && (block_sent || can_skip))
				//	continue;

				// Block is valid if lighting is up-to-date and data exists
				/*
								if(block->is isValid() == false)
								{
									block_is_invalid = true;
								}
				*/

				if (block->isGenerated() == false) {
					// DUMP(p, block->isGenerated());
					continue;
				}

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
				if (generate || !env->getServerMap().m_db_miss.count(p)) {

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

			PrioritySortedBlockTransfer q((float)d, p, peer_id);

			dest.push_back(q);

			if (block->content_only == CONTENT_AIR)
				++num_blocks_air;
			else
				num_blocks_selected += 1;
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
	if (nearest_emerged_d != -1 && nearest_emerged_d > nearest_emergefull_d) {
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
