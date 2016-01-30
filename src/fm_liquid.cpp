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

#include "irr_v3d.h"
#include "map.h"
#include "gamedef.h"
#include "settings.h"
#include "nodedef.h"
#include "log_types.h"
#include "server.h"
#include "scripting_game.h"
#include "profiler.h"
#include "emerge.h"

#define LIQUID_DEBUG 0

enum NeighborType {
	NEIGHBOR_UPPER,
	NEIGHBOR_SAME_LEVEL,
	NEIGHBOR_LOWER
};

struct NodeNeighbor {
	MapNode node;
	NeighborType type;
	v3POS pos;
	content_t content;
	bool liquid; //can liquid
	bool infinity;
	int weight;
	int drop; //drop by liquid
};

const v3POS liquid_flow_dirs[7] = {
	// +right, +top, +back
	v3POS( 0, -1, 0), // 0 bottom
	v3POS( 0, 0, 0), // 1 self
	v3POS( 0, 0, 1), // 2 back
	v3POS( 0, 0, -1), // 3 front
	v3POS( 1, 0, 0), // 4 right
	v3POS(-1, 0, 0), // 5 left
	v3POS( 0, 1, 0)  // 6 top
};

// when looking around we must first check self node for correct type definitions
const s8 liquid_explore_map[7] = {1, 0, 6, 2, 3, 4, 5};
const s8 liquid_random_map[4][7] = {
	{0, 1, 2, 3, 4, 5, 6},
	{0, 1, 4, 3, 5, 2, 6},
	{0, 1, 3, 5, 4, 2, 6},
	{0, 1, 5, 3, 2, 4, 6}
};

#define D_BOTTOM 0
#define D_TOP 6
#define D_SELF 1

u32 Map::transformLiquidsReal(Server *m_server, unsigned int max_cycle_ms) {

	INodeDefManager *nodemgr = m_gamedef->ndef();

	DSTACK(FUNCTION_NAME);
	//TimeTaker timer("transformLiquidsReal()");
	u32 loopcount = 0;
	u32 initial_size = transforming_liquid_size();

	s32 regenerated = 0;

#if LIQUID_DEBUG
	bool debug = 1;
#endif

	u8 relax = g_settings->getS16("liquid_relax");
	static bool fast_flood = g_settings->getS16("liquid_fast_flood");
	static int water_level = g_settings->getS16("water_level");
	s16 liquid_pressure = m_server->m_emerge->params.liquid_pressure;
	//g_settings->getS16NoEx("liquid_pressure", liquid_pressure);

	// list of nodes that due to viscosity have not reached their max level height
	//unordered_map_v3POS<bool> must_reflow, must_reflow_second, must_reflow_third;
	std::list<v3POS> must_reflow, must_reflow_second, must_reflow_third;
	// List of MapBlocks that will require a lighting update (due to lava)
	int falling = 0;
	u16 loop_rand = myrand();

	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

NEXT_LIQUID:
	;
	while (transforming_liquid_size() > 0) {
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size * 2 || porting::getTimeMs() > end_ms)
			break;
		++loopcount;
		/*
			Get a queued transforming liquid node
		*/
		v3POS p0;
		{
			//MutexAutoLock lock(m_transforming_liquid_mutex);
			p0 = transforming_liquid_pop();
		}
		s16 total_level = 0;
		//u16 level_max = 0;
		// surrounding flowing liquid nodes
		NodeNeighbor neighbors[7] = { { } };
		// current level of every block
		s8 liquid_levels[7] = { -1, -1, -1, -1, -1, -1, -1};
		// target levels
		s8 liquid_levels_want[7] = { -1, -1, -1, -1, -1, -1, -1};
		s8 can_liquid_same_level = 0;
		s8 can_liquid = 0;
		// warning! when MINETEST_PROTO enabled - CONTENT_IGNORE != 0
		content_t liquid_kind = CONTENT_IGNORE;
		content_t liquid_kind_flowing = CONTENT_IGNORE;
		content_t melt_kind = CONTENT_IGNORE;
		content_t melt_kind_flowing = CONTENT_IGNORE;
		//s8 viscosity = 0;

		bool fall_down = false;
		/*
			Collect information about the environment, start from self
		 */
		for (u8 e = 0; e < 7; e++) {
			u8 i = liquid_explore_map[e];
			NodeNeighbor & nb = neighbors[i];
			nb.pos = p0 + liquid_flow_dirs[i];
			nb.node = getNodeNoEx(neighbors[i].pos);
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
				//if (i == D_SELF && (loopcount % 2) && initial_size < m_liquid_step_flow * 3)
				//	must_reflow_third[nb.pos] = 1;
				//	must_reflow_third.push_back(nb.pos);
				continue;
			}

			const auto & f = nodemgr->get(nb.content);
			switch (f.liquid_type) {
			case LIQUID_NONE:
				if (nb.content == CONTENT_AIR) {
					liquid_levels[i] = 0;
					nb.liquid = 1;
				}
				//TODO: if (nb.content == CONTENT_AIR || nodemgr->get(nb.node).buildable_to && !nodemgr->get(nb.node).walkable) { // need lua drop api for drop torches
				else if (	melt_kind_flowing != CONTENT_IGNORE &&
				            nb.content == melt_kind_flowing &&
				            nb.type != NEIGHBOR_UPPER &&
				            !(loopcount % 2)) {
					u8 melt_max_level = nb.node.getMaxLevel(nodemgr);
					u8 my_max_level = MapNode(liquid_kind_flowing).getMaxLevel(nodemgr);
					liquid_levels[i] = ((float)my_max_level / (melt_max_level ? melt_max_level : my_max_level)) * nb.node.getLevel(nodemgr);
					if (liquid_levels[i])
						nb.liquid = 1;
				} else if (	melt_kind != CONTENT_IGNORE &&
				            nb.content == melt_kind &&
				            nb.type != NEIGHBOR_UPPER &&
				            !(loopcount % 8)) {
					liquid_levels[i] = nodemgr->get(liquid_kind_flowing).getMaxLevel();
					if (liquid_levels[i])
						nb.liquid = 1;
				} else {
					int drop = ((ItemGroupList) f.groups)["drop_by_liquid"];
					if (drop && !(loopcount % drop) ) {
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
					liquid_kind_flowing = nodemgr->getId(
					                          f.liquid_alternative_flowing);
				if (liquid_kind == CONTENT_IGNORE)
					liquid_kind = nb.content;
				if (liquid_kind_flowing == CONTENT_IGNORE)
					liquid_kind_flowing = liquid_kind;
				if (melt_kind == CONTENT_IGNORE)
					melt_kind = nodemgr->getId(f.melt);
				if (melt_kind_flowing == CONTENT_IGNORE)
					melt_kind_flowing =
					    nodemgr->getId(
					        nodemgr->get(nodemgr->getId(f.melt)
					                    ).liquid_alternative_flowing);
				if (melt_kind_flowing == CONTENT_IGNORE)
					melt_kind_flowing = melt_kind;
				if (nb.content == liquid_kind) {
					if (nb.node.param2 & LIQUID_STABLE_MASK)
						continue;
					liquid_levels[i] = nb.node.getLevel(nodemgr); //LIQUID_LEVEL_SOURCE;
					nb.liquid = 1;
					nb.infinity = (nb.node.param2 & LIQUID_INFINITY_MASK);
				}
				break;
			case LIQUID_FLOWING:
				// if this node is not (yet) of a liquid type,
				// choose the first liquid type we encounter
				if (liquid_kind_flowing == CONTENT_IGNORE)
					liquid_kind_flowing = nb.content;
				if (liquid_kind == CONTENT_IGNORE)
					liquid_kind = nodemgr->getId(
					                  f.liquid_alternative_source);
				if (liquid_kind == CONTENT_IGNORE)
					liquid_kind = liquid_kind_flowing;
				if (melt_kind_flowing == CONTENT_IGNORE)
					melt_kind_flowing = nodemgr->getId(f.melt);
				if (melt_kind == CONTENT_IGNORE)
					melt_kind = nodemgr->getId(nodemgr->get(nodemgr->getId(
					        f.melt)).liquid_alternative_source);
				if (melt_kind == CONTENT_IGNORE)
					melt_kind = melt_kind_flowing;
				if (nb.content == liquid_kind_flowing) {
					if (nb.node.param2 & LIQUID_STABLE_MASK)
						continue;
					liquid_levels[i] = nb.node.getLevel(nodemgr);
					nb.liquid = 1;
					nb.infinity = (nb.node.param2 & LIQUID_INFINITY_MASK);
				}
				break;
			}

			// only self, top, bottom swap
			if (f.liquid_type && e <= 2) {
				try {
					nb.weight = ((ItemGroupList) f.groups)["weight"];
					if (e == 1 && neighbors[D_BOTTOM].weight && neighbors[D_SELF].weight > neighbors[D_BOTTOM].weight) {
						setNode(neighbors[D_SELF].pos, neighbors[D_BOTTOM].node);
						setNode(neighbors[D_BOTTOM].pos, neighbors[D_SELF].node);
						//must_reflow_second[neighbors[D_SELF].pos] = 1;
						//must_reflow_second[neighbors[D_BOTTOM].pos] = 1;
						must_reflow_second.push_back(neighbors[D_SELF].pos);
						must_reflow_second.push_back(neighbors[D_BOTTOM].pos);
#if LIQUID_DEBUG
						infostream << "Liquid swap1" << neighbors[D_SELF].pos << nodemgr->get(neighbors[D_SELF].node).name << neighbors[D_SELF].node << " w=" << neighbors[D_SELF].weight << " VS " << neighbors[D_BOTTOM].pos << nodemgr->get(neighbors[D_BOTTOM].node).name << neighbors[D_BOTTOM].node << " w=" << neighbors[D_BOTTOM].weight << std::endl;
#endif
						goto NEXT_LIQUID;
					}
					if (e == 2 && neighbors[D_SELF].weight && neighbors[D_TOP].weight > neighbors[D_SELF].weight) {
						setNode(neighbors[D_SELF].pos, neighbors[D_TOP].node);
						setNode(neighbors[D_TOP].pos, neighbors[D_SELF].node);
						//must_reflow_second[neighbors[D_SELF].pos] = 1;
						//must_reflow_second[neighbors[D_TOP].pos] = 1;
						must_reflow_second.push_back(neighbors[D_SELF].pos);
						must_reflow_second.push_back(neighbors[D_TOP].pos);
#if LIQUID_DEBUG
						infostream << "Liquid swap2" << neighbors[D_TOP].pos << nodemgr->get(neighbors[D_TOP].node).name << neighbors[D_TOP].node  << " w=" << neighbors[D_TOP].weight << " VS " << neighbors[D_SELF].pos << nodemgr->get(neighbors[D_SELF].node).name << neighbors[D_SELF].node << " w=" << neighbors[D_SELF].weight << std::endl;
#endif
						goto NEXT_LIQUID;
					}
				} catch(InvalidPositionException &e) {
					verbosestream << "transformLiquidsReal: weight: setNode() failed:" << nb.pos << ":" << e.what() << std::endl;
					//goto NEXT_LIQUID;
				}
			}

			if (nb.liquid) {
				liquid_levels_want[i] = 0;
				++can_liquid;
				if(nb.type == NEIGHBOR_SAME_LEVEL)
					++can_liquid_same_level;
			}
			if (liquid_levels[i] > 0)
				total_level += liquid_levels[i];

#if LIQUID_DEBUG
			infostream << "get node i=" << (int)i << " " << PP(nb.pos) << " c="
			           << nb.content << " p0=" << (int)nb.node.param0 << " p1="
			           << (int)nb.node.param1 << " p2=" << (int)nb.node.param2 << " lt="
			           << f.liquid_type
			           //<< " lk=" << liquid_kind << " lkf=" << liquid_kind_flowing
			           << " l=" << nb.liquid	<< " inf=" << nb.infinity << " nlevel=" << (int)liquid_levels[i]
			           << " totallevel=" << (int)total_level << " cansame="
			           << (int)can_liquid_same_level << " Lmax=" << (int)nodemgr->get(liquid_kind_flowing).getMaxLevel() << std::endl;
#endif
		}

		if (liquid_kind == CONTENT_IGNORE || !neighbors[D_SELF].liquid || total_level <= 0)
			continue;

		s16 level_max = nodemgr->get(liquid_kind_flowing).getMaxLevel();
		s16 level_max_compressed = nodemgr->get(liquid_kind_flowing).getMaxLevel(1);
		s16 pressure = liquid_pressure ? ((ItemGroupList) nodemgr->get(liquid_kind).groups)["pressure"] : 0;
		auto liquid_renewable = nodemgr->get(liquid_kind).liquid_renewable;
#if LIQUID_DEBUG
		s16 total_was = total_level; //debug
#endif
		//viscosity = nodemgr->get(liquid_kind).viscosity;

		s16 level_avg = total_level / can_liquid;
		if (!pressure && level_avg) {
			level_avg = level_max;
		}

#if LIQUID_DEBUG
		if (debug)
			infostream << " go: "
			           << nodemgr->get(liquid_kind).name
			           << " total_level=" << (int)total_level
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
			if (falling++ < 100 && !liquid_levels[D_BOTTOM] && ((ItemGroupList) nodemgr->get(liquid_kind).groups)["falling_node"]) {
				fall_down = true;
				//m_server->getEnv().nodeUpdate(neighbors[D_SELF].pos, 2);
				//goto NEXT_LIQUID;
			}

			liquid_levels_want[D_BOTTOM] = level_avg > level_max ? level_avg : total_level > level_max ? level_max : total_level;
			total_level -= liquid_levels_want[D_BOTTOM];

			//if (pressure && total_level && liquid_levels_want[D_BOTTOM] < level_max_compressed) {
			//	++liquid_levels_want[D_BOTTOM];
			//	--total_level;
			//}
		}

		//relax up
		u16 relax_want = level_max * can_liquid_same_level;
		if (	liquid_renewable &&
		        relax &&
		        ((p0.Y == water_level) || (fast_flood && p0.Y <= water_level)) &&
		        level_max > 1 &&
		        liquid_levels[D_TOP] == 0 &&
		        liquid_levels[D_BOTTOM] >= level_max &&
		        total_level >= relax_want - (can_liquid_same_level - relax) &&
		        total_level < relax_want &&
		        can_liquid_same_level >= relax + 1) {
			regenerated += relax_want - total_level;
#if LIQUID_DEBUG
			infostream << " relax_up: " << " total_level=" << (int)total_level << " to=> " << int(relax_want) << std::endl;
#endif
			total_level = relax_want;
		}

		// prevent lakes in air above unloaded blocks
		if (	liquid_levels[D_TOP] == 0 &&
		        p0.Y > water_level &&
		        level_max > 1 &&
		        !neighbors[D_BOTTOM].node &&
		        !(loopcount % 3)) {
			--total_level;
#if LIQUID_DEBUG
			infostream << " above unloaded fix: " << " total_level=" << (int)total_level << std::endl;
#endif
		}

		// calculate self level 5 blocks
		u16 want_level = level_avg > level_max ? level_avg :
		                 total_level >= level_max * can_liquid_same_level
		                 ? level_max
		                 : total_level / can_liquid_same_level;
		total_level -= want_level * can_liquid_same_level;

		/*
				if (pressure && total_level > 0 && neighbors[D_BOTTOM].liquid) { // bottom pressure +1
					++liquid_levels_want[D_BOTTOM];
					--total_level;
		#if LIQUID_DEBUG
					infostream << " bottom1 pressure+1: " << " bottom=" << (int)liquid_levels_want[D_BOTTOM] << " total_level=" << (int)total_level << std::endl;
		#endif
				}
		*/

		//relax down
		if (	liquid_renewable &&
		        relax &&
		        p0.Y == water_level + 1 &&
		        liquid_levels[D_TOP] == 0 &&
		        (total_level <= 1 || !(loopcount % 2)) &&
		        level_max > 1 &&
		        liquid_levels[D_BOTTOM] >= level_max &&
		        want_level <= 0 &&
		        total_level <= (can_liquid_same_level - relax) &&
		        can_liquid_same_level >= relax + 1) {
#if LIQUID_DEBUG
			infostream << " relax_down: " << " total_level WAS=" << (int)total_level << " to => 0" << std::endl;
#endif
			regenerated -= total_level;
			total_level = 0;
		}

		for (u16 ir = D_SELF; ir < D_TOP; ++ir) { // fill only same level
			u16 ii = liquid_random_map[(loopcount + loop_rand + 1) % 4][ir];
			if (!neighbors[ii].liquid)
				continue;
			liquid_levels_want[ii] = want_level;
			//if (viscosity > 1 && (liquid_levels_want[ii]-liquid_levels[ii]>8-viscosity))
			// randomly place rest of divide
			if (liquid_levels_want[ii] < level_max && total_level > 0) {
				if (level_max > LIQUID_LEVEL_SOURCE || loopcount % 3 || liquid_levels[ii] <= 0) {
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

		for (u16 ir = D_SELF; ir < D_TOP; ++ir) {
			if (total_level < 1)
				break;
			u16 ii = liquid_random_map[(loopcount + loop_rand + 2) % 4][ir];
			if (liquid_levels_want[ii] >= 0 &&
			        liquid_levels_want[ii] < level_max) {
				++liquid_levels_want[ii];
				--total_level;
			}
		}

		// fill top block if can
		if (neighbors[D_TOP].liquid && total_level > 0) {
			//infostream<<"compressing to top was="<<liquid_levels_want[D_TOP]<<" add="<<total_level<<std::endl;
			//liquid_levels_want[D_TOP] = total_level>level_max_compressed?level_max_compressed:total_level;
			liquid_levels_want[D_TOP] = total_level > level_max ? level_max : total_level;
			total_level -= liquid_levels_want[D_TOP];

			//if (liquid_levels_want[D_TOP] && total_level && pressure) {
			if (total_level > 0 && pressure) {

				/*
								if (total_level > 0 && neighbors[D_BOTTOM].liquid) { // bottom pressure +2
									++liquid_levels_want[D_BOTTOM];
									--total_level;
								}
				*/
				//compressing self level while can
				//for (u16 ir = D_SELF; ir < D_TOP; ++ir) {
				for (u16 ir = D_BOTTOM; ir <= D_TOP; ++ir) {
					if (total_level < 1)
						break;
					u16 ii = liquid_random_map[(loopcount + loop_rand + 3) % 4][ir];
					if (neighbors[ii].liquid &&
					        liquid_levels_want[ii] < level_max_compressed) {
						++liquid_levels_want[ii];
						--total_level;
					}
				}

				/*
								if (total_level > 0 && neighbors[D_BOTTOM].liquid) { // bottom pressure +2
									++liquid_levels_want[D_BOTTOM];
									--total_level;
				#if LIQUID_DEBUG
							infostream << " bottom2 pressure+1: " << " bottom=" << (int)liquid_levels_want[D_BOTTOM] << " total_level=" << (int)total_level << std::endl;
				#endif
								}
				*/
			}
		}

		if (pressure) {
			if (neighbors[D_BOTTOM].liquid &&
			        liquid_levels_want[D_BOTTOM] < level_max_compressed &&
			        liquid_levels_want[D_TOP] > 0
			   ) {
				//if (liquid_levels_want[D_BOTTOM] <= liquid_levels_want[D_TOP]) {
				--liquid_levels_want[D_TOP];
				++liquid_levels_want[D_BOTTOM];
#if LIQUID_DEBUG
				infostream << " bottom1 pressure+: " << " bot=" << (int)liquid_levels_want[D_BOTTOM] << " slf=" << (int)liquid_levels_want[D_SELF] << " top=" << (int)liquid_levels_want[D_TOP] << " total_level=" << (int)total_level << std::endl;
#endif
				//}
			} else if (
			    neighbors[D_BOTTOM].liquid &&
			    liquid_levels_want[D_BOTTOM] < level_max_compressed &&
			    liquid_levels_want[D_SELF] > level_max
			) {
				if (liquid_levels_want[D_BOTTOM] <= liquid_levels_want[D_SELF]) {
					--liquid_levels_want[D_SELF];
					++liquid_levels_want[D_BOTTOM];
#if LIQUID_DEBUG
					infostream << " bottom2 pressure+: " << " bot=" << (int)liquid_levels_want[D_BOTTOM] << " slf=" << (int)liquid_levels_want[D_SELF] << " top=" << (int)liquid_levels_want[D_TOP] << " total_level=" << (int)total_level << std::endl;
#endif
				}
			} else if (
			    neighbors[D_TOP].liquid &&
			    liquid_levels_want[D_SELF] < level_max_compressed &&
			    liquid_levels_want[D_TOP] > level_max
			) {
				if (liquid_levels_want[D_SELF] <= liquid_levels_want[D_TOP]) {
					--liquid_levels_want[D_TOP];
					++liquid_levels_want[D_SELF];
#if LIQUID_DEBUG
					infostream << " bottom3 pressure+: " << " bot=" << (int)liquid_levels_want[D_BOTTOM] << " slf=" << (int)liquid_levels_want[D_SELF] << " top=" << (int)liquid_levels_want[D_TOP] << " total_level=" << (int)total_level << std::endl;
#endif
				}
			}

			if (liquid_levels_want[D_TOP] > level_max && relax && total_level <= 0 && level_avg > level_max && liquid_levels_want[D_TOP] < level_avg) {
#if LIQUID_DEBUG
				infostream << " top pressure relax: " << " top=" << (int)liquid_levels_want[D_TOP] << " to=>" << level_avg << std::endl;
#endif

				//regenerated += level_avg - liquid_levels_want[D_TOP];
				//liquid_levels_want[D_TOP] = level_avg;
				regenerated += 1 ;
				liquid_levels_want[D_TOP] += 1;
			}
		}


#if LIQUID_DEBUG
		if (total_level > 0)
			infostream << " rest 1: "
			           << " wtop=" << (int)liquid_levels_want[D_TOP]
			           << " total_level=" << (int)total_level << std::endl;
#endif

		if (total_level > 0 && neighbors[D_TOP].liquid && liquid_levels_want[D_TOP] < level_max_compressed) {
			s16 add = (total_level > level_max_compressed - liquid_levels_want[D_TOP]) ? level_max_compressed - liquid_levels_want[D_TOP] : total_level;
			liquid_levels_want[D_TOP] += add;
			total_level -= add;
		}


		if (total_level > 0 && neighbors[D_SELF].liquid && liquid_levels_want[D_SELF] < level_max_compressed) { // very rare, compressed only
			s16 add = (total_level > level_max_compressed - liquid_levels_want[D_SELF]) ? level_max_compressed - liquid_levels_want[D_SELF] : total_level;
#if LIQUID_DEBUG
			if (total_level > 0)
				infostream << " rest 2: "
				           << " wself=" << (int)liquid_levels_want[D_SELF]
				           << " total_level=" << (int)total_level
				           << " add=" << (int)add
				           << std::endl;
#endif

			liquid_levels_want[D_SELF] += add;
			total_level -= add;
		}


#if LIQUID_DEBUG
		if (total_level > 0)
			infostream << " rest 3: "
			           << " total_level=" << (int)total_level << std::endl;
#endif

		for (u16 ii = 0; ii < 7; ii++) { // infinity and cave flood optimization
			if (neighbors[ii].infinity && liquid_levels_want[ii] < liquid_levels[ii]) {
#if LIQUID_DEBUG
				infostream << " infinity: was=" << (int)ii << " = "
				           << (int)liquid_levels_want[ii] << "  to=" << (int)liquid_levels[ii] << std::endl;
#endif

				regenerated += liquid_levels[ii] - liquid_levels_want[ii];
				liquid_levels_want[ii] = liquid_levels[ii];
			} else if ( liquid_levels_want[ii] >= 0	&&
			            liquid_levels_want[ii] < level_max &&
			            level_max > 1					&&
			            fast_flood					&&
			            p0.Y < water_level			&&
			            initial_size >= 1000			&&
			            ii != D_TOP					&&
			            want_level >= level_max / 4	&&
			            can_liquid_same_level >= 5	&&
			            liquid_levels[D_TOP] >= level_max) {
#if LIQUID_DEBUG
				infostream << " flood_fast: was=" << (int)ii << " = "
				           << (int)liquid_levels_want[ii] << "  to=" << (int)level_max << std::endl;
#endif
				regenerated += level_max - liquid_levels_want[ii];
				liquid_levels_want[ii] = level_max;
			}
		}

#if LIQUID_DEBUG
		if (total_level != 0) //|| flowed != volume)
			infostream << " AFTER err level=" << (int)total_level
			           //<< " flowed="<<flowed<< " volume=" << volume
			           << " max=" << (int)level_max
			           << " wantsame=" << (int)want_level << " top="
			           << (int)liquid_levels_want[D_TOP] << " topwas="
			           << (int)liquid_levels[D_TOP]
			           << " bot=" << (int)liquid_levels_want[D_BOTTOM]
			           << " botwas=" << (int)liquid_levels[D_BOTTOM]
			           << std::endl;

		s16 flowed = 0; // for debug
#endif

#if LIQUID_DEBUG
		if (debug) infostream << " dpress=" << " bot=" << (int)liquid_levels_want[D_BOTTOM] << " slf=" << (int)liquid_levels_want[D_SELF] << " top=" << (int)liquid_levels_want[D_TOP] << std::endl;
#endif

		for (u16 r = 0; r < 7; r++) {
			u16 i = liquid_random_map[(loopcount + loop_rand + 4) % 4][r];
			if (liquid_levels_want[i] < 0 || !neighbors[i].liquid)
				continue;

#if LIQUID_DEBUG
			if (debug) infostream << " set=" << i << " " << neighbors[i].pos << " want=" << (int)liquid_levels_want[i] << " was=" << (int) liquid_levels[i] << std::endl;
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
			        liquid_levels[D_TOP] <= 0 && (!neighbors[D_BOTTOM].liquid || level_max == 1) &&
			        liquid_levels_want[i] >= 1 && liquid_levels_want[i] <= 2) {
				for (u16 ir = D_SELF + 1; ir < D_TOP; ++ir) { // only same level
					u16 ii = liquid_random_map[(loopcount + loop_rand + 5) % 4][ir];
					if (neighbors[ii].liquid)
						must_reflow_second.push_back(neighbors[i].pos + liquid_flow_dirs[ii]);
					//must_reflow_second[neighbors[i].pos + liquid_flow_dirs[ii]] = 1;
				}
			}

#if LIQUID_DEBUG
			if (liquid_levels_want[i] > 0)
				flowed += liquid_levels_want[i];
#endif
			if (liquid_levels[i] == liquid_levels_want[i]) {
				continue;
			}

			if (neighbors[i].drop) {// && level_max > 1 && total_level >= level_max - 1
				m_server->getEnv().getScriptIface()->node_drop(neighbors[i].pos, 2);
			}

			neighbors[i].node.setContent(liquid_kind_flowing);
			neighbors[i].node.setLevel(nodemgr, liquid_levels_want[i], 1);

			try {
				setNode(neighbors[i].pos, neighbors[i].node);
			} catch(InvalidPositionException &e) {
				verbosestream << "transformLiquidsReal: setNode() failed:" << neighbors[i].pos << ":" << e.what() << std::endl;
			}

			// If node emits light, MapBlock requires lighting update
			// or if node removed
			v3POS blockpos = getNodeBlockPos(neighbors[i].pos);
			MapBlock *block = getBlockNoCreateNoEx(blockpos, true); // remove true if light bugs
			if(block) {
				block->setLightingExpired(true);
				//modified_blocks[blockpos] = block;
				//if(!nodemgr->get(neighbors[i].node).light_propagates || nodemgr->get(neighbors[i].node).light_source) // better to update always
				//	lighting_modified_blocks.set_try(block->getPos(), block);
			}
			// fmtodo: make here random %2 or..
			if (total_level < level_max * can_liquid)
				must_reflow.push_back(neighbors[i].pos);

		}

		if (fall_down) {
			m_server->getEnv().nodeUpdate(neighbors[D_BOTTOM].pos, 1);
		}

#if LIQUID_DEBUG
		//if (total_was != flowed) {
		if (total_was > flowed) {
			infostream << " volume changed!  flowed=" << flowed << " total_was=" << total_was << " want_level=" << want_level;
			for (u16 rr = 0; rr <= 6; rr++) {
				infostream << "  i=" << rr << ",b" << (int)liquid_levels[rr] << ",a" << (int)liquid_levels_want[rr];
			}
			infostream << std::endl;
		}
#endif
		/* //for better relax  only same level
		if (changed)  for (u16 ii = D_SELF + 1; ii < D_TOP; ++ii) {
			if (!neighbors[ii].l) continue;
			must_reflow.push_back(p0 + dirs[ii]);
		}*/
		//g_profiler->graphAdd("liquids", 1);
	}

	u32 ret = loopcount >= initial_size ? 0 : transforming_liquid_size();
	if (ret || loopcount > m_liquid_step_flow)
		m_liquid_step_flow += (m_liquid_step_flow > loopcount ? -1 : 1) * (int)loopcount / 10;
	/*
	if (loopcount)
		infostream<<"Map::transformLiquidsReal(): loopcount="<<loopcount<<" initial_size="<<initial_size
		<<" avgflow="<<m_liquid_step_flow
		<<" reflow="<<must_reflow.size()
		<<" reflow_second="<<must_reflow_second.size()
		<<" reflow_third="<<must_reflow_third.size()
		<<" queue="<< transforming_liquid_size()
		<<" per="<< porting::getTimeMs() - (end_ms - max_cycle_ms)
		<<" ret="<<ret<<std::endl;
	*/

	{
		//TimeTaker timer13("transformLiquidsReal() reflow");
		//auto lock = m_transforming_liquid.lock_unique_rec();
		std::lock_guard<std::mutex> lock(m_transforming_liquid_mutex);

		//m_transforming_liquid.insert(must_reflow.begin(), must_reflow.end());
		for (const auto & p : must_reflow)
			m_transforming_liquid.push_back(p);
		must_reflow.clear();
		//m_transforming_liquid.insert(must_reflow_second.begin(), must_reflow_second.end());
		for (const auto & p : must_reflow_second)
			m_transforming_liquid.push_back(p);
		must_reflow_second.clear();
		//m_transforming_liquid.insert(must_reflow_third.begin(), must_reflow_third.end());
		for (const auto & p : must_reflow_third)
			m_transforming_liquid.push_back(p);
		must_reflow_third.clear();
	}

	g_profiler->add("Server: liquids real processed", loopcount);
	if (regenerated)
		g_profiler->add("Server: liquids regenerated", regenerated);
	if (loopcount < initial_size)
		g_profiler->add("Server: liquids queue", initial_size);

	return loopcount;
}
