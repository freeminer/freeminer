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

const v3POS liquid_flow_dirs[7] =
{
	// +right, +top, +back
	v3POS( 0,-1, 0), // bottom
	v3POS( 0, 0, 0), // self
	v3POS( 0, 0, 1), // back
	v3POS( 0, 0,-1), // front
	v3POS( 1, 0, 0), // right
	v3POS(-1, 0, 0), // left
	v3POS( 0, 1, 0)  // top
};

// when looking around we must first check self node for correct type definitions
const s8 liquid_explore_map[7] = {1,0,6,2,3,4,5};
const s8 liquid_random_map[4][7] = {
	{0,1,2,3,4,5,6},
	{0,1,4,3,5,2,6},
	{0,1,3,5,4,2,6},
	{0,1,5,3,2,4,6}
};

#define D_BOTTOM 0
#define D_TOP 6
#define D_SELF 1

u32 Map::transformLiquidsReal(Server *m_server, unsigned int max_cycle_ms)
{

	INodeDefManager *nodemgr = m_gamedef->ndef();

	DSTACK(__FUNCTION_NAME);
	//TimeTaker timer("transformLiquidsReal()");
	u32 loopcount = 0;
	u32 initial_size = transforming_liquid_size();
	u32 regenerated = 0;

	//bool debug = 0;

	u8 relax = g_settings->getS16("liquid_relax");
	bool fast_flood = g_settings->getS16("liquid_fast_flood");
	int water_level = g_settings->getS16("water_level");
	s16 liquid_pressure = 0;
	g_settings->getS16NoEx("liquid_pressure", liquid_pressure);

	// list of nodes that due to viscosity have not reached their max level height
	//std::unordered_map<v3POS, bool, v3POSHash, v3POSEqual> must_reflow, must_reflow_second, must_reflow_third;
	std::list<v3POS> must_reflow, must_reflow_second, must_reflow_third;
	// List of MapBlocks that will require a lighting update (due to lava)
	u16 loop_rand = myrand();

	u32 end_ms = porting::getTimeMs() + max_cycle_ms;

	NEXT_LIQUID:;
	while (transforming_liquid_size() > 0)
	{
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size*2 || porting::getTimeMs() > end_ms)
			break;
		++loopcount;
		/*
			Get a queued transforming liquid node
		*/
		v3POS p0;
		{
			//JMutexAutoLock lock(m_transforming_liquid_mutex);
			p0 = transforming_liquid_pop();
		}
		s16 total_level = 0;
		//u16 level_max = 0;
		// surrounding flowing liquid nodes
		NodeNeighbor neighbors[7] = { { } };
		// current level of every block
		s8 liquid_levels[7] = {-1, -1, -1, -1, -1, -1, -1};
		 // target levels
		s8 liquid_levels_want[7] = {-1, -1, -1, -1, -1, -1, -1};
		s8 can_liquid_same_level = 0;
		s8 can_liquid = 0;
		content_t liquid_kind = CONTENT_IGNORE;
		content_t liquid_kind_flowing = CONTENT_IGNORE;
		content_t melt_kind = CONTENT_IGNORE;
		content_t melt_kind_flowing = CONTENT_IGNORE;
		//s8 viscosity = 0;
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

			if (!nb.content) {
				//if (i == D_SELF && (loopcount % 2) && initial_size < m_liquid_step_flow * 3)
				//	must_reflow_third[nb.pos] = 1;
				//	must_reflow_third.push_back(nb.pos);
				continue;
			}

			switch (nodemgr->get(nb.content).liquid_type) {
				case LIQUID_NONE:
					if (nb.content == CONTENT_AIR) {
						liquid_levels[i] = 0;
						nb.liquid = 1;
					}
					//TODO: if (nb.content == CONTENT_AIR || nodemgr->get(nb.node).buildable_to && !nodemgr->get(nb.node).walkable) { // need lua drop api for drop torches
					else if (	melt_kind_flowing &&
							nb.content == melt_kind_flowing &&
							nb.type != NEIGHBOR_UPPER &&
							!(loopcount % 2)) {
						u8 melt_max_level = nb.node.getMaxLevel(nodemgr);
						u8 my_max_level = MapNode(liquid_kind_flowing).getMaxLevel(nodemgr);
						liquid_levels[i] = (float)my_max_level / melt_max_level * nb.node.getLevel(nodemgr);
						if (liquid_levels[i])
							nb.liquid = 1;
					} else if (	melt_kind &&
							nb.content == melt_kind &&
							nb.type != NEIGHBOR_UPPER &&
							!(loopcount % 8)) {
						liquid_levels[i] = nodemgr->get(liquid_kind_flowing).getMaxLevel();
						if (liquid_levels[i])
							nb.liquid = 1;
					} else {
						int drop = ((ItemGroupList) nodemgr->get(nb.node).groups)["drop_by_liquid"];
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
					if (!liquid_kind_flowing)
						liquid_kind_flowing = nodemgr->getId(
							nodemgr->get(nb.node).liquid_alternative_flowing);
					if (!liquid_kind)
						liquid_kind = nb.content;
					if (!liquid_kind_flowing)
						liquid_kind_flowing = liquid_kind;
					if (!melt_kind)
						melt_kind = nodemgr->getId(nodemgr->get(nb.node).melt);
					if (!melt_kind_flowing)
						melt_kind_flowing =
							nodemgr->getId(
							nodemgr->get(nodemgr->getId(nodemgr->get(nb.node).melt)
									).liquid_alternative_flowing);
					if (!melt_kind_flowing)
						melt_kind_flowing = melt_kind;
					if (nb.content == liquid_kind) {
						liquid_levels[i] = nb.node.getLevel(nodemgr); //LIQUID_LEVEL_SOURCE;
						nb.liquid = 1;
						nb.infinity = (nb.node.param2 & LIQUID_INFINITY_MASK);
					}
					break;
				case LIQUID_FLOWING:
					// if this node is not (yet) of a liquid type,
					// choose the first liquid type we encounter
					if (!liquid_kind_flowing)
						liquid_kind_flowing = nb.content;
					if (!liquid_kind)
						liquid_kind = nodemgr->getId(
							nodemgr->get(nb.node).liquid_alternative_source);
					if (!liquid_kind)
						liquid_kind = liquid_kind_flowing;
					if (!melt_kind_flowing)
						melt_kind_flowing = nodemgr->getId(nodemgr->get(nb.node).melt);
					if (!melt_kind)
						melt_kind = nodemgr->getId(nodemgr->get(nodemgr->getId(
							nodemgr->get(nb.node).melt)).liquid_alternative_source);
					if (!melt_kind)
						melt_kind = melt_kind_flowing;
					if (nb.content == liquid_kind_flowing) {
						liquid_levels[i] = nb.node.getLevel(nodemgr);
						nb.liquid = 1;
					}
					break;
			}

			// only self, top, bottom swap
			if (nodemgr->get(nb.content).liquid_type && e <= 2) {
				try{
					nb.weight = ((ItemGroupList) nodemgr->get(nb.node).groups)["weight"];
					if (e == 1 && neighbors[D_BOTTOM].weight && neighbors[D_SELF].weight > neighbors[D_BOTTOM].weight) {
						setNode(neighbors[D_SELF].pos, neighbors[D_BOTTOM].node);
						setNode(neighbors[D_BOTTOM].pos, neighbors[D_SELF].node);
						//must_reflow_second[neighbors[D_SELF].pos] = 1;
						//must_reflow_second[neighbors[D_BOTTOM].pos] = 1;
						must_reflow_second.push_back(neighbors[D_SELF].pos);
						must_reflow_second.push_back(neighbors[D_BOTTOM].pos);
						goto NEXT_LIQUID;
					}
					if (e == 2 && neighbors[D_SELF].weight && neighbors[D_TOP].weight > neighbors[D_SELF].weight) {
						setNode(neighbors[D_SELF].pos, neighbors[D_TOP].node);
						setNode(neighbors[D_TOP].pos, neighbors[D_SELF].node);
						//must_reflow_second[neighbors[D_SELF].pos] = 1;
						//must_reflow_second[neighbors[D_TOP].pos] = 1;
						must_reflow_second.push_back(neighbors[D_SELF].pos);
						must_reflow_second.push_back(neighbors[D_TOP].pos);
						goto NEXT_LIQUID;
					}
				}
				catch(InvalidPositionException &e) {
					verbosestream<<"transformLiquidsReal: weight: setNode() failed:"<< nb.pos<<":"<<e.what()<<std::endl;
					//goto NEXT_LIQUID;
				}
			}

			if (nb.liquid) {
				++can_liquid;
				if(nb.type == NEIGHBOR_SAME_LEVEL)
					++can_liquid_same_level;
			}
			if (liquid_levels[i] > 0)
				total_level += liquid_levels[i];

			/*
			infostream << "get node i=" <<(int)i<<" " << PP(nb.pos) << " c="
			<< nb.content <<" p0="<< (int)nb.node.param0 <<" p1="
			<< (int)nb.node.param1 <<" p2="<< (int)nb.node.param2 << " lt="
			<< nodemgr->get(nb.content).liquid_type
			//<< " lk=" << liquid_kind << " lkf=" << liquid_kind_flowing
			<< " l="<< nb.liquid	<< " inf="<< nb.infinity << " nlevel=" << (int)liquid_levels[i]
			<< " totallevel=" << (int)total_level << " cansame="
			<< (int)can_liquid_same_level << " Lmax="<<(int)nodemgr->get(liquid_kind_flowing).getMaxLevel()<<std::endl;
			*/
		}

		if (!liquid_kind || !neighbors[D_SELF].liquid || total_level <= 0)
			continue;

		s16 level_max = nodemgr->get(liquid_kind_flowing).getMaxLevel();
		s16 level_max_compressed = nodemgr->get(liquid_kind_flowing).getMaxLevel(1);
		s16 pressure = liquid_pressure ? ((ItemGroupList) nodemgr->get(liquid_kind).groups)["pressure"] : 0;
		//s16 total_was = total_level; //debug
		//viscosity = nodemgr->get(liquid_kind).viscosity;

		s16 level_avg = total_level / can_liquid;
		if (!pressure && level_avg) {
			level_avg = level_max;
		}

/*
		if (debug)
			infostream<<" go: "
				<<" total_level="<<(int)total_level
				<<" total_was="<<(int)total_was
				<<" level_max="<<(int)level_max
				<<" level_max_compressed="<<(int)level_max_compressed
				<<" level_avg="<<(int)level_avg
				<<" pressure="<<(int)pressure
				<<" can_liquid="<<(int)can_liquid
				<<" can_liquid_same_level="<<(int)can_liquid_same_level
			;
*/

		// fill bottom block
		if (neighbors[D_BOTTOM].liquid) {
			liquid_levels_want[D_BOTTOM] = level_avg > level_max ? level_avg : total_level > level_max ? level_max : total_level;
			total_level -= liquid_levels_want[D_BOTTOM];
			//if (pressure && total_level && liquid_levels_want[D_BOTTOM] < level_max_compressed) {
			//	++liquid_levels_want[D_BOTTOM];
			//	--total_level;
			//}
		}

		//relax up
		if (	nodemgr->get(liquid_kind).liquid_renewable &&
			relax &&
			((p0.Y == water_level) || (fast_flood && p0.Y <= water_level)) &&
			level_max > 1 &&
			liquid_levels[D_TOP] == 0 &&
			liquid_levels[D_BOTTOM] == level_max &&
			total_level >= level_max * can_liquid_same_level - (can_liquid_same_level - relax) &&
			can_liquid_same_level >= relax + 1) {
			regenerated += level_max * can_liquid_same_level - total_level;
			total_level = level_max * can_liquid_same_level;
		}

		// prevent lakes in air above unloaded blocks
		if (	liquid_levels[D_TOP] == 0 &&
			p0.Y > water_level &&
			level_max > 1 &&
			!neighbors[D_BOTTOM].content &&
			!(loopcount % 3)) {
			--total_level;
		}

		// calculate self level 5 blocks
		u16 want_level = level_avg > level_max ? level_avg :
			  total_level >= level_max * can_liquid_same_level
			? level_max
			: total_level / can_liquid_same_level;
		total_level -= want_level * can_liquid_same_level;

		if (pressure && total_level > 0 && neighbors[D_BOTTOM].liquid) { // bottom pressure +1
			++liquid_levels_want[D_BOTTOM];
			--total_level;
		}

		//relax down
		if (	nodemgr->get(liquid_kind).liquid_renewable &&
			relax &&
			p0.Y == water_level &&
			liquid_levels[D_TOP] == 0 &&
			!(loopcount % 2) &&
			level_max > 1 &&
			liquid_levels[D_BOTTOM] == level_max &&
			want_level == 0 &&
			total_level <= (can_liquid_same_level - relax) &&
			can_liquid_same_level >= relax + 1) {
			total_level = 0;
		}

		for (u16 ir = D_SELF; ir < D_TOP; ++ir) { // fill only same level
			u16 ii = liquid_random_map[(loopcount+loop_rand+1)%4][ir];
			if (!neighbors[ii].liquid)
				continue;
			liquid_levels_want[ii] = want_level;
			//if (viscosity > 1 && (liquid_levels_want[ii]-liquid_levels[ii]>8-viscosity))
			if (liquid_levels_want[ii] < level_max && total_level > 0) {
				if (level_max > LIQUID_LEVEL_SOURCE || loopcount % 3 || liquid_levels[ii] <= 0){
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
			u16 ii = liquid_random_map[(loopcount+loop_rand+2)%4][ir];
			if (liquid_levels_want[ii] >= 0 &&
				liquid_levels_want[ii] < level_max) {
				++liquid_levels_want[ii];
				--total_level;
			}
		}

		// fill top block if can
		if (neighbors[D_TOP].liquid) {
			//infostream<<"compressing to top was="<<liquid_levels_want[D_TOP]<<" add="<<total_level<<std::endl;
			//liquid_levels_want[D_TOP] = total_level>level_max_compressed?level_max_compressed:total_level;
			liquid_levels_want[D_TOP] = total_level>level_max?level_max:total_level;
			total_level -= liquid_levels_want[D_TOP];
		}

		if (liquid_levels_want[D_TOP] && total_level && pressure) {
			if (total_level > 0 && neighbors[D_BOTTOM].liquid) { // bottom pressure +2
				++liquid_levels_want[D_BOTTOM];
				--total_level;
			}

			for (u16 ir = D_SELF; ir < D_TOP; ++ir) {
				if (total_level < 1)
					break;
				u16 ii = liquid_random_map[(loopcount+loop_rand+2)%4][ir];
				if (neighbors[ii].liquid &&
					liquid_levels_want[ii] < level_max_compressed) {
					++liquid_levels_want[ii];
					--total_level;
				}
			}
		}

		if (total_level > 0 && neighbors[D_TOP].liquid && liquid_levels_want[D_TOP] < level_max_compressed) {
			s16 add = (total_level > level_max_compressed - liquid_levels_want[D_TOP]) ? level_max_compressed - liquid_levels_want[D_TOP] : total_level;
			liquid_levels_want[D_TOP] += add;
			total_level -= add;
		}

		if (total_level > 0 && liquid_levels_want[D_SELF] < level_max_compressed) { // very rare, compressed only
			s16 add = (total_level > level_max_compressed - liquid_levels_want[D_SELF]) ? level_max_compressed - liquid_levels_want[D_SELF] : total_level;
			liquid_levels_want[D_SELF] += add;
			total_level -= add;
		}

		for (u16 ii = 0; ii < 7; ii++) // infinity and cave flood optimization
			if (    neighbors[ii].infinity		||
				(liquid_levels_want[ii] >= 0	&&
				 level_max > 1					&&
				 fast_flood						&&
				 p0.Y < water_level				&&
				 initial_size >= 1000			&&
				 ii != D_TOP					&&
				 want_level >= level_max/4		&&
				 can_liquid_same_level >= 5		&&
				 liquid_levels[D_TOP] >= level_max)) {
					liquid_levels_want[ii] = level_max;
			}

		/*
		if (total_level != 0) //|| flowed != volume)
			infostream <<" AFTER err level=" << (int)total_level
			//<< " flowed="<<flowed<< " volume=" << volume
			<< " max="<<(int)level_max
			<< " wantsame="<<(int)want_level<< " top="
			<< (int)liquid_levels_want[D_TOP]<< " topwas="
			<< (int)liquid_levels[D_TOP]
			<< " bot=" << (int)liquid_levels_want[D_BOTTOM] 
			<< " botwas=" << (int)liquid_levels[D_BOTTOM]
			<<std::endl;
		*/

		//s16 flowed = 0; // for debug
		for (u16 r = 0; r < 7; r++) {
			u16 i = liquid_random_map[(loopcount+loop_rand+3)%4][r];
			if (liquid_levels_want[i] < 0 || !neighbors[i].liquid)
				continue;

			//if (debug) infostream <<" set=" <<i<< " " << PP(neighbors[i].pos) << " want="<<(int)liquid_levels_want[i] << " was=" <<(int) liquid_levels[i] << std::endl;
			
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
					u16 ii = liquid_random_map[(loopcount+loop_rand+4)%4][ir];
					if (neighbors[ii].liquid)
						must_reflow_second.push_back(neighbors[i].pos + liquid_flow_dirs[ii]);
						//must_reflow_second[neighbors[i].pos + liquid_flow_dirs[ii]] = 1;
				}
			}

			//flowed += liquid_levels_want[i];
			if (liquid_levels[i] == liquid_levels_want[i]) {
				continue;
			}

			if (neighbors[i].drop) {// && level_max > 1 && total_level >= level_max - 1
				//JMutexAutoLock envlock(m_server->m_env_mutex); // 8(
				m_server->getEnv().getScriptIface()->node_drop(neighbors[i].pos, 2);
			}

			neighbors[i].node.setContent(liquid_kind_flowing);
			neighbors[i].node.setLevel(nodemgr, liquid_levels_want[i], 1);

			try{
				setNode(neighbors[i].pos, neighbors[i].node);
			} catch(InvalidPositionException &e) {
				verbosestream<<"transformLiquidsReal: setNode() failed:"<<neighbors[i].pos<<":"<<e.what()<<std::endl;
			}

			// If node emits light, MapBlock requires lighting update
			// or if node removed
			v3POS blockpos = getNodeBlockPos(neighbors[i].pos);
			MapBlock *block = getBlockNoCreateNoEx(blockpos, true); // remove true if light bugs
			if(block != NULL) {
				//modified_blocks[blockpos] = block;
				if(!nodemgr->get(neighbors[i].node).light_propagates || nodemgr->get(neighbors[i].node).light_source) // better to update always
					lighting_modified_blocks.set_try(block->getPos(), block);
			}
			must_reflow.push_back(neighbors[i].pos);

		}

		/* debug
		if (total_was!=flowed) {
			verbosestream<<" volume changed!  flowed="<<flowed<<" total_was="<<total_was;
			for (u16 rr = 0; rr <= 6; rr++) {
				infostream<<"  i=" <<rr<<",b"<<(int)liquid_levels[rr]<<",a"<<(int)liquid_levels_want[rr];
			}
			infostream<<std::endl;
		}
		*/
		/* //for better relax  only same level
		if (changed)  for (u16 ii = D_SELF + 1; ii < D_TOP; ++ii) {
			if (!neighbors[ii].l) continue;
			must_reflow.push_back(p0 + dirs[ii]);
		}*/
		//g_profiler->graphAdd("liquids", 1);
	}

	u32 ret = loopcount >= initial_size ? 0 : transforming_liquid_size();
	if (ret || loopcount > m_liquid_step_flow)
		m_liquid_step_flow += (m_liquid_step_flow > loopcount ? -1 : 1) * (int)loopcount/10;
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

	//JMutexAutoLock lock(m_transforming_liquid_mutex);

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

	return loopcount;
}
