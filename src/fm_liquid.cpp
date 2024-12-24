/*
Copyright (C) 2013-2023 proller <proler@gmail.com>
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

#include "constants.h"

#include "emerge.h"
#include "gamedef.h"
#include "irr_v3d.h"
#include "map.h"
#include "mapblock.h"
#include "nodedef.h"
#include "profiler.h"
#include "scripting_server.h"
#include "server.h"
#include "settings.h"
#include "util/unordered_map_hash.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#define LIQUID_DEBUG 0

namespace {
enum NeighborType : u8 {
	NEIGHBOR_UPPER,
	NEIGHBOR_SAME_LEVEL,
	NEIGHBOR_LOWER
};

struct NodeNeighbor
{
	MapNode node;
	NeighborType type;
	v3pos_t pos;
	content_t content;
	bool liquid; // can liquid
	bool infinity;
	int weight;
	int drop; // drop by liquid
};
}

const v3pos_t liquid_flow_dirs[7] = {
		// +right, +top, +back
		v3pos_t(0, -1, 0), // 0 bottom
		v3pos_t(0, 0, 0),  // 1 self
		v3pos_t(0, 0, 1),  // 2 back
		v3pos_t(0, 0, -1), // 3 front
		v3pos_t(1, 0, 0),  // 4 right
		v3pos_t(-1, 0, 0), // 5 left
		v3pos_t(0, 1, 0)   // 6 top
};

// when looking around we must first check self node for correct type definitions
const int8_t liquid_explore_map[7] = {1, 0, 6, 2, 3, 4, 5};
const int8_t liquid_random_map[4][7] = {{0, 1, 2, 3, 4, 5, 6}, {0, 1, 4, 3, 5, 2, 6},
		{0, 1, 3, 5, 4, 2, 6}, {0, 1, 5, 3, 2, 4, 6}};

constexpr auto D_BOTTOM = 0;
constexpr auto D_TOP = 6;
constexpr auto D_SELF = 1;

size_t ServerMap::transforming_liquid_size()
{
	std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
	return m_transforming_liquid.size() + m_transforming_liquid_local_size;
}

v3pos_t ServerMap::transforming_liquid_pop()
{
	std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
	auto front = m_transforming_liquid.front();
	m_transforming_liquid.pop_front();
	return front;

	// const auto lock = m_transforming_liquid.lock_unique_rec();
	// auto it = m_transforming_liquid.begin();
	// auto value = it->first;
	// m_transforming_liquid.erase(it);
	// return value;
}

class cached_map_block
{
	Map *map_{};
	MapBlockPtr cached_block;
	v3bpos_t cached_block_pos;
	std::unique_ptr<MapBlock::lock_rec_unique> lock;

	int hit = 0, miss = 0;

public:
	cached_map_block(Map *map) : map_{map} {}
	~cached_map_block()
	{
		//DUMP(hit, miss, hit / (miss ? miss : 1));
	}

	MapBlockPtr change_block(const v3pos_t &pos)
	{
		auto blockpos = getNodeBlockPos(pos);
		if (cached_block_pos == blockpos && cached_block) {
			++hit;
			return cached_block;
		}
		++miss;
		lock = {};
		cached_block = map_->getBlock(blockpos);
		if (!cached_block)
			return {};
		cached_block_pos = blockpos;
		lock = cached_block->lock_unique_rec();
		return cached_block;
	}

	MapNode getNode(const v3pos_t &pos)
	{
		if (!change_block(pos)) {
			return map_->getNode(pos);
		}
		v3pos_t relpos = pos - cached_block_pos * MAP_BLOCKSIZE;
		return cached_block->getNodeNoLock(relpos);
	}

	void setNode(const v3pos_t &pos, const MapNode &n, bool important = false)
	{
		if (!change_block(pos)) {
			return map_->setNode(pos, n, important);
		}
		v3pos_t relpos = pos - cached_block_pos * MAP_BLOCKSIZE;
		return cached_block->setNodeNoLock(relpos, n, important);
	}
};

size_t ServerMap::transformLiquidsReal(Server *m_server, unsigned int max_cycle_ms)
{
	const auto *nodemgr = m_nodedef;

	// TimeTaker timer("transformLiquidsReal()");
	size_t loopcount = 0;
	const auto initial_size =
			transforming_liquid_size() - m_transforming_liquid_local_size;

	size_t regenerated = 0;

#if LIQUID_DEBUG
	bool debug = 1;
#endif

	thread_local static const uint8_t relax = g_settings->getS16("liquid_relax");
	thread_local static const auto fast_flood = g_settings->getS16("liquid_fast_flood");
	thread_local static const int water_level = g_settings->getS16("water_level");
	const int16_t liquid_pressure = m_server->m_emerge->mgparams->liquid_pressure;
	// g_settings->getS16NoEx("liquid_pressure", liquid_pressure);

	// list of nodes that due to viscosity have not reached their max level height
	// unordered_map_v3pos<bool> must_reflow, must_reflow_second, must_reflow_third;
	std::unordered_set<v3bpos_t> node_update, node_drop;
	std::list<v3pos_t> must_reflow, must_reflow_second; //, must_reflow_third;
	std::unordered_map<v3bpos_t, std::list<v3pos_t>> fast_reflow;
	const auto reflow = [&must_reflow, &fast_reflow](const v3pos_t &pos) {
		const auto blockpos = getNodeBlockPos(pos);
		v3pos_t relpos = pos - blockpos * MAP_BLOCKSIZE;
		if (!relpos.X || !relpos.Y || !relpos.Z || relpos.X == MAP_BLOCKSIZE - 1 ||
				relpos.Y == MAP_BLOCKSIZE - 1 || relpos.Z == MAP_BLOCKSIZE - 1) {
			must_reflow.emplace_back(pos);
			return;
		}
		fast_reflow[blockpos].emplace_back(pos);
	};

	// List of MapBlocks that will require a lighting update (due to lava)
	std::unordered_map<v3bpos_t, size_t> falling;
	uint16_t loop_rand = myrand();

	unordered_set_v3bpos blocks_lighting_update;

	const auto end_ms = porting::getTimeMs() + max_cycle_ms;

	{
		std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);
		m_transforming_liquid_local.reserve(initial_size);
		while (!m_transforming_liquid.m_queue.empty()) {
			m_transforming_liquid_local.emplace_back(
					m_transforming_liquid.m_queue.front());
			m_transforming_liquid.m_queue.pop();
		}
		m_transforming_liquid.m_set.clear();
	}

	{
		cached_map_block cached_map(this);

		for (const auto &p0 : m_transforming_liquid_local) {
			// This should be done here so that it is done when continue is used
			//if (loopcount >= initial_size * 2 || porting::getTimeMs() > end_ms)
			//	break;
			++loopcount;
			/*
			Get a queued transforming liquid node
		*/
			//v3pos_t p0;
			{
				// MutexAutoLock lock(m_transforming_liquid_mutex);
				// p0 = transforming_liquid_pop();
			}
			int16_t total_level = 0;
			// u16 level_max = 0;
			//  surrounding flowing liquid nodes
			NodeNeighbor neighbors[7] = {{}};
			// current level of every block
			int8_t liquid_levels[7] = {-1, -1, -1, -1, -1, -1, -1};
			// target levels
			int8_t liquid_levels_want[7] = {-1, -1, -1, -1, -1, -1, -1};
			int8_t can_liquid_same_level = 0;
			int8_t can_liquid = 0;
			// warning! when MINETEST_PROTO enabled - CONTENT_IGNORE != 0
			content_t liquid_kind = CONTENT_IGNORE;
			content_t liquid_kind_flowing = CONTENT_IGNORE;
			content_t melt_kind = CONTENT_IGNORE;
			content_t melt_kind_flowing = CONTENT_IGNORE;
			// s8 viscosity = 0;

			bool fall_down = false;
			/*
			Collect information about the environment, start from self
		 */
			bool want_continue = false;
			for (uint8_t e = D_BOTTOM; e <= D_TOP; e++) {
				uint8_t i = liquid_explore_map[e];
				NodeNeighbor &nb = neighbors[i];
				nb.pos = p0 + liquid_flow_dirs[i];
				nb.node = cached_map.getNode(neighbors[i].pos);
				nb.content = nb.node.getContent();
				NeighborType nt = NEIGHBOR_SAME_LEVEL;
				switch (i) {
				case D_TOP:
					nt = NEIGHBOR_UPPER;
					break;
				case D_BOTTOM:
					nt = NEIGHBOR_LOWER;
					break;
				}
				nb.type = nt;
				nb.liquid = 0;
				nb.infinity = 0;
				nb.weight = 0;
				nb.drop = 0;

				if (!nb.node) {
					//if (i == D_SELF && (loopcount % 8) && initial_size < m_liquid_step_flow * 3)	// must_reflow_third[nb.pos] = 1;
					//	must_reflow_third.emplace_back(nb.pos);
					continue;
				}

				const auto &f = nodemgr->get(nb.content);
				switch (f.liquid_type) {
				case LIQUID_NONE:
					if (nb.content == CONTENT_AIR) {
						liquid_levels[i] = 0;
						nb.liquid = 1;
					}
					// TODO: if (nb.content == CONTENT_AIR ||
					// nodemgr->get(nb.node).buildable_to && !nodemgr->get(nb.node).walkable)
					// { // need lua drop api for drop torches
					else if (melt_kind_flowing != CONTENT_IGNORE &&
							 nb.content == melt_kind_flowing &&
							 nb.type != NEIGHBOR_UPPER && !(loopcount % 2)) {
						uint8_t melt_max_level = nb.node.getMaxLevel(nodemgr);
						uint8_t my_max_level =
								MapNode(liquid_kind_flowing).getMaxLevel(nodemgr);
						liquid_levels[i] =
								((float)my_max_level / (melt_max_level ? melt_max_level
																	   : my_max_level)) *
								nb.node.getLevel(nodemgr);
						if (liquid_levels[i])
							nb.liquid = 1;
					} else if (melt_kind != CONTENT_IGNORE && nb.content == melt_kind &&
							   nb.type != NEIGHBOR_UPPER && !(loopcount % 8)) {
						liquid_levels[i] =
								nodemgr->get(liquid_kind_flowing).getMaxLevel();
						if (liquid_levels[i])
							nb.liquid = 1;
					} else {
						int drop = ((ItemGroupList)f.groups)["drop_by_liquid"];
						if (drop && !(loopcount % drop)) {
							liquid_levels[i] = 0;
							nb.liquid = 1;
							nb.drop = 1;
						}
					}

					// todo: for erosion add something here..
					break;
				case LIQUID_SOURCE:
					// if this node is not (yet) of a liquid type,
					// choose the first liquid type we encounter
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = f.liquid_alternative_flowing_id;
					if (liquid_kind == CONTENT_IGNORE)
						liquid_kind = nb.content;
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = liquid_kind;
					if (melt_kind == CONTENT_IGNORE)
						melt_kind = f.melt_id;
					if (melt_kind_flowing == CONTENT_IGNORE)
						melt_kind_flowing =
								nodemgr->get(f.melt_id).liquid_alternative_flowing_id;
					if (melt_kind_flowing == CONTENT_IGNORE)
						melt_kind_flowing = melt_kind;
					if (nb.content == liquid_kind) {
						nb.liquid = 1;
						if (nb.node.param2 & LIQUID_STABLE_MASK)
							continue;
						liquid_levels[i] =
								nb.node.getLevel(nodemgr); // LIQUID_LEVEL_SOURCE;
						nb.infinity = (nb.node.param2 & LIQUID_INFINITY_MASK);
					}
					break;
				case LIQUID_FLOWING:
					// if this node is not (yet) of a liquid type,
					// choose the first liquid type we encounter
					if (liquid_kind_flowing == CONTENT_IGNORE)
						liquid_kind_flowing = nb.content;
					if (liquid_kind == CONTENT_IGNORE)
						liquid_kind = f.liquid_alternative_source_id;
					if (liquid_kind == CONTENT_IGNORE)
						liquid_kind = liquid_kind_flowing;
					if (melt_kind_flowing == CONTENT_IGNORE)
						melt_kind_flowing = f.melt_id;
					if (melt_kind == CONTENT_IGNORE)
						melt_kind = nodemgr->get(f.melt_id).liquid_alternative_source_id;
					if (melt_kind == CONTENT_IGNORE)
						melt_kind = melt_kind_flowing;
					if (nb.content == liquid_kind_flowing) {
						nb.liquid = 1;
						if (nb.node.param2 & LIQUID_STABLE_MASK)
							continue;
						liquid_levels[i] = nb.node.getLevel(nodemgr);
						nb.infinity = (nb.node.param2 & LIQUID_INFINITY_MASK);
					}
					break;
				}

				// DUMP(i, nb.liquid, nb.infinity, (int)liquid_levels[i], f.name);

				// only self, top, bottom swap
				if (f.liquid_type && e <= 2) {
					try {
						nb.weight = ((ItemGroupList)f.groups)["weight"];
						if (e == 1 && neighbors[D_BOTTOM].weight &&
								neighbors[D_SELF].weight > neighbors[D_BOTTOM].weight) {
							cached_map.setNode(
									neighbors[D_SELF].pos, neighbors[D_BOTTOM].node);
							cached_map.setNode(
									neighbors[D_BOTTOM].pos, neighbors[D_SELF].node);
							// must_reflow_second[neighbors[D_SELF].pos] = 1;
							// must_reflow_second[neighbors[D_BOTTOM].pos] = 1;
							//must_reflow_second.emplace_back(neighbors[D_SELF].pos);
							//must_reflow_second.emplace_back(neighbors[D_BOTTOM].pos);
							reflow(neighbors[D_BOTTOM].pos);
							reflow(neighbors[D_BOTTOM].pos);
#if LIQUID_DEBUG
							infostream << "Liquid swap1" << neighbors[D_SELF].pos
									   << nodemgr->get(neighbors[D_SELF].node).name
									   << neighbors[D_SELF].node
									   << " w=" << neighbors[D_SELF].weight << " VS "
									   << neighbors[D_BOTTOM].pos
									   << nodemgr->get(neighbors[D_BOTTOM].node).name
									   << neighbors[D_BOTTOM].node
									   << " w=" << neighbors[D_BOTTOM].weight
									   << std::endl;
#endif
							want_continue = true;
							break;
						}
						if (e == 2 && neighbors[D_SELF].weight &&
								neighbors[D_TOP].weight > neighbors[D_SELF].weight) {
							cached_map.setNode(
									neighbors[D_SELF].pos, neighbors[D_TOP].node);
							cached_map.setNode(
									neighbors[D_TOP].pos, neighbors[D_SELF].node);
							// must_reflow_second[neighbors[D_SELF].pos] = 1;
							// must_reflow_second[neighbors[D_TOP].pos] = 1;
							//must_reflow_second.emplace_back(neighbors[D_SELF].pos);
							//must_reflow_second.emplace_back(neighbors[D_TOP].pos);
							reflow(neighbors[D_SELF].pos);
							reflow(neighbors[D_TOP].pos);
#if LIQUID_DEBUG
							infostream << "Liquid swap2" << neighbors[D_TOP].pos
									   << nodemgr->get(neighbors[D_TOP].node).name
									   << neighbors[D_TOP].node
									   << " w=" << neighbors[D_TOP].weight << " VS "
									   << neighbors[D_SELF].pos
									   << nodemgr->get(neighbors[D_SELF].node).name
									   << neighbors[D_SELF].node
									   << " w=" << neighbors[D_SELF].weight << std::endl;
#endif
							want_continue = true;
							break;
						}
					} catch (const InvalidPositionException &e) {
						verbosestream << "transformLiquidsReal: weight: setNode() failed:"
									  << nb.pos << ":" << e.what() << std::endl;
						// goto NEXT_LIQUID;
					}
				}

				if (nb.liquid) {
					liquid_levels_want[i] = 0;
					++can_liquid;
					if (nb.type == NEIGHBOR_SAME_LEVEL)
						++can_liquid_same_level;
				}
				if (liquid_levels[i] > 0)
					total_level += liquid_levels[i];

#if LIQUID_DEBUG
				infostream << "get node i=" << (int)i << " " << PP(nb.pos)
						   << " c=" << nb.content << " p0=" << (int)nb.node.param0
						   << " p1=" << (int)nb.node.param1
						   << " p2=" << (int)nb.node.param2 << " lt="
						   << f.liquid_type
						   //<< " lk=" << liquid_kind << " lkf=" << liquid_kind_flowing
						   << " l=" << nb.liquid << " inf=" << nb.infinity
						   << " nlevel=" << (int)liquid_levels[i]
						   << " totallevel=" << (int)total_level
						   << " cansame=" << (int)can_liquid_same_level << " Lmax="
						   << (int)nodemgr->get(liquid_kind_flowing).getMaxLevel()
						   << std::endl;
#endif
			}
			if (want_continue)
				continue;

			if (liquid_kind == CONTENT_IGNORE || !neighbors[D_SELF].liquid ||
					total_level <= 0)
				continue;

			int16_t level_max = nodemgr->get(liquid_kind_flowing).getMaxLevel();
			int16_t level_max_compressed =
					nodemgr->get(liquid_kind_flowing).getMaxLevel(1);
			int16_t pressure = liquid_pressure ? ((ItemGroupList)nodemgr->get(liquid_kind)
																 .groups)["pressure"]
											   : 0;
			auto liquid_renewable = nodemgr->get(liquid_kind).liquid_renewable;
#if LIQUID_DEBUG
			s16 total_was = total_level; // debug
#endif
			// viscosity = nodemgr->get(liquid_kind).viscosity;

			s16 level_avg = total_level / can_liquid;
			if (!pressure && level_avg) {
				level_avg = level_max;
			}

#if LIQUID_DEBUG
			if (debug)
				infostream << " go: " << nodemgr->get(liquid_kind).name << " total_level="
						   << (int)total_level
						   //<<" total_was="<<(int)total_was
						   << " level_max=" << (int)level_max
						   << " level_max_compressed=" << (int)level_max_compressed
						   << " level_avg=" << (int)level_avg
						   << " pressure=" << (int)pressure
						   << " can_liquid=" << (int)can_liquid
						   << " can_liquid_same_level=" << (int)can_liquid_same_level
						   << std::endl;
			;
#endif

			// fill bottom block
			if (neighbors[D_BOTTOM].liquid) {
				const auto bpos = getNodeBlockPos(neighbors[D_SELF].pos);
				if (falling[bpos] <= 10 && !liquid_levels[D_BOTTOM] &&
						((ItemGroupList)nodemgr->get(liquid_kind)
										.groups)["falling_node"]) {
					++falling[bpos];
					fall_down = true;
					node_update.emplace(neighbors[D_SELF].pos);
					/*
				if (m_server->getEnv().nodeUpdate(neighbors[D_SELF].pos, 2)) {

					want_continue = true;
					break;
				} else {
					falling[bpos] += 100;
				}
*/
				}

				liquid_levels_want[D_BOTTOM] = level_avg > level_max	 ? level_avg
											   : total_level > level_max ? level_max
																		 : total_level;
				total_level -= liquid_levels_want[D_BOTTOM];

				// if (pressure && total_level && liquid_levels_want[D_BOTTOM] <
				// level_max_compressed) {
				//	++liquid_levels_want[D_BOTTOM];
				//	--total_level;
				// }
			}

			// relax up
			uint16_t relax_want = level_max * can_liquid_same_level;
			if (liquid_renewable && relax &&
					((p0.Y == water_level) ||
							(fast_flood && p0.Y <= water_level && p0.Y > fast_flood)) &&
					level_max > 1 && liquid_levels[D_TOP] == 0 &&
					liquid_levels[D_BOTTOM] >= level_max &&
					total_level >= relax_want - (can_liquid_same_level - relax) &&
					total_level < relax_want && can_liquid_same_level >= relax + 1) {
				regenerated += relax_want - total_level;
#if LIQUID_DEBUG
				infostream << " relax_up: " << " total_level=" << (int)total_level
						   << " to=> " << int(relax_want) << std::endl;
#endif
				total_level = relax_want;
			}

			// prevent lakes in air above unloaded blocks
			if (liquid_levels[D_TOP] == 0 && p0.Y > water_level && level_max > 1 &&
					!neighbors[D_BOTTOM].node && !(loopcount % 3)) {
				--total_level;
#if LIQUID_DEBUG
				infostream << " above unloaded fix: " << " total_level="
						   << (int)total_level << std::endl;
#endif
			}

			// calculate self level 5 blocks
			uint16_t want_level = level_avg > level_max ? level_avg
								  : total_level >= level_max * can_liquid_same_level
										  ? level_max
										  : total_level / can_liquid_same_level;
			total_level -= want_level * can_liquid_same_level;

			/*
				if (pressure && total_level > 0 && neighbors[D_BOTTOM].liquid) { // bottom
		pressure +1
					++liquid_levels_want[D_BOTTOM];
					--total_level;
		#if LIQUID_DEBUG
					infostream << " bottom1 pressure+1: " << " bottom=" <<
		(int)liquid_levels_want[D_BOTTOM] << " total_level=" << (int)total_level <<
		std::endl; #endif
				}
		*/

			// relax down
			if (liquid_renewable && relax && p0.Y == water_level + 1 &&
					liquid_levels[D_TOP] == 0 && (total_level <= 1 || !(loopcount % 2)) &&
					level_max > 1 && liquid_levels[D_BOTTOM] >= level_max &&
					want_level <= 0 && total_level <= (can_liquid_same_level - relax) &&
					can_liquid_same_level >= relax + 1) {
#if LIQUID_DEBUG
				infostream << " relax_down: " << " total_level WAS=" << (int)total_level
						   << " to => 0" << std::endl;
#endif
				regenerated -= total_level;
				total_level = 0;
			}

			for (uint8_t ir = D_SELF; ir < D_TOP; ++ir) { // fill only same level
				uint8_t ii = liquid_random_map[(loopcount + loop_rand + 1) % 4][ir];
				if (!neighbors[ii].liquid)
					continue;
				liquid_levels_want[ii] = want_level;
				// if (viscosity > 1 &&
				// (liquid_levels_want[ii]-liquid_levels[ii]>8-viscosity))
				//  randomly place rest of divide
				if (liquid_levels_want[ii] < level_max && total_level > 0) {
					if (level_max > LIQUID_LEVEL_SOURCE || loopcount % 3 ||
							liquid_levels[ii] <= 0) {
						if (liquid_levels[ii] > liquid_levels_want[ii]) {
							++liquid_levels_want[ii];
							--total_level;
						}
					} else {
						++liquid_levels_want[ii];
						--total_level;
					}
				}
			}

			for (uint8_t ir = D_SELF; ir < D_TOP; ++ir) {
				if (total_level < 1)
					break;
				uint8_t ii = liquid_random_map[(loopcount + loop_rand + 2) % 4][ir];
				if (liquid_levels_want[ii] >= 0 && liquid_levels_want[ii] < level_max) {
					++liquid_levels_want[ii];
					--total_level;
				}
			}

			// fill top block if can
			if (neighbors[D_TOP].liquid && total_level > 0) {
				// infostream<<"compressing to top was="<<liquid_levels_want[D_TOP]<<"
				// add="<<total_level<<std::endl; liquid_levels_want[D_TOP] =
				// total_level>level_max_compressed?level_max_compressed:total_level;
				liquid_levels_want[D_TOP] =
						total_level > level_max ? level_max : total_level;
				total_level -= liquid_levels_want[D_TOP];

				// if (liquid_levels_want[D_TOP] && total_level && pressure) {
				if (total_level > 0 && pressure) {

					/*
								if (total_level > 0 && neighbors[D_BOTTOM].liquid) { //
				   bottom pressure +2
									++liquid_levels_want[D_BOTTOM];
									--total_level;
								}
				*/
					// compressing self level while can
					// for (u16 ir = D_SELF; ir < D_TOP; ++ir) {
					for (uint8_t ir = D_BOTTOM; ir <= D_TOP; ++ir) {
						if (total_level < 1)
							break;
						uint8_t ii =
								liquid_random_map[(loopcount + loop_rand + 3) % 4][ir];
						if (neighbors[ii].liquid &&
								liquid_levels_want[ii] < level_max_compressed) {
							++liquid_levels_want[ii];
							--total_level;
						}
					}

					/*
								if (total_level > 0 && neighbors[D_BOTTOM].liquid) { //
				bottom pressure +2
									++liquid_levels_want[D_BOTTOM];
									--total_level;
				#if LIQUID_DEBUG
							infostream << " bottom2 pressure+1: " << " bottom=" <<
				(int)liquid_levels_want[D_BOTTOM] << " total_level=" << (int)total_level
				<< std::endl; #endif
								}
				*/
				}
			}

			if (pressure) {
				if (neighbors[D_BOTTOM].liquid &&
						liquid_levels_want[D_BOTTOM] < level_max_compressed &&
						liquid_levels_want[D_TOP] > 0) {
					// if (liquid_levels_want[D_BOTTOM] <= liquid_levels_want[D_TOP]) {
					--liquid_levels_want[D_TOP];
					++liquid_levels_want[D_BOTTOM];
#if LIQUID_DEBUG
					infostream << " bottom1 pressure+: " << " bot="
							   << (int)liquid_levels_want[D_BOTTOM]
							   << " slf=" << (int)liquid_levels_want[D_SELF]
							   << " top=" << (int)liquid_levels_want[D_TOP]
							   << " total_level=" << (int)total_level << std::endl;
#endif
					//}
				} else if (neighbors[D_BOTTOM].liquid &&
						   liquid_levels_want[D_BOTTOM] < level_max_compressed &&
						   liquid_levels_want[D_SELF] > level_max) {
					if (liquid_levels_want[D_BOTTOM] <= liquid_levels_want[D_SELF]) {
						--liquid_levels_want[D_SELF];
						++liquid_levels_want[D_BOTTOM];
#if LIQUID_DEBUG
						infostream << " bottom2 pressure+: " << " bot="
								   << (int)liquid_levels_want[D_BOTTOM]
								   << " slf=" << (int)liquid_levels_want[D_SELF]
								   << " top=" << (int)liquid_levels_want[D_TOP]
								   << " total_level=" << (int)total_level << std::endl;
#endif
					}
				} else if (neighbors[D_TOP].liquid &&
						   liquid_levels_want[D_SELF] < level_max_compressed &&
						   liquid_levels_want[D_TOP] > level_max) {
					if (liquid_levels_want[D_SELF] <= liquid_levels_want[D_TOP]) {
						--liquid_levels_want[D_TOP];
						++liquid_levels_want[D_SELF];
#if LIQUID_DEBUG
						infostream << " bottom3 pressure+: " << " bot="
								   << (int)liquid_levels_want[D_BOTTOM]
								   << " slf=" << (int)liquid_levels_want[D_SELF]
								   << " top=" << (int)liquid_levels_want[D_TOP]
								   << " total_level=" << (int)total_level << std::endl;
#endif
					}
				}

				if (liquid_levels_want[D_TOP] > level_max && relax && total_level <= 0 &&
						level_avg > level_max && liquid_levels_want[D_TOP] < level_avg) {
#if LIQUID_DEBUG
					infostream << " top pressure relax: " << " top="
							   << (int)liquid_levels_want[D_TOP] << " to=>" << level_avg
							   << std::endl;
#endif

					// regenerated += level_avg - liquid_levels_want[D_TOP];
					// liquid_levels_want[D_TOP] = level_avg;
					regenerated += 1;
					liquid_levels_want[D_TOP] += 1;
				}
			}

#if LIQUID_DEBUG
			if (total_level > 0)
				infostream << " rest 1: " << " wtop=" << (int)liquid_levels_want[D_TOP]
						   << " total_level=" << (int)total_level << std::endl;
#endif

			if (total_level > 0 && neighbors[D_TOP].liquid &&
					liquid_levels_want[D_TOP] < level_max_compressed) {
				int16_t add =
						(total_level > level_max_compressed - liquid_levels_want[D_TOP])
								? level_max_compressed - liquid_levels_want[D_TOP]
								: total_level;
				liquid_levels_want[D_TOP] += add;
				total_level -= add;
			}

			if (total_level > 0 && neighbors[D_SELF].liquid &&
					liquid_levels_want[D_SELF] <
							level_max_compressed) { // very rare, compressed only
				int16_t add =
						(total_level > level_max_compressed - liquid_levels_want[D_SELF])
								? level_max_compressed - liquid_levels_want[D_SELF]
								: total_level;
#if LIQUID_DEBUG
				if (total_level > 0)
					infostream << " rest 2: " << " wself="
							   << (int)liquid_levels_want[D_SELF]
							   << " total_level=" << (int)total_level
							   << " add=" << (int)add << std::endl;
#endif

				liquid_levels_want[D_SELF] += add;
				total_level -= add;
			}

#if LIQUID_DEBUG
			if (total_level > 0)
				infostream << " rest 3: " << " total_level=" << (int)total_level
						   << std::endl;
#endif

			for (uint8_t ii = 0; ii < 7; ii++) { // infinity and cave flood optimization
				if (neighbors[ii].infinity &&
						liquid_levels_want[ii] < liquid_levels[ii]) {
#if LIQUID_DEBUG
					infostream << " infinity: was=" << (int)ii << " = "
							   << (int)liquid_levels_want[ii]
							   << "  to=" << (int)liquid_levels[ii] << std::endl;
#endif

					regenerated += liquid_levels[ii] - liquid_levels_want[ii];
					liquid_levels_want[ii] = liquid_levels[ii];
				} else if (liquid_levels_want[ii] >= 0 &&
						   liquid_levels_want[ii] < level_max && level_max > 1 &&
						   fast_flood && p0.Y < water_level && p0.Y > fast_flood &&
						   initial_size >= 1000 && ii != D_TOP &&
						   want_level >= level_max / 4 && can_liquid_same_level >= 5 &&
						   liquid_levels[D_TOP] >= level_max) {
#if LIQUID_DEBUG
					infostream << " flood_fast: was=" << (int)ii << " = "
							   << (int)liquid_levels_want[ii] << "  to=" << (int)level_max
							   << std::endl;
#endif
					regenerated += level_max - liquid_levels_want[ii];
					liquid_levels_want[ii] = level_max;
				}
			}

#if LIQUID_DEBUG
			if (total_level != 0) //|| flowed != volume)
				infostream << " AFTER err level="
						   << (int)total_level
						   //<< " flowed="<<flowed<< " volume=" << volume
						   << " max=" << (int)level_max << " wantsame=" << (int)want_level
						   << " top=" << (int)liquid_levels_want[D_TOP]
						   << " topwas=" << (int)liquid_levels[D_TOP]
						   << " bot=" << (int)liquid_levels_want[D_BOTTOM]
						   << " botwas=" << (int)liquid_levels[D_BOTTOM] << std::endl;

			s16 flowed = 0; // for debug
#endif

#if LIQUID_DEBUG
			if (debug)
				infostream << " dpress=" << " bot=" << (int)liquid_levels_want[D_BOTTOM]
						   << " slf=" << (int)liquid_levels_want[D_SELF]
						   << " top=" << (int)liquid_levels_want[D_TOP] << std::endl;
#endif

			for (int8_t r = D_BOTTOM; r <= D_TOP; ++r) {
				uint8_t i = liquid_random_map[(loopcount + loop_rand + 4) % 4][r];
				if (liquid_levels_want[i] < 0 || !neighbors[i].liquid)
					continue;

#if LIQUID_DEBUG
				if (debug)
					infostream << " set=" << (int)i << " " << neighbors[i].pos
							   << " want=" << (int)liquid_levels_want[i]
							   << " was=" << (int)liquid_levels[i] << std::endl;
#endif

				/* disabled because brokes constant volume of lava
			u8 viscosity = nodemgr->get(liquid_kind).liquid_viscosity;
			if (viscosity > 1 && liquid_levels_want[i] != liquid_levels[i]) {
				// amount to gain, limited by viscosity
				// must be at least 1 in absolute value
				s8 level_inc = liquid_levels_want[i] - liquid_levels[i];
				if (level_inc < -viscosity || level_inc > viscosity)
					new_node_level = liquid_levels[i] + level_inc/viscosity;
				else if (level_inc < 0)
					new_node_level = liquid_levels[i] - 1;
				else if (level_inc > 0)
					new_node_level = liquid_levels[i] + 1;
			} else {
			*/

				// last level must flow down on stairs
				if (liquid_levels_want[i] != liquid_levels[i] &&
						liquid_levels[D_TOP] <= 0 &&
						(!neighbors[D_BOTTOM].liquid || level_max == 1) &&
						liquid_levels_want[i] >= 1 && liquid_levels_want[i] <= 2) {
					for (uint8_t ir = D_SELF + 1; ir < D_TOP; ++ir) { // only same level
						uint8_t ii =
								liquid_random_map[(loopcount + loop_rand + 5) % 4][ir];
						if (neighbors[ii].liquid)
							//must_reflow_second.emplace_back(neighbors[i].pos + liquid_flow_dirs[ii]);
							reflow(neighbors[i].pos + liquid_flow_dirs[ii]);
						// must_reflow_second[neighbors[i].pos + liquid_flow_dirs[ii]] = 1;
					}
				}

#if LIQUID_DEBUG
				if (liquid_levels_want[i] > 0)
					flowed += liquid_levels_want[i];
#endif
				if (liquid_levels[i] == liquid_levels_want[i]) {
					continue;
				}

				if (neighbors[i].drop) { // && level_max > 1 && total_level >= level_max - 1
					node_drop.emplace(neighbors[i].pos);
				}

				neighbors[i].node.setContent(liquid_kind_flowing);
				neighbors[i].node.setLevel(nodemgr, liquid_levels_want[i], 1);

				try {
					cached_map.setNode(neighbors[i].pos, neighbors[i].node);
				} catch (const InvalidPositionException &e) {
					verbosestream << "transformLiquidsReal: setNode() failed:"
								  << neighbors[i].pos << ":" << e.what() << std::endl;
				}

				// If node emits light, MapBlock requires lighting update
				// or if node removed
				if (!(bool)liquid_levels[i] != !(bool)liquid_levels_want[i]) {
					v3bpos_t blockpos = getNodeBlockPos(neighbors[i].pos);
					blocks_lighting_update.emplace(blockpos);
				}
				// fmtodo: make here random %2 or..
				if (total_level < level_max * can_liquid) {
					reflow(neighbors[i].pos);
					//must_reflow.emplace_back(neighbors[i].pos);
				}
			}

			if (fall_down) {
				//? m_server->getEnv().nodeUpdate(neighbors[D_BOTTOM].pos, 1);
			}

#if LIQUID_DEBUG
			// if (total_was != flowed) {
			if (total_was > flowed) {
				infostream << " volume changed!  flowed=" << flowed
						   << " total_was=" << total_was << " want_level=" << want_level;
				for (uint8_t rr = 0; rr <= 6; rr++) {
					infostream << "  i=" << (int)rr << ",b" << (int)liquid_levels[rr]
							   << ",a" << (int)liquid_levels_want[rr];
				}
				infostream << std::endl;
			}
#endif
			/* //for better relax  only same level
		if (changed)  for (u16 ii = D_SELF + 1; ii < D_TOP; ++ii) {
			if (!neighbors[ii].l) continue;
			must_reflow.push_back(p0 + dirs[ii]);
		}*/
			// g_profiler->graphAdd("liquids", 1);
		}
	}
	m_transforming_liquid_local.clear();

	//size_t ret = loopcount >= initial_size ? 0 : transforming_liquid_size();
	//if (ret || loopcount > m_liquid_step_flow)
	if (porting::getTimeMs() > end_ms)
		m_liquid_step_flow +=
				(m_liquid_step_flow > loopcount ? -1 : 1) * (int)loopcount / 10;
	/*
	if (loopcount)
		infostream<<"Map::transformLiquidsReal(): loopcount="<<loopcount<<"
	initial_size="<<initial_size
		<<" avgflow="<<m_liquid_step_flow
		<<" reflow="<<must_reflow.size()
		<<" reflow_second="<<must_reflow_second.size()
		<<" reflow_third="<<must_reflow_third.size()
		<<" queue="<< transforming_liquid_size()
		<<" per="<< porting::getTimeMs() - (end_ms - max_cycle_ms)
		<<" ret="<<ret<<std::endl;
	*/

	{
		// TimeTaker timer13("transformLiquidsReal() reflow");
		// const auto lock = m_transforming_liquid.lock_unique_rec();
		//std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);

		// m_transforming_liquid.insert(must_reflow.begin(), must_reflow.end());

		//DUMP(fast_reflow.size(), must_reflow.size(), must_reflow_second.size());
		std::unordered_set<v3pos_t> uniq;
		m_transforming_liquid_local.reserve(
				must_reflow.size() + must_reflow_second.size() + fast_reflow.size() * 4);
		for (const auto &[bp, list] : fast_reflow) {
			for (const auto &p : list) {
				if (!uniq.contains(p))
					m_transforming_liquid_local.emplace_back(p);
				uniq.emplace(p);
			}
		}

		for (const auto &p : must_reflow) {
			if (!uniq.contains(p))
				m_transforming_liquid_local.emplace_back(p);
			uniq.emplace(p);
		}
		for (const auto &p : must_reflow_second) {
			if (!uniq.contains(p))
				m_transforming_liquid_local.emplace_back(p);
			uniq.emplace(p);
		}

		m_transforming_liquid_local_size = m_transforming_liquid_local.size();
	}

	for (const auto &pos : node_drop) {
		m_server->getEnv().getScriptIface()->postponed.emplace_back([=]() {
    		m_server->getEnv().getScriptIface()->node_drop(pos, 2);
		});
	}

	for (const auto &pos : node_update) {
		m_server->getEnv().nodeUpdate(pos, 2);
	}

	for (const auto &blockpos : blocks_lighting_update) {
		auto block = getBlockNoCreateNoEx(blockpos, true); // remove true if light bugs
		if (!block)
			continue;
		block->setLightingComplete(0);
		// modified_blocks[blockpos] = block;
		// if(!nodemgr->get(neighbors[i].node).light_propagates ||
		// nodemgr->get(neighbors[i].node).light_source) // better to update always
		//	lighting_modified_blocks.set_try(block->getPos(), block);
	}

	g_profiler->avg("Server: liquids real processed", loopcount);
	if (regenerated)
		g_profiler->avg("Server: liquids regenerated", regenerated);
	/*
		if (loopcount < initial_size)
			g_profiler->add("Server: liquids queue", initial_size);
	*/
	// g_profiler->avg("Server: liquids queue internal", m_transforming_liquid_local_size);

	return loopcount;
}
