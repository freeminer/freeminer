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

#include "servermap.h"

#include "content_abm.h"
#include "light.h"
#include "server.h"
#include "serverenvironment.h"
#include "util/numeric.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
void queue_liquid_update(ServerMap *map, const v3pos_t &p)
{
	map->transforming_liquid_add(p);
}
}

class LiquidDropABM : public ActiveBlockModifier
{
private:
	std::vector<std::string> contents;

public:
	LiquidDropABM(ServerEnvironment *env, NodeDefManager *nodemgr)
	{
		contents.emplace_back("group:liquid_drop");
	}
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return contents;
	}
	const std::vector<std::string> rn{"air"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return rn;
	}
	const std::vector<std::string> wn{};
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return wn;
	};

	virtual float getTriggerInterval() override { return 20; }
	virtual u32 getTriggerChance() override { return 10; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };

	bool getSimpleCatchUp() override { return true; }
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
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
	const std::vector<std::string> tc{"group:freeze"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> rn{"air", "group:melt"};
	const std::vector<std::string> rna{"air"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return activate ? rna : rn;
	}
	std::vector<std::string> wn;
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return wn;
	};

	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 10; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	bool getSimpleCatchUp() override { return true; }
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		static const auto water_level = g_settings->getPos("water_level");
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
					queue_liquid_update(map, p);
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
					//const auto &look_cf = ndef->get(n_look);
					const auto n_up = map->getNodeTry(p_new + dir_up);
					const auto &n_up_cf = ndef->get(n_up.getContent());
					if (n_look.getContent() == CONTENT_AIR &&
							((n_up_cf.walkable && !n_up_cf.buildable_to) ||
									n_up.getContent() == nndef.freeze_id)) {
						map->setNode(p, n_look); // swap to old air
						n.freeze_melt(ndef, -1);
						map->setNode(p_new, n);
						queue_liquid_update(map, p);
						queue_liquid_update(map, p_new);
					} else if (n_look.getContent() == nndef.freeze_id) {
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
							queue_liquid_update(map, p);
							queue_liquid_update(map, p_new);
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
	const std::vector<std::string> tc{"group:melt"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> nothing;
	const std::vector<std::string> rn{"air", "group:freeze"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return activate ? nothing : rn;
	}
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return nothing;
	};

	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
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
			queue_liquid_update(map, p);
			env->nodeUpdate(p, 2); // enable after making FAST nodeupdate
		}
	}
};

class MeltHot : public ActiveBlockModifier
{
public:
	MeltHot(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	const std::vector<std::string> tc{"group:melt"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> rn{"group:igniter", "group:hot"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return rn;
	}
	const std::vector<std::string> nothing;
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return nothing;
	};

	virtual u32 getNeighborsRange() override { return 2; }
	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 5; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		const auto &neighbor = map->getNodeTry(neighbor_pos);
		const int hot = ((ItemGroupList)ndef->get(neighbor).groups)["hot"];
		const int melt = ((ItemGroupList)ndef->get(n).groups)["melt"];
		if (hot > melt) {
			n.freeze_melt(ndef, +1);
			map->setNode(p, n);
			queue_liquid_update(map, p);
			env->nodeUpdate(p, 2);
		}
	}
};

class BurnHot : public ActiveBlockModifier
{
public:
	BurnHot(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	const std::vector<std::string> tc{"group:flammable"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> rn{"air"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return rn;
	}
	const std::vector<std::string> nothing;
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return nothing;
	};

	virtual u32 getNeighborsRange() override { return 1; }
	virtual float getTriggerInterval() override { return 20; }
	virtual u32 getTriggerChance() override { return 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{

		const auto *ndef = env->getGameDef()->ndef();
		const int flammable = ((ItemGroupList)ndef->get(n).groups)["flammable"];
		ServerMap *map = &env->getServerMap();
		const auto heat = map->updateBlockHeat(env, p);

		if (heat < 600 - flammable * 50)
			return;

		map->setNode(p, ndef->getId("fire:basic_flame"));
		env->nodeUpdate(p, 2);
	}
};

class LiquidFreezeCold : public ActiveBlockModifier
{
public:
	LiquidFreezeCold(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	const std::vector<std::string> tc{"group:freeze"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> rn{"group:cold"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return rn;
	}
	const std::vector<std::string> nothing;
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return nothing;
	};

	virtual u32 getNeighborsRange() override { return 2; }
	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 4; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		const auto &neighbor = map->getNodeTry(neighbor_pos);
		const int cold = ((ItemGroupList)ndef->get(neighbor).groups)["cold"];
		const int freeze = ((ItemGroupList)ndef->get(n).groups)["freeze"];
		if (cold < freeze) {
			n.freeze_melt(ndef, -1);
			map->setNode(p, n);
			queue_liquid_update(map, p);
		}
	}
};

class WaterEvaporateABM : public ActiveBlockModifier
{
public:
	WaterEvaporateABM(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	const std::vector<std::string> tc{"default:water_flowing"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> rn{"air"};
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return rn;
	}
	const std::vector<std::string> nothing;
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return nothing;
	};

	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		const v3pos_t top_pos = p + v3pos_t(0, 1, 0);
		const MapNode top_node = map->getNodeTry(top_pos);
		if (top_node.getContent() != CONTENT_AIR)
			return;

		int humidity = map->updateBlockHumidity(env, p);
		if (humidity >= 96)
			return;
		humidity = rangelim(humidity, 0, 100);

		const int heat = map->updateBlockHeat(env, p);
		if (heat < -5)
			return;
		if (humidity >= 75 && heat >= -2 && heat <= 50)
			return;

		const auto light = top_node.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(top_node));
		const float sun = light >= LIGHT_SUN ? 1.0f :
				static_cast<float>(light) / static_cast<float>(LIGHT_SUN);
		const float dry = std::max(0.0f, static_cast<float>(96 - humidity) / 96.0f);
		const float warm = rangelim(static_cast<float>(heat + 5) / 45.0f, 0.0f, 1.8f);
		const float amount = dry * warm * (0.35f + 0.65f * sun) * 6.0f;
		int whole = static_cast<int>(std::floor(amount));
		const float frac = amount - whole;
		if (myrand_range(0.0f, 1.0f) < frac)
			++whole;

		const int evaporate = rangelim(whole, 0, 8);
		if (evaporate <= 0)
			return;

		n.addLevel(ndef, -evaporate);
		map->setNode(p, n);
		queue_liquid_update(map, p);
	}
};

class SteamEvaporateABM : public ActiveBlockModifier
{
public:
	SteamEvaporateABM(ServerEnvironment *env, NodeDefManager *nodemgr) {}
	const std::vector<std::string> tc{"group:steam"};
	virtual const std::vector<std::string> &getTriggerContents() const override
	{
		return tc;
	}
	const std::vector<std::string> nothing;
	virtual const std::vector<std::string> &getRequiredNeighbors(
			uint8_t activate) const override
	{
		return nothing;
	}
	virtual const std::vector<std::string> &getWithoutNeighbors() const override
	{
		return nothing;
	};

	virtual float getTriggerInterval() override { return 10; }
	virtual u32 getTriggerChance() override { return 4; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t neighbor_pos,
			uint8_t activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();

		int humidity = map->updateBlockHumidity(env, p);
		if (humidity < 0)
			humidity = 0;
		if (humidity > 100)
			humidity = 100;

		const int dry_air = 100 - humidity;
		if (dry_air <= 0 || myrand_range(1, 100) > dry_air)
			return;

		const u8 level = n.getLevel(ndef);
		if (level <= 1) {
			n = MapNode(CONTENT_AIR);
		} else {
			n.setLevel(ndef, level - 1);
		}
		map->setNode(p, n);
		queue_liquid_update(map, p);
	}
};

void add_abm_grow_tree(ServerEnvironment *env, NodeDefManager *nodedef);

void add_fast_abms(ServerEnvironment *env, NodeDefManager *nodedef)
{
	if (g_settings->getBool("liquid_real")) {
		env->addActiveBlockModifier(new LiquidDropABM(env, nodedef));
		env->addActiveBlockModifier(new MeltHot(env, nodedef));
		env->addActiveBlockModifier(new BurnHot(env, nodedef));
		env->addActiveBlockModifier(new LiquidFreezeCold(env, nodedef));
		if (env->m_use_weather) {
			env->addActiveBlockModifier(new LiquidFreeze(env, nodedef));
			env->addActiveBlockModifier(new MeltWeather(env, nodedef));
			env->addActiveBlockModifier(new WaterEvaporateABM(env, nodedef));
			env->addActiveBlockModifier(new SteamEvaporateABM(env, nodedef));
		}
	}
	add_abm_grow_tree(env, nodedef);
}
