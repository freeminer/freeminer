/*
content_abm.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "content_abm.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gamedef.h"
#include "irrTypes.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "light.h"
#include "mapnode.h"
#include "nodedef.h"
#include "settings.h"
#include "mapblock.h" // For getNodeBlockPos
#include "map.h"
#include "log_types.h"
#include "serverenvironment.h"
#include "server.h"
#include "util/numeric.h"
#include "util/unordered_map_hash.h"

class LiquidDropABM : public ActiveBlockModifier
{
private:
	std::vector<std::string> contents;

public:
	LiquidDropABM(ServerEnvironment *env, NodeDefManager *nodemgr)
	{
		contents.emplace_back("group:liquid_drop");
	}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return contents;
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {"air"};
	}
	virtual float getTriggerInterval() override { return 20; }
	virtual u32 getTriggerChance() override { return 10; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };

	bool getSimpleCatchUp() override { return true; }
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		if (map->transforming_liquid_size() > map->m_liquid_step_flow)
			return;
		if (map->getNodeTry(p - v3pos_t(0, 1, 0)).getContent() != CONTENT_AIR // below
				&&
				map->getNodeTry(p - v3pos_t(1, 0, 0)).getContent() != CONTENT_AIR // right
				&&
				map->getNodeTry(p - v3pos_t(-1, 0, 0)).getContent() != CONTENT_AIR // left
				&&
				map->getNodeTry(p - v3pos_t(0, 0, 1)).getContent() != CONTENT_AIR // back
				&& map->getNodeTry(p - v3pos_t(0, 0, -1)).getContent() !=
						   CONTENT_AIR // front
		)
			return;
		map->transforming_liquid_add(p);
	}
};

class LiquidFreeze : public ActiveBlockModifier
{
public:
	LiquidFreeze(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:freeze"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		std::vector<std::string> s;
		s.emplace_back("air"); // maybe if !activate
		if (!activate) {
			s.emplace_back("group:melt");
		}
		return s;
	}
	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 10; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	bool getSimpleCatchUp() override { return true; }
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			bool activate) override
	{
		static const int water_level = g_settings->getS16("water_level");
		// Try avoid flying square freezed blocks
		if (p.Y > water_level && activate)
			return;

		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();

		const auto heat = map->updateBlockHeat(env, p);
		// heater = rare
		const auto c_top = map->getNodeTry(p - v3pos_t(0, -1, 0)).getContent(); // top
		const auto nndef = ndef->get(n);
		// more chance to freeze if air at top
		bool top_liquid = nndef.liquid_type > LIQUID_NONE && p.Y > water_level;
		int freeze = ((ItemGroupList)nndef.groups)["freeze"];
		if (heat <= freeze - 1) {
			if ((!top_liquid && (activate || (heat <= freeze - 50))) ||
					heat <= freeze - 50 ||
					(myrand_range(freeze - 50, heat) <=
							(freeze + (top_liquid					 ? -42
											  : c_top == CONTENT_AIR ? -10
																	 : -40)))) {
				const content_t c_self = n.getContent();
				// making freeze not annoying, do not freeze random blocks in center
				// of ocean todo: any block not water (dont freeze _source near
				// _flowing)

				auto c = map->getNodeTry(p - v3pos_t(0, 1, 0)).getContent(); // below
				if ((c == CONTENT_AIR || c == CONTENT_IGNORE) &&
						(nndef.liquid_type == LIQUID_FLOWING ||
								nndef.liquid_type == LIQUID_SOURCE))
					return; // do not freeze when falling

				bool allow = activate || (heat < freeze - 40 && p.Y <= water_level);
				// todo: make for(...)
				if (!allow) {
					if (c != c_self && c != CONTENT_IGNORE)
						allow = 1;
					if (!allow) {
						c = map->getNodeTry(p - v3pos_t(1, 0, 0)).getContent(); // right
						if (c != c_self && c != CONTENT_IGNORE)
							allow = 1;
						if (!allow) {
							c = map->getNodeTry(p - v3pos_t(-1, 0, 0))
										.getContent(); // left
							if (c != c_self && c != CONTENT_IGNORE)
								allow = 1;
							if (!allow) {
								c = map->getNodeTry(p - v3pos_t(0, 0, 1))
											.getContent(); // back
								if (c != c_self && c != CONTENT_IGNORE)
									allow = 1;
								if (!allow) {
									c = map->getNodeTry(p - v3pos_t(0, 0, -1))
												.getContent(); // front
									if (c != c_self && c != CONTENT_IGNORE)
										allow = 1;
								}
							}
						}
					}
				}
				if (allow) {
					n.freeze_melt(ndef, -1);
					map->setNode(p, n);
				}
			} else if (!activate && (c_top == CONTENT_AIR || n.getContent() == c_top)) {
				// icicle
				const v3pos_t dir_up{0, 1, 0};
				for (const auto &dir_look : {
							 v3pos_t{1, 0, 0},
							 v3pos_t{-1, 0, 0},
							 v3pos_t{0, 0, 1},
							 v3pos_t{0, 0, -1},
					 }) {
					const auto p_new = p + dir_look;
					auto n_look = map->getNodeTry(p_new);
					const auto &look_cf = ndef->get(n_look);
					const auto n_up = map->getNodeTry(p_new + dir_up);
					const auto &n_up_cf = ndef->get(n_up.getContent());
					if (n_look.getContent() == CONTENT_AIR &&
							((n_up_cf.walkable && !n_up_cf.buildable_to) ||
									n_up_cf.name == nndef.freeze)) {
						map->setNode(p, n_look); // swap to old air
						n.freeze_melt(ndef, -1);
						map->setNode(p_new, n);
					} else if (look_cf.name == nndef.freeze) {
						const auto freezed_level = n_look.getLevel(ndef);
						const auto can_freeze =
								n_look.getMaxLevel(ndef, true) - freezed_level;
						const auto have = n.getLevel(ndef);
						const auto amount = have > can_freeze ? can_freeze : have;

						if (amount) {
							n_look.addLevel(ndef, amount);
							map->setNode(p_new, n_look);
							n.addLevel(ndef, -amount);
							map->setNode(p, n);
						}
					}
				}
			}
		}
	}
};

class MeltWeather : public ActiveBlockModifier
{
public:
	MeltWeather(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:melt"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		std::vector<std::string> s;
		if (!activate) {
			s.emplace_back("air");
			s.emplace_back("group:freeze");
		}
		return s;
	}
	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		float heat = map->updateBlockHeat(env, p);
		content_t c = map->getNodeTry(p - v3pos_t(0, -1, 0)).getContent(); // top
		const int melt = ((ItemGroupList)ndef->get(n).groups)["melt"];
		if (heat >= melt + 1 &&
				(activate || heat >= melt + 40 ||
						((myrand_range(heat, (float)melt + 40)) >=
								(c == CONTENT_AIR ? melt + 10 : melt + 20)))) {
			if (ndef->get(n.getContent()).liquid_type == LIQUID_FLOWING ||
					ndef->get(n.getContent()).liquid_type == LIQUID_SOURCE) {
				c = map->getNodeTry(p - v3pos_t(0, 1, 0)).getContent(); // below
				if (c == CONTENT_AIR || c == CONTENT_IGNORE)
					return; // do not melt when falling (dirt->dirt_with_grass on air)
			}
			n.freeze_melt(ndef, +1);
			map->setNode(p, n);
			env->nodeUpdate(p, 2); // enable after making FAST nodeupdate
		}
	}
};

class MeltHot : public ActiveBlockModifier
{
public:
	MeltHot(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:melt"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {"group:igniter", "group:hot"};
	}
	virtual u32 getNeighborsRange() override { return 2; }
	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 5; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		const auto &neighbor = map->getNodeTry(neighbor_pos);
		const int hot = ((ItemGroupList)ndef->get(neighbor).groups)["hot"];
		const int melt = ((ItemGroupList)ndef->get(n).groups)["melt"];
		if (hot > melt) {
			n.freeze_melt(ndef, +1);
			map->setNode(p, n);
			env->nodeUpdate(p, 2);
		}
	}
};

class LiquidFreezeCold : public ActiveBlockModifier
{
public:
	LiquidFreezeCold(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:freeze"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {"group:cold"};
	}
	virtual u32 getNeighborsRange() override { return 2; }
	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 4; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		const auto &neighbor = map->getNodeTry(neighbor_pos);
		const int cold = ((ItemGroupList)ndef->get(neighbor).groups)["cold"];
		const int freeze = ((ItemGroupList)ndef->get(n).groups)["freeze"];
		if (cold < freeze) {
			n.freeze_melt(ndef, -1);
			map->setNode(p, n);
		}
	}
};

// Trees use param2 for rotation, level1 is free
inline uint8_t get_tree_water_level(const MapNode &n)
{
	return n.getParam1();
}
inline void set_tree_water_level(MapNode &n, const uint8_t level)
{
	n.setParam1(level);
}

// Leaves use param1 for light, level2 is free
inline uint8_t get_leaves_water_level(const MapNode &n)
{
	return n.getParam2();
}

inline void set_leaves_water_level(MapNode &n, const uint8_t level)
{
	n.setParam2(level);
}

const v3pos_t leaves_grow_dirs[] = {
		// +right, +top, +back
		v3pos_t{0, 1, 0},  // 1 top
		v3pos_t{0, 0, 1},  // 2 back
		v3pos_t{0, 0, -1}, // 3 front
		v3pos_t{1, 0, 0},  // 4 right
		v3pos_t{-1, 0, 0}, // 5 left
		v3pos_t{0, -1, 0}, // 6 bottom
};

struct GrowParams
{
	int tree_water_max = 30; // todo: depend on humidity 10-100
	int tree_grow_water_min = 5;
	int tree_grow_heat_min = 7;
	int tree_grow_heat_max = 35;
	int tree_grow_light_max = 12; // grow more leaves around before grow tree up
	int leaves_water_max = 20;	  // todo: depend on humidity 2-20
	int leaves_grow_light_min = 8;
	int leaves_grow_water_min_top = 3;
	int leaves_grow_water_min_bottom = 4;
	int leaves_grow_water_min_side = 2;
	int leaves_grow_heat_max = 40;
	int leaves_grow_heat_min = 3;
	int leaves_grow_prefer_top = 0;
	int leaves_die_light_max = 7; // 8
	int leaves_die_heat_max = -1;
	int leaves_die_heat_min = 55;
	int leaves_die_chance = 10;
	int leaves_to_fruit_water_min = 8;
	int leaves_to_fruit_heat_min = 15;
	int leaves_to_fruit_light_min = 9;
	int leaves_to_fruit_chance = 10;

	GrowParams(const ContentFeatures &cf)
	{
		if (cf.groups.contains("tree_water_max"))
			tree_water_max = cf.groups.at("tree_water_max");
		if (cf.groups.contains("tree_grow_water_min"))
			tree_grow_water_min = cf.groups.at("tree_grow_water_min");
		if (cf.groups.contains("tree_grow_heat_min"))
			tree_grow_heat_min = cf.groups.at("tree_grow_heat_min");
		if (cf.groups.contains("tree_grow_heat_max"))
			tree_grow_heat_max = cf.groups.at("tree_grow_heat_max");
		if (cf.groups.contains("tree_grow_light_max"))
			tree_grow_light_max = cf.groups.at("tree_grow_light_max");
		if (cf.groups.contains("leaves_water_max"))
			leaves_water_max = cf.groups.at("leaves_water_max");
		if (cf.groups.contains("leaves_grow_light_min"))
			leaves_grow_light_min = cf.groups.at("leaves_grow_light_min");
		if (cf.groups.contains("leaves_grow_water_min_top"))
			leaves_grow_water_min_top = cf.groups.at("leaves_grow_water_min_top");
		if (cf.groups.contains("leaves_grow_water_min_bottom"))
			leaves_grow_water_min_bottom = cf.groups.at("leaves_grow_water_min_bottom");
		if (cf.groups.contains("leaves_grow_water_min_side"))
			leaves_grow_water_min_side = cf.groups.at("leaves_grow_water_min_side");
		if (cf.groups.contains("leaves_grow_heat_max"))
			leaves_grow_heat_max = cf.groups.at("leaves_grow_heat_max");
		if (cf.groups.contains("leaves_grow_prefer_top"))
			leaves_grow_prefer_top = cf.groups.at("leaves_grow_prefer_top");
		if (cf.groups.contains("leaves_grow_heat_min"))
			leaves_grow_heat_min = cf.groups.at("leaves_grow_heat_min");
		if (cf.groups.contains("leaves_die_light_max"))
			leaves_die_light_max = cf.groups.at("leaves_die_light_max");
		if (cf.groups.contains("leaves_die_heat_max"))
			leaves_die_heat_max = cf.groups.at("leaves_die_heat_max");
		if (cf.groups.contains("leaves_die_heat_min"))
			leaves_die_heat_min = cf.groups.at("leaves_die_heat_min");
		if (cf.groups.contains("leaves_die_chance"))
			leaves_die_chance = cf.groups.at("leaves_die_chance");
		if (cf.groups.contains("leaves_to_fruit_water_min"))
			leaves_to_fruit_water_min = cf.groups.at("leaves_to_fruit_water_min");
		if (cf.groups.contains("leaves_to_fruit_heat_min"))
			leaves_to_fruit_heat_min = cf.groups.at("leaves_to_fruit_heat_min");
		if (cf.groups.contains("leaves_to_fruit_light_min"))
			leaves_to_fruit_light_min = cf.groups.at("leaves_to_fruit_light_min");
		if (cf.groups.contains("leaves_to_fruit_chance"))
			leaves_to_fruit_chance = cf.groups.at("leaves_to_fruit_chance");
	}
};

class GrowTreePump : public ActiveBlockModifier
{
	std::unordered_map<content_t, content_t> tree_to_leaves;
	std::unordered_map<content_t, GrowParams> type_params;

	bool grow_debug_fast = false;
	//  bool grow_debug = false;
public:
	GrowTreePump(ServerEnvironment *env, NodeDefManager *ndef)
	{
		// g_settings->getBoolNoEx("grow_debug", grow_debug);
		g_settings->getBoolNoEx("grow_debug_fast", grow_debug_fast);

		std::vector<content_t> ids;
		ndef->getIds("group:grow_tree", ids);
		for (const auto &id : ids) {
			const auto &cf = ndef->get(id);
			type_params.emplace(id, GrowParams(cf));
			if (!cf.liquid_alternative_source.empty())
				tree_to_leaves[id] = ndef->getId(cf.liquid_alternative_source);
		}
	}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:grow_tree"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {};
	}
	// u32 getNeighborsRange() override { return 3; }
	virtual float getTriggerInterval() override { return grow_debug_fast ? 0.1 : 10; }
	virtual u32 getTriggerChance() override { return grow_debug_fast ? 1 : 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		float heat = map->updateBlockHeat(env, p);
		int16_t n_water_level = get_tree_water_level(n);
		const auto c = n.getContent();
		const auto params = type_params.at(c);

		const auto n_water_level_orig = n_water_level;

		const auto save = [&]() {
			if (n_water_level_orig != n_water_level) {
				set_tree_water_level(n, n_water_level);
				map->setNode(p, n);
			}
		};
		const auto decrease = [&](auto &level, int amount = 1) -> auto {
			if (level <= amount)
				return false;
			level -= amount;
			return true;
		};

		const content_t leaves_c =
				tree_to_leaves.contains(c) ? tree_to_leaves.at(c) : CONTENT_IGNORE;

		const auto facedir = n.getFaceDir(ndef);
		bool allow_grow_by_rotation =
				(facedir >= 0 && facedir <= 3) || (facedir >= 20 && facedir <= 23);

		bool up_all_leaves = true;
		for (pos_t li = 1; li <= LIGHT_SUN - params.tree_grow_light_max; ++li) {
			const auto p_up = p + v3pos_t{0, li, 0};
			const auto n_up = map->getNodeTry(p_up);
			if (!n_up || n_up.getContent() != leaves_c) {
				up_all_leaves = false;
				break;
			}
		}
		bool can_grow_replace_leaves = allow_grow_by_rotation && up_all_leaves;

		size_t i = 0;
		bool allow_grow_tree =
				allow_grow_by_rotation; // dont grow to sides if can grow up
		bool allow_grow_leaves = allow_grow_by_rotation;
		bool have_tree_near = false;
		size_t has_soil = 0;
		std::vector<v3pos_t> has_liquids;
		for (const auto &dir : leaves_grow_dirs) {
			const auto p_dir = p + dir;
			auto n_dir = map->getNodeTry(p_dir);
			if (!n_dir) {
				// if (grow_debug) DUMP("getfail tr", p_dir.X, p_dir.Y, p_dir.Z);
				break;
			}

			auto c_dir = n_dir.getContent();
			const auto &cf = ndef->get(c_dir);

			const auto light_dir =
					n_dir.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(n_dir));

			bool top = !i;
			bool bottom = i + 1 == sizeof(leaves_grow_dirs) / sizeof(leaves_grow_dirs[0]);

			bool is_liquid = cf.groups.contains("liquid");
			if (is_liquid)
				has_liquids.emplace_back(p_dir);

			bool is_tree = cf.groups.contains("tree");
			bool is_soil = cf.groups.contains("soil");
			has_soil += is_soil;

			if (!top && !bottom && is_tree) {
				have_tree_near = true;
			}

			if ((!params.tree_grow_heat_min || heat > params.tree_grow_heat_min) &&
					(!params.tree_grow_heat_max || heat < params.tree_grow_heat_max) &&
					n_water_level >= params.tree_grow_water_min &&
					(((allow_grow_tree &&
							  ((c_dir == leaves_c && can_grow_replace_leaves) ||
									  ((top || bottom) && is_liquid))) ||
							(bottom &&
									(is_liquid || is_soil || cf.groups.contains("sand"))))
#if 0
// TODO: directional grow   
							||(!top && !bottom && n_water_level >= max_water_in_tree &&
									light_dir <= 1
									//&& c_dir == CONTENT_AIR
									&& cf.groups.contains("soil"))
#endif
									)) {
				// dont grow too deep in liquid
				if (bottom && is_liquid && light_dir <= 0)
					continue;
				if (bottom && have_tree_near)
					continue;
				if (!decrease(n_water_level))
					break;

				// if (grow_debug) DUMP("tr->tr", p_dir.Y, p_dir, n_water_level,
				// n_water_level_orig, light_dir);

				map->setNode(p_dir, {c, 1});
				--n_water_level;

			} else if (((!top && !bottom && c_dir == c) || c_dir == leaves_c)) {
				auto wl_dir = c_dir == leaves_c ? get_leaves_water_level(n_dir)
												: get_tree_water_level(n_dir);
				if (wl_dir < (c_dir == leaves_c ? params.leaves_water_max
												: params.tree_water_max) &&
						n_water_level > wl_dir
						/* !!!
												n_water_level > wl_dir + (top ? -1 :bottom
						   ? 1 : 0)
						*/
				) {
					// if (grow_debug) DUMP("tr pumpup", p.Y, n_water_level,
					// n_water_level_orig, wl_dir, top, bottom, c_dir, c);

					{
						if (!decrease(n_water_level)) {
							// if (grow_debug) DUMP("pumpfail", n_water_level,
							// n_water_level_orig, wl_dir, top, bottom, c_dir, c);
							break;
						}
						++wl_dir;
						c_dir == leaves_c ? set_leaves_water_level(n_dir, wl_dir)
										  : set_tree_water_level(n_dir, wl_dir);
					}
					map->setNode(p_dir, n_dir);
				}
			}
			if ((top && c_dir == leaves_c) || c_dir == c) {
				allow_grow_tree = false;
			}
			if (top && c_dir == c) {
				allow_grow_leaves = false;
			}

			if (allow_grow_leaves && leaves_c != CONTENT_IGNORE &&
					heat >= params.tree_grow_heat_min &&
					heat <= params.tree_grow_heat_max &&
					(n_water_level >= (top ? params.leaves_grow_water_min_top
										   : params.leaves_grow_water_min_side)) &&
					// can_grow_leaves(n_water_level, top, bottom) &&
					light_dir >= params.leaves_grow_light_min) {

				if (cf.buildable_to && !is_liquid) {
					if (!decrease(n_water_level))
						break;
					map->setNode(p_dir, {leaves_c, n_dir.param1, 1});
					--n_water_level;
					// if (grow_debug) DUMP("tr->lv", p_dir, n_water_level,
					// n_water_level_orig, light_dir);
					if (const auto block = map->getBlock(getNodeBlockPos(p_dir)); block) {
						block->setLightingExpired(true);
					}
				}
			}

			++i;
		}

		// up-down distribute of rest
		{
			int16_t total_level = n_water_level;
			int8_t have_liquid = 1;
			bool have_top = false, have_bottom = false;
			const auto p_bottom = p + v3pos_t{0, -1, 0};
			auto n_bottom = map->getNodeTry(p_bottom);
			if (n_bottom.getContent() == c) {
				have_bottom = true;
				int16_t wl_bottom = get_tree_water_level(n_bottom);
				total_level += wl_bottom;
				++have_liquid;
				// if (grow_debug) DUMP("get bot", wl_bottom, total_level,
				// (int)have_liquid, have_bottom);
			}

			const auto p_top = p + v3pos_t{0, 1, 0};
			auto n_top = map->getNodeTry(p_top);
			if (n_top.getContent() == c) {
				have_top = true;
				int16_t wl_top = get_tree_water_level(n_top);
				total_level += wl_top;
				++have_liquid;
				// if (grow_debug) DUMP("get top", wl_top, total_level, (int)have_liquid,
				// have_top);
			}

			if (have_bottom) {
				const auto avg_level_for_bottom = ceil((float)total_level / have_liquid);
				// if (grow_debug) DUMP(avg_level_for_bottom, (int)have_liquid,
				// total_level, have_bottom, have_top);
				const auto bottom_level =
						avg_level_for_bottom < params.tree_water_max
								? avg_level_for_bottom +
										  (avg_level_for_bottom >= total_level ? 0 : 1)
								: params.tree_water_max;
				total_level -= bottom_level;
				--have_liquid;
				// if (grow_debug) DUMP("setbot", bottom_level, total_level,
				// avg_level_for_bottom);
				set_tree_water_level(n_bottom, bottom_level);
				map->setNode(p_bottom, n_bottom);
			}
			if (have_top) {
				const auto avg_level_for_top = floor((float)total_level / have_liquid);
				const auto top_level = avg_level_for_top; //- 1;
				total_level -= top_level;
				// if (grow_debug) DUMP("settop", top_level, total_level,
				// avg_level_for_top);
				set_tree_water_level(n_top, top_level);
				map->setNode(p_top, n_top);
			}
			// if (grow_debug) DUMP("total res self:", (int)total_level);
			n_water_level = total_level;
		}

		if (has_soil) {
			n_water_level = grow_debug_fast ? params.tree_water_max
											: 1; // TODO depend on humidity
												 // tODO: absorb water from water here
		}

		if (n_water_level < params.tree_water_max) {
			if (!has_liquids.empty()) {
				// TODO: cached and random
				const auto neighbor_pos = has_liquids.front();
				auto neighbor = map->getNodeTry(neighbor_pos);
				auto level = neighbor.getLevel(ndef);

				// TODO: allow get all water if bottom of water != water
				if (level <= 1)
					return;
				auto amount = grow_debug_fast ? level - 1 : 1;
				if (n_water_level + amount > params.tree_water_max)
					amount = params.tree_water_max - n_water_level;
				level -= amount;

				neighbor.setLevel(ndef, level);

				if (!grow_debug_fast)
					map->setNode(neighbor_pos, neighbor);
				set_tree_water_level(n, n_water_level += amount);
				map->setNode(p, n);
				// if (grow_debug) DUMP("absorb", n_water_level, level, amount);
			} else if (has_soil) {
				float humidity = map->updateBlockHumidity(env, p);
				if (humidity > 70)
					++n_water_level;
			}
		}

		save();
	}
};

class GrowLeaves : public ActiveBlockModifier
{
	std::unordered_map<content_t, content_t> leaves_to_fruit;
	std::unordered_map<content_t, GrowParams> type_params;
	bool grow_debug_fast = false;
	//  bool grow_debug = false;

	static bool can_grow_leaves(
			GrowParams params, int8_t level, bool is_top, bool is_bottom)
	{
		if (is_top)
			return level >= params.leaves_grow_water_min_top;
		if (is_bottom)
			return level >= params.leaves_grow_water_min_bottom;
		return level >= params.leaves_grow_water_min_side;
	}

public:
	GrowLeaves(ServerEnvironment *env, NodeDefManager *ndef)
	{
		// g_settings->getBoolNoEx("grow_debug", grow_debug);
		g_settings->getBoolNoEx("grow_debug_fast", grow_debug_fast);

		std::vector<content_t> ids;
		ndef->getIds("group:grow_leaves", ids);
		for (const auto &id : ids) {
			const auto &cf = ndef->get(id);
			type_params.emplace(id, GrowParams(cf));
			if (!cf.liquid_alternative_source.empty())
				leaves_to_fruit[id] = ndef->getId(cf.liquid_alternative_source);
		}
		DUMP(leaves_to_fruit);
	}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:grow_leaves"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {};
	}
	u32 getNeighborsRange() override { return 1; }
	virtual float getTriggerInterval() override { return grow_debug_fast ? 0.1 : 10; }
	virtual u32 getTriggerChance() override { return grow_debug_fast ? 1 : 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		float heat = map->updateBlockHeat(env, p);
		const auto c = n.getContent();
		const auto params = type_params.at(c);

		int n_water_level = get_leaves_water_level(n);
		const auto n_water_level_orig = n_water_level;

		const auto l = n.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(n));
		// TODO: very rand and slow (then remove with light 0 too)

		if (n_water_level >= 1 && // dont touch old static trees
				((l < params.leaves_die_light_max &&
						 (l > 0 || !myrand_range(0, params.leaves_die_chance))) ||
						((params.leaves_die_heat_max &&
								 heat < params.leaves_die_heat_max) ||
								(params.leaves_die_heat_min &&
										heat > params.leaves_die_heat_min)))) {
			map->removeNodeWithEvent(p, false);
			// todo: return water?
			// if (grow_debug) DUMP("lv rem light", p.X, p.Y, p.Z, l, n_water_level,
			// heat);
			return;
		}

		uint8_t i = 0;
		bool have_tree_or_soil = false;
		bool allow_grow_fruit = leaves_to_fruit.contains(c);
		const content_t c_fruit =
				allow_grow_fruit ? leaves_to_fruit.at(c) : CONTENT_IGNORE;
		// TODO: choose pump and grow direction by leaves type
		for (const auto &dir : leaves_grow_dirs) {
			const auto p_dir = p + dir;
			auto n_dir = map->getNodeTry(p_dir);
			if (!n_dir) {
				have_tree_or_soil = true; // dont remove when map busy
				allow_grow_fruit = false;
				continue;
			}
			auto c_dir = n_dir.getContent();

			const auto &cf = ndef->get(c_dir);
			bool is_tree = cf.groups.contains("tree");
			bool is_leaves = cf.groups.contains("leaves");
			bool top = !i;
			bool bottom = i + 1 == sizeof(leaves_grow_dirs) / sizeof(leaves_grow_dirs[0]);
			/*todo: shapes:
			o    sphere
			|   cypress
			___
			\ /
			*/

			if ((c_dir == c_fruit) || (!top && !bottom && !is_leaves))
				allow_grow_fruit = false;

			if (!have_tree_or_soil)
				have_tree_or_soil = is_tree || is_leaves || cf.groups.contains("soil") ||
									cf.groups.contains("liquid");
			if ((!params.leaves_grow_heat_min || heat >= params.leaves_grow_heat_min) &&
					(!params.leaves_grow_heat_max ||
							heat <= params.leaves_grow_heat_max) &&
					can_grow_leaves(params, n_water_level, top, bottom) &&
					l > params.leaves_grow_light_min && cf.buildable_to &&
					!cf.groups.contains("liquid")) {
				// if (grow_debug) DUMP("lv->lv  ", p.Y, n_water_level,
				// n_water_level_orig, l, ndef->get(c_dir).name);
				// TODO: apples sometimes
				map->setNode(p_dir, {c, n_dir.getParam1(), 1});
				--n_water_level;

				if (const auto block = map->getBlock(getNodeBlockPos(p_dir)); block) {
					block->setLightingExpired(true);
				}

			} else if (c_dir == c) {
				const auto l_dir =
						n.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(n_dir));

				auto wl_dir = get_leaves_water_level(n_dir);
				if (n_water_level > 1 && wl_dir < params.leaves_water_max && l_dir >= l &&
						// todo: all up by type?
						wl_dir < n_water_level - 1 //(top ? -1 : bottom ? 1 : -2)
				) {
					--n_water_level;
					set_leaves_water_level(n_dir, ++wl_dir);
					map->setNode(p_dir, n_dir);

					// if (grow_debug) DUMP("lv pumpup2", p.Y, n_water_level,
					// n_water_level_orig, wl_dir, top, bottom, c_dir);

					// Prefer pump up
					// todo: its like cypress, by settings
					if (top && params.leaves_grow_prefer_top) {
						break;
					}
				}
			}

			if (n_water_level != n_water_level_orig) {
				set_leaves_water_level(n, n_water_level);
				map->setNode(p, n);
			}
			++i;
		}

		// DUMP(allow_grow_fruit, n_water_level, leaves_to_fruit_water_min, heat ,
		// leaves_to_fruit_heat_min);
		if (allow_grow_fruit && n_water_level >= params.leaves_to_fruit_water_min &&
				heat >= params.leaves_to_fruit_heat_min &&
				l >= params.leaves_to_fruit_light_min &&
				(grow_debug_fast || !myrand_range(0, params.leaves_to_fruit_chance))) {
			map->setNode(p, {c_fruit});

		} else if (!have_tree_or_soil && !myrand_range(0, 10)) {
			// if (grow_debug) DUMP("remove single", p.Y, have_tree_or_soil,
			// n_water_level);
			map->removeNodeWithEvent(p, false);
		}
	}
};

void add_legacy_abms(ServerEnvironment *env, NodeDefManager *nodedef)
{
	if (g_settings->getBool("liquid_real")) {
		env->addActiveBlockModifier(new LiquidDropABM(env, nodedef));
		env->addActiveBlockModifier(new MeltHot(env, nodedef));
		env->addActiveBlockModifier(new LiquidFreezeCold(env, nodedef));
		if (env->m_use_weather) {
			env->addActiveBlockModifier(new LiquidFreeze(env, nodedef));
			env->addActiveBlockModifier(new MeltWeather(env, nodedef));
		}
	}
	bool grow = true;
	g_settings->getBoolNoEx("grow_trees", grow);
	if (grow) {
		env->addActiveBlockModifier(new GrowTreePump(env, nodedef));
		env->addActiveBlockModifier(new GrowLeaves(env, nodedef));
	}
}
