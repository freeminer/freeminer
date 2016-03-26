/*
clientiface.cpp
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include <sstream>

#include "clientiface.h"
#include "util/numeric.h"
#include "util/mathconstants.h"
#include "player.h"
#include "settings.h"
#include "mapblock.h"
#include "network/connection.h"
#include "environment.h"
#include "map.h"
#include "emerge.h"
#include "serverobject.h"              // TODO this is used for cleanup of only
#include "log_types.h"
#include "util/srp.h"

#include "util/numeric.h"
#include "util/mathconstants.h"
#include "profiler.h"
#include "gamedef.h"


//VERY BAD COPYPASTE FROM clientmap.cpp!
static bool isOccluded(Map *map, v3s16 p0, v3s16 p1, float step, float stepfac,
		float start_off, float end_off, u32 needed_count, INodeDefManager *nodemgr,
		unordered_map_v3POS<bool> & occlude_cache)
{
	float d0 = (float)1 * p0.getDistanceFrom(p1);
	v3s16 u0 = p1 - p0;
	v3f uf = v3f(u0.X, u0.Y, u0.Z);
	uf.normalize();
	v3f p0f = v3f(p0.X, p0.Y, p0.Z);
	u32 count = 0;
	for(float s=start_off; s<d0+end_off; s+=step){
		v3f pf = p0f + uf * s;
		v3s16 p = floatToInt(pf, 1);
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
		if(f.solidness == 0)
			is_transparent = (f.visual_solidness != 2);
		else
			is_transparent = (f.solidness != 2);
		}
		if (cache)
			occlude_cache[p] = is_transparent;
		if(!is_transparent){
			if(count == needed_count)
				return true;
			count++;
		}
		step *= stepfac;
	}
	return false;
}

const char *ClientInterface::statenames[] = {
	"Invalid",
	"Disconnecting",
	"Denied",
	"Created",
	"AwaitingInit2",
	"HelloSent",
	"InitDone",
	"DefinitionsSent",
	"Active",
	"SudoMode",
};



std::string ClientInterface::state2Name(ClientState state)
{
	return statenames[state];
}

void RemoteClient::ResendBlockIfOnWire(v3s16 p)
{
	// if this block is on wire, mark it for sending again as soon as possible
	SetBlockNotSent(p);
}

int RemoteClient::GetNextBlocks (
		ServerEnvironment *env,
		EmergeManager * emerge,
		float dtime,
		double m_uptime,
		std::vector<PrioritySortedBlockTransfer> &dest)
{
	DSTACK(FUNCTION_NAME);

	auto lock = lock_unique_rec();
	if (!lock->owns_lock())
		return 0;

	// Increment timers
	m_nothing_to_send_pause_timer -= dtime;
	m_nearest_unsent_reset_timer += dtime;
	m_time_from_building += dtime;

	if (m_nearest_unsent_reset) {
		m_nearest_unsent_reset = 0;
		m_nearest_unsent_reset_timer = 999;
		m_nothing_to_send_pause_timer = 0;
	}

	if(m_nothing_to_send_pause_timer >= 0)
		return 0;

	Player *player = env->getPlayer(peer_id);
	// This can happen sometimes; clients and players are not in perfect sync.
	if(player == NULL)
		return 0;

	v3f playerpos = player->getPosition();
	v3f playerspeed = player->getSpeed();
	if(playerspeed.getLength() > 1000.0*BS) //cheater or bug, ignore him
		return 0;
	v3f playerspeeddir(0,0,0);
	if(playerspeed.getLength() > 1.0*BS)
		playerspeeddir = playerspeed / playerspeed.getLength();
	// Predict to next block
	v3f playerpos_predicted = playerpos + playerspeeddir*MAP_BLOCKSIZE*BS;

	v3s16 center_nodepos = floatToInt(playerpos_predicted, BS);

	v3s16 center = getNodeBlockPos(center_nodepos);

	// Camera position and direction
	v3f camera_pos = player->getEyePosition();
	v3f camera_dir = v3f(0,0,1);
	camera_dir.rotateYZBy(player->getPitch());
	camera_dir.rotateXZBy(player->getYaw());

	//infostream<<"camera_dir=("<<camera_dir<<")"<< " camera_pos="<<camera_pos<<std::endl;

	/*
		Get the starting value of the block finder radius.
	*/

	if(m_last_center != center)
	{
		m_last_center = center;
		m_nearest_unsent_reset_timer = 999;
	}

	if (m_last_direction.getDistanceFrom(camera_dir)>0.4) { // 1 = 90deg
		m_last_direction = camera_dir;
		m_nearest_unsent_reset_timer = 999;
	}

	/*infostream<<"m_nearest_unsent_reset_timer="
			<<m_nearest_unsent_reset_timer<<std::endl;*/

	// Reset periodically to workaround for some bugs or stuff
	if(m_nearest_unsent_reset_timer > 120.0)
	{
		m_nearest_unsent_reset_timer = 0;
		m_nearest_unsent_d = 0;
		m_nearest_unsent_reset = 0;
		//infostream<<"Resetting m_nearest_unsent_d for "<<peer_id<<std::endl;
	}

	//s16 last_nearest_unsent_d = m_nearest_unsent_d;
	s16 d_start = m_nearest_unsent_d;

	//infostream<<"d_start="<<d_start<<std::endl;

	static const u16 max_simul_sends_setting = g_settings->getU16
			("max_simultaneous_block_sends_per_client");
	static const u16 max_simul_sends_usually = max_simul_sends_setting;

	/*
		Check the time from last addNode/removeNode.

		Decrease send rate if player is building stuff.
	*/
	static const auto full_block_send_enable_min_time_from_building = g_settings->getFloat("full_block_send_enable_min_time_from_building");
	if(m_time_from_building < full_block_send_enable_min_time_from_building)
	{
		/*
		max_simul_sends_usually
			= LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS;
		*/
		if(d_start<=1)
			d_start=2;
		++m_nearest_unsent_reset_want;
	} else if (m_nearest_unsent_reset_want) {
		m_nearest_unsent_reset_want = 0;
		m_nearest_unsent_reset_timer = 999; //magical number more than ^ other number 120 - need to reset d on next iteration
	}

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

	static const auto max_block_send_distance = g_settings->getS16("max_block_send_distance");
	s16 full_d_max = max_block_send_distance;
	if (wanted_range) {
		s16 wanted_blocks = wanted_range / MAP_BLOCKSIZE + 1;
		if (wanted_blocks < full_d_max)
			full_d_max = wanted_blocks;
	}

	s16 d_max = full_d_max;
	static const s16 d_max_gen = g_settings->getS16("max_block_generate_distance");

	// Don't loop very much at a time
	s16 max_d_increment_at_time = 10;
	if(d_max > d_start + max_d_increment_at_time)
		d_max = d_start + max_d_increment_at_time;
	/*if(d_max_gen > d_start+2)
		d_max_gen = d_start+2;*/

	//infostream<<"Starting from "<<d_start<<std::endl;

	s32 nearest_emerged_d = -1;
	s32 nearest_emergefull_d = -1;
	s32 nearest_sent_d = -1;
	//bool queue_is_full = false;

	f32 speed_in_blocks = (playerspeed/(MAP_BLOCKSIZE*BS)).getLength();

	int num_blocks_air = 0;
	int blocks_occlusion_culled = 0;
	static const bool server_occlusion = g_settings->getBool("server_occlusion");
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

	if(n && nodemgr->get(n).solidness == 2)
		occlusion_culling_enabled = false;

	unordered_map_v3POS<bool> occlude_cache;

	s16 d;
	for(d = d_start; d <= d_max; d++) {
		/*errorstream<<"checking d="<<d<<" for "
				<<server->getPlayerName(peer_id)<<std::endl;*/
		//infostream<<"RemoteClient::SendBlocks(): d="<<d<<" d_start="<<d_start<<" d_max="<<d_max<<" d_max_gen="<<d_max_gen<<std::endl;

		std::vector<v3POS> list;
		if (d > 2 && d == d_start && !m_nearest_unsent_reset_want && m_nearest_unsent_reset_timer != 999) { // oops, again magic number from up ^
			list.push_back(v3POS(0,0,0));
		}

		bool can_skip = d > 1;
		// Fast fall/move optimize. speed_in_blocks now limited to 6.4
		if (speed_in_blocks>0.8 && d <= 2) {
			can_skip = false;
			if (d == 0) {
				for(s16 addn = 0; addn < (speed_in_blocks+1)*2; ++addn)
					list.push_back(floatToInt(playerspeeddir*addn, 1));
			} else if (d == 1) {
				for(s16 addn = 0; addn < (speed_in_blocks+1)*1.5; ++addn) {
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( 0,  0,  1)); // back
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( -1, 0,  0)); // left
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( 1,  0,  0)); // right
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( 0,  0, -1)); // front
				}
			} else if (d == 2) {
				for(s16 addn = 0; addn < (speed_in_blocks+1)*1.5; ++addn) {
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( -1, 0,  1)); // back left
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( 1,  0,  1)); // left right
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( -1, 0, -1)); // right left
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3POS( 1,  0, -1)); // front right
				}
			}
		} else {
		/*
			Get the border/face dot coordinates of a "d-radiused"
			box
		*/
			list = FacePositionCache::getFacePositions(d);
		}


		for(auto li=list.begin(); li!=list.end(); ++li)
		{
			v3POS p = *li + center;

			/*
				Send throttling
				- Don't allow too many simultaneous transfers
				- EXCEPT when the blocks are very close

				Also, don't send blocks that are already flying.
			*/

			// Start with the usual maximum
			u16 max_simul_dynamic = max_simul_sends_usually;

			// If block is very close, allow full maximum
			if(d <= BLOCK_SEND_DISABLE_LIMITS_MAX_D)
				max_simul_dynamic = max_simul_sends_setting;

			// Don't select too many blocks for sending
			if (num_blocks_selected + num_blocks_sending >= max_simul_dynamic) {
				//queue_is_full = true;
				goto queue_full_break;
			}

			/*
				Do not go over-limit
			*/
			if (blockpos_over_limit(p))
				continue;

			// If this is true, inexistent block will be made from scratch
			bool generate = d <= d_max_gen;

			{
				/*// Limit the generating area vertically to 2/3
				if(abs(p.Y - center.Y) > d_max_gen - d_max_gen / 3)
					generate = false;*/

				/* maybe good idea (if not use block culling) but brokes far (25+) area generate by flooding emergequeue with no generate blocks
				// Limit the send area vertically to 1/2
				if(can_skip && abs(p.Y - center.Y) > full_d_max / 2)
					generate = false;
				*/
			}


			//infostream<<"d="<<d<<std::endl;
			/*
				Don't generate or send if not in sight
				FIXME This only works if the client uses a small enough
				FOV setting. The default of 72 degrees is fine.
			*/

			float camera_fov = ((fov+5)*M_PI/180) * 4./3.;
			if(can_skip && isBlockInSight(p, camera_pos, camera_dir, camera_fov, 10000*BS) == false)
			{
				continue;
			}

			/*
				Don't send already sent blocks
			*/
			unsigned int block_sent = 0;
			{
				auto lock = m_blocks_sent.lock_shared_rec();
				block_sent = m_blocks_sent.find(p) != m_blocks_sent.end() ? m_blocks_sent.get(p) : 0;
			}

			if(block_sent > 0 && (/* (block_overflow && d>1) || */ block_sent + (d <= 2 ? 1 : d*d*d) > m_uptime)) {
				continue;
			}

			/*
				Check if map has this block
			*/

			MapBlock *block;
			{
#if !ENABLE_THREADS
			auto lock = env->getServerMap().m_nothread_locker.lock_shared_rec();
#endif

			block = env->getMap().getBlockNoCreateNoEx(p);
			}

			//bool surely_not_found_on_disk = false;
			bool block_is_invalid = false;
			if(block != NULL)
			{

				/*if (d > 3 && block->content_only == CONTENT_AIR) {
					continue;
				}*/

				if (block_sent > 0 && block_sent >= block->m_changed_timestamp) {
					continue;
				}

		if (occlusion_culling_enabled) {
			ScopeProfiler sp(g_profiler, "SMap: Occusion calls");
			//Occlusion culling
			auto cpn = p*MAP_BLOCKSIZE;

			// No occlusion culling when free_move is on and camera is
			// inside ground
			cpn += v3POS(MAP_BLOCKSIZE/2, MAP_BLOCKSIZE/2, MAP_BLOCKSIZE/2);

			float step = 1;
			float stepfac = 1.3;
			float startoff = 5;
			float endoff = -MAP_BLOCKSIZE;
			v3POS spn = cam_pos_nodes + v3POS(0,0,0);
			s16 bs2 = MAP_BLOCKSIZE/2 + 1;
			u32 needed_count = 1;
#if !ENABLE_THREADS
			auto lock = env->getServerMap().m_nothread_locker.lock_shared_rec();
#endif
			//VERY BAD COPYPASTE FROM clientmap.cpp!
			if( can_skip &&
				occlusion_culling_enabled &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(0,0,0),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(bs2,bs2,bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(bs2,bs2,-bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(bs2,-bs2,bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(bs2,-bs2,-bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(-bs2,bs2,bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(-bs2,bs2,-bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(-bs2,-bs2,bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache) &&
				isOccluded(&env->getMap(), spn, cpn + v3POS(-bs2,-bs2,-bs2),
					step, stepfac, startoff, endoff, needed_count, nodemgr, occlude_cache)
			)
			{
				//infostream<<" occlusion player="<<cam_pos_nodes<<" d="<<d<<" block="<<cpn<<" total="<<blocks_occlusion_culled<<"/"<<num_blocks_selected<<std::endl;
				g_profiler->add("SMap: Occlusion skip", 1);
				blocks_occlusion_culled++;
				continue;
			}
		}

				// Reset usage timer, this block will be of use in the future.
				block->resetUsageTimer();

				if (block->getLightingExpired()) {
					//env->getServerMap().lighting_modified_blocks.set(p, nullptr);
					env->getServerMap().lighting_modified_add(p, d);
					if (block_sent && can_skip)
						continue;
				}

				if (block->lighting_broken > 0 && (block_sent || can_skip))
					continue;

				// Block is valid if lighting is up-to-date and data exists
				if(block->isValid() == false)
				{
					block_is_invalid = true;
				}

				if(block->isGenerated() == false)
				{
					continue;
				}

				/*
					If block is not close, don't send it unless it is near
					ground level.

					Block is near ground level if night-time mesh
					differs from day-time mesh.
				*/
/*
				if(d >= 4)
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
			if(!block || /*surely_not_found_on_disk ||*/ block_is_invalid)
			{
				//infostream<<"start gen d="<<d<<" p="<<p<<" notfound="<<surely_not_found_on_disk<<" invalid="<< block_is_invalid<<" block="<<block<<" generate="<<generate<<std::endl;
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
					//infostream << "skip tryload " << p << "\n";
				}

				// get next one.
				continue;
			}

			if(nearest_sent_d == -1)
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

	//infostream<<"Stopped at "<<d<<" d_start="<<d_start<< " d_max="<<d_max<<" nearest_emerged_d="<<nearest_emerged_d<<" nearest_emergefull_d="<<nearest_emergefull_d<< " new_nearest_unsent_d="<<new_nearest_unsent_d<< " sel="<<num_blocks_selected<< "+"<<num_blocks_sending << " air="<<num_blocks_air<< " culled=" << blocks_occlusion_culled <<" cEN="<<occlusion_culling_enabled<<std::endl;
	num_blocks_selected += num_blocks_sending;
	if(!num_blocks_selected && !num_blocks_air && d_start <= d) {
		//new_nearest_unsent_d = 0;
		m_nothing_to_send_pause_timer = 1.0;
	}
		

	// If nothing was found for sending and nothing was queued for
	// emerging, continue next time browsing from here
	if(nearest_emerged_d != -1 && nearest_emerged_d > nearest_emergefull_d){
		new_nearest_unsent_d = nearest_emerged_d;
	} else if(nearest_emergefull_d != -1){
		new_nearest_unsent_d = nearest_emergefull_d;
	} else {
		if(d > full_d_max){
			new_nearest_unsent_d = 0;
			m_nothing_to_send_pause_timer = 10.0;
		} else {
			if(nearest_sent_d != -1)
				new_nearest_unsent_d = nearest_sent_d;
			else
				new_nearest_unsent_d = d;
		}
	}

	if(new_nearest_unsent_d != -1) {
		m_nearest_unsent_d = new_nearest_unsent_d;
	}

	return num_blocks_selected - num_blocks_sending;
}

/*
void RemoteClient::GotBlock(v3s16 p)
{
	if (m_blocks_modified.find(p) == m_blocks_modified.end()) {
		if (m_blocks_sending.find(p) != m_blocks_sending.end())
			m_blocks_sending.erase(p);
		else
			m_excess_gotblocks++;

		m_blocks_sent.insert(p);
	}
}
*/

void RemoteClient::SentBlock(v3s16 p, double time)
{
	m_blocks_sent.set(p, time);
}

/*
void RemoteClient::SentBlock(v3s16 p)
{
	if (m_blocks_modified.find(p) != m_blocks_modified.end())
		m_blocks_modified.erase(p);

	if(m_blocks_sending.find(p) == m_blocks_sending.end())
		m_blocks_sending[p] = 0.0;
	else
		infostream<<"RemoteClient::SentBlock(): Sent block"
				" already in m_blocks_sending"<<std::endl;
}
*/

void RemoteClient::SetBlockNotSent(v3s16 p)
{
	++m_nearest_unsent_reset;
/*
	m_nearest_unsent_d = 0;
	m_nothing_to_send_pause_timer = 0;

	if(m_blocks_sending.find(p) != m_blocks_sending.end())
		m_blocks_sending.erase(p);
	if(m_blocks_sent.find(p) != m_blocks_sent.end())
		m_blocks_sent.erase(p);
	m_blocks_modified.insert(p);
*/
}

void RemoteClient::SetBlocksNotSent()
{
	++m_nearest_unsent_reset;
}

void RemoteClient::SetBlocksNotSent(std::map<v3s16, MapBlock*> &blocks)
{
	++m_nearest_unsent_reset;
/*
	for(std::map<v3s16, MapBlock*>::iterator
			i = blocks.begin();
			i != blocks.end(); ++i)
	{
		v3s16 p = i->first;
		m_blocks_modified.insert(p);
	}
*/
}

void RemoteClient::SetBlockDeleted(v3s16 p) {
	m_blocks_sent.erase(p);
}

void RemoteClient::notifyEvent(ClientStateEvent event)
{
	std::ostringstream myerror;
	switch (m_state)
	{
	case CS_Invalid:
		//intentionally do nothing
		break;
	case CS_Created:
		switch (event) {
		case CSE_Hello:
			m_state = CS_HelloSent;
			break;
		case CSE_InitLegacy:
			m_state = CS_AwaitingInit2;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		/* GotInit2 SetDefinitionsSent SetMediaSent */
		default:
			myerror << "Created: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_Denied:
		/* don't do anything if in denied state */
		break;
	case CS_HelloSent:
		switch(event)
		{
		case CSE_AuthAccept:
			m_state = CS_AwaitingInit2;
			if ((chosen_mech == AUTH_MECHANISM_SRP)
					|| (chosen_mech == AUTH_MECHANISM_LEGACY_PASSWORD))
				srp_verifier_delete((SRPVerifier *) auth_data);
			chosen_mech = AUTH_MECHANISM_NONE;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			if ((chosen_mech == AUTH_MECHANISM_SRP)
					|| (chosen_mech == AUTH_MECHANISM_LEGACY_PASSWORD))
				srp_verifier_delete((SRPVerifier *) auth_data);
			chosen_mech = AUTH_MECHANISM_NONE;
			break;
		default:
			myerror << "HelloSent: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_AwaitingInit2:
		switch(event)
		{
		case CSE_GotInit2:
			confirmSerializationVersion();
			m_state = CS_InitDone;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;

		/* Init SetDefinitionsSent SetMediaSent */
		default:
			myerror << "InitSent: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;

	case CS_InitDone:
		switch(event)
		{
		case CSE_SetDefinitionsSent:
			m_state = CS_DefinitionsSent;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;

		/* Init GotInit2 SetMediaSent */
		default:
			myerror << "InitDone: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_DefinitionsSent:
		switch(event)
		{
		case CSE_SetClientReady:
			m_state = CS_Active;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		/* Init GotInit2 SetDefinitionsSent */
		default:
			myerror << "DefinitionsSent: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_Active:
		switch(event)
		{
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SudoSuccess:
			m_state = CS_SudoMode;
			if ((chosen_mech == AUTH_MECHANISM_SRP)
					|| (chosen_mech == AUTH_MECHANISM_LEGACY_PASSWORD))
				srp_verifier_delete((SRPVerifier *) auth_data);
			chosen_mech = AUTH_MECHANISM_NONE;
			break;
		/* Init GotInit2 SetDefinitionsSent SetMediaSent SetDenied */
		default:
			myerror << "Active: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
			break;
		}
		break;
	case CS_SudoMode:
		switch(event)
		{
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SudoLeave:
			m_state = CS_Active;
			break;
		default:
			myerror << "Active: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
			break;
		}
		break;
	case CS_Disconnecting:
		/* we are already disconnecting */
		break;
	}
}

u32 RemoteClient::uptime()
{
	return getTime(PRECISION_SECONDS) - m_connection_time;
}

ClientInterface::ClientInterface(con::Connection* con)
:
	m_con(con),
	m_env(NULL),
	m_print_info_timer(0.0)
{

}
ClientInterface::~ClientInterface()
{
}

std::vector<u16> ClientInterface::getClientIDs(ClientState min_state)
{
	std::vector<u16> reply;
	auto clientslock = m_clients.lock_shared_rec();

	for(auto
		i = m_clients.begin();
		i != m_clients.end(); ++i)
	{
		if (i->second->getState() >= min_state)
			reply.push_back(i->second->peer_id);
	}

	return reply;
}

std::vector<std::string> ClientInterface::getPlayerNames()
{
	return m_clients_names;
}


void ClientInterface::step(float dtime)
{
	g_profiler->add("Server: Clients", m_clients.size());
	m_print_info_timer += dtime;
	if(m_print_info_timer >= 30.0)
	{
		m_print_info_timer = 0.0;
		UpdatePlayerList();
	}
}

void ClientInterface::UpdatePlayerList()
{
	if (m_env != NULL)
		{
		std::vector<u16> clients = getClientIDs();
		m_clients_names.clear();


		if(!clients.empty())
			infostream<<"Players ["<<clients.size()<<"]:"<<std::endl;

		for(auto
			i = clients.begin();
			i != clients.end(); ++i) {
			Player *player = m_env->getPlayer(*i);

			if (player == NULL)
				continue;

			infostream << "* " << player->getName() << "\t";

			{
				//MutexAutoLock clientslock(m_clients_mutex);
				RemoteClient* client = lockedGetClientNoEx(*i);
				if(client != NULL)
					client->PrintInfo(infostream);
			}

			m_clients_names.push_back(player->getName());
		}
	}
}



#if !MINETEST_PROTO
void ClientInterface::send(u16 peer_id,u8 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	m_con->Send(peer_id, channelnum, data, reliable);
}

void ClientInterface::send(u16 peer_id,u8 channelnum,
		const msgpack::sbuffer &buffer, bool reliable)
{
	SharedBuffer<u8> data((unsigned char*)buffer.data(), buffer.size());
	send(peer_id, channelnum, data, reliable);
}
#endif

#if MINETEST_PROTO
void ClientInterface::send(u16 peer_id, u8 channelnum,
		NetworkPacket* pkt, bool reliable)
{
	m_con->Send(peer_id, channelnum, pkt, reliable);
}

void ClientInterface::sendToAll(u16 channelnum,
		NetworkPacket* pkt, bool reliable)
{
	auto clientslock = m_clients.lock_shared_rec();
	for(auto
		i = m_clients.begin();
		i != m_clients.end(); ++i)
	{
		RemoteClient *client = i->second.get();

		if (client->net_proto_version != 0) {
			m_con->Send(client->peer_id, channelnum, pkt, reliable);
		}
	}
}

#else

void ClientInterface::sendToAll(u16 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	auto lock = m_clients.lock_shared_rec();
	for(auto
		i = m_clients.begin();
		i != m_clients.end(); ++i)
	{
		RemoteClient *client = i->second.get();

		if (client->net_proto_version != 0)
		{
			m_con->Send(client->peer_id, channelnum, data, reliable);
		}
	}
}

void ClientInterface::sendToAll(u16 channelnum,
		const msgpack::sbuffer &buffer, bool reliable)
{
	SharedBuffer<u8> data((unsigned char*)buffer.data(), buffer.size());
	sendToAll(channelnum, data, reliable);
}
#endif

//TODO: return here shared_ptr
RemoteClient* ClientInterface::getClientNoEx(u16 peer_id, ClientState state_min)
{
	auto client = getClient(peer_id, state_min);
	return client.get();
}

std::shared_ptr<RemoteClient> ClientInterface::getClient(u16 peer_id, ClientState state_min) {
	auto clientslock = m_clients.lock_shared_rec();
	auto n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n == m_clients.end())
		return NULL;

	if (n->second->getState() >= state_min)
		return n->second;
	else
		return NULL;
}

RemoteClient* ClientInterface::lockedGetClientNoEx(u16 peer_id, ClientState state_min)
{
	return getClientNoEx(peer_id, state_min);
}

ClientState ClientInterface::getClientState(u16 peer_id)
{
	auto clientslock = m_clients.lock_shared_rec();
	auto n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n == m_clients.end())
		return CS_Invalid;

	return n->second->getState();
}

void ClientInterface::setPlayerName(u16 peer_id,std::string name)
{
	auto client = getClient(peer_id, CS_Invalid);
	if(!client)
		return;

	client->setName(name);
}

void ClientInterface::DeleteClient(u16 peer_id)
{
	auto client = getClient(peer_id, CS_Invalid);
	if(!client)
		return;

	/*
		Mark objects to be not known by the client
	*/
	//TODO this should be done by client destructor!!!
	// Handle objects
	{
	auto lock = client->m_known_objects.lock_unique_rec();
	for(auto
			i = client->m_known_objects.begin();
			i != client->m_known_objects.end(); ++i)
	{
		// Get object
		u16 id = i->first;
		ServerActiveObject* obj = m_env->getActiveObject(id, true);

		if(obj && obj->m_known_by_count > 0)
			obj->m_known_by_count--;
	}
	}

	// Delete client
	//delete m_clients.get(peer_id);
	m_clients.erase(peer_id);
}

void ClientInterface::CreateClient(u16 peer_id)
{
	{
		auto client = getClient(peer_id, CS_Invalid);
		if(client)
			return;
	}

	// Create client
	auto client = std::shared_ptr<RemoteClient>(new RemoteClient(m_env));
	client->peer_id = peer_id;
	m_clients.set(client->peer_id, client);
}

void ClientInterface::event(u16 peer_id, ClientStateEvent event)
{
	auto client = getClient(peer_id, CS_Invalid);
	if(!client)
		return;

	client->notifyEvent(event);

	if ((event == CSE_SetClientReady) ||
		(event == CSE_Disconnect)     ||
		(event == CSE_SetDenied))
	{
		UpdatePlayerList();
	}
}

u16 ClientInterface::getProtocolVersion(u16 peer_id)
{
	auto client = getClient(peer_id, CS_Invalid);
	if(!client)
		return 0;

	return client->net_proto_version;
}

void ClientInterface::setClientVersion(u16 peer_id, u8 major, u8 minor, u8 patch, std::string full)
{
	auto client = getClient(peer_id, CS_Invalid);
	if(!client)
		return;

	client->setVersionInfo(major,minor,patch,full);
}
