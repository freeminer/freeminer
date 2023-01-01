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
#include <vector>

//#include "environment.h"
#include "gamedef.h"
#include "nodedef.h"
#include "settings.h"
#include "mapblock.h" // For getNodeBlockPos
#include "map.h"
#include "log_types.h"
#include "serverenvironment.h"
#include "server.h"


class LiquidDropABM : public ActiveBlockModifier {
private:
	std::vector<std::string> contents;

public:
	LiquidDropABM(ServerEnvironment *env, NodeDefManager *nodemgr) {
		contents.emplace_back("group:liquid_drop");
	}
	virtual const std::vector<std::string> getTriggerContents() const override
	{ return contents; }
	virtual const std::vector<std::string> getRequiredNeighbors(bool activate) const override {
		std::vector<std::string> neighbors;
		neighbors.emplace_back("air");
		return neighbors;
	}
	virtual float getTriggerInterval() override
	{ return 20; }
	virtual u32 getTriggerChance() override
	{ return 10; }
	virtual s16 getMinY() override { return -MAX_MAP_GENERATION_LIMIT;};
	virtual s16 getMaxY() override { return MAX_MAP_GENERATION_LIMIT;};

	bool getSimpleCatchUp() override { return true; }
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
	                     u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) override {
		ServerMap *map = &env->getServerMap();
		if (map->transforming_liquid_size() > map->m_liquid_step_flow)
			return;
		if (   map->getNodeTry(p - v3pos_t(0,  1, 0 )).getContent() != CONTENT_AIR  // below
		        && map->getNodeTry(p - v3pos_t(1,  0, 0 )).getContent() != CONTENT_AIR  // right
		        && map->getNodeTry(p - v3pos_t(-1, 0, 0 )).getContent() != CONTENT_AIR  // left
		        && map->getNodeTry(p - v3pos_t(0,  0, 1 )).getContent() != CONTENT_AIR  // back
		        && map->getNodeTry(p - v3pos_t(0,  0, -1)).getContent() != CONTENT_AIR  // front
		   )
			return;
		map->transforming_liquid_add(p);
	}
};

class LiquidFreeze : public ActiveBlockModifier {
public:
	LiquidFreeze(ServerEnvironment *env, NodeDefManager *nodemgr) { }
	virtual const std::vector<std::string> getTriggerContents() const override {
		std::vector<std::string> s;
		s.emplace_back("group:freeze");
		return s;
	}
	virtual const std::vector<std::string> getRequiredNeighbors(bool activate) const override {
		std::vector<std::string> s;
		s.emplace_back("air"); //maybe if !activate
		if(!activate) {
			s.emplace_back("group:melt");
		}
		return s;
	}
	virtual float getTriggerInterval() override
	{ return 10; }
	virtual u32 getTriggerChance() override
	{ return 10; }
	virtual s16 getMinY() override { return -MAX_MAP_GENERATION_LIMIT;};
	virtual s16 getMaxY() override { return MAX_MAP_GENERATION_LIMIT;};
	bool getSimpleCatchUp() { return true; }
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
	                     u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) override {
		static int water_level = g_settings->getS16("water_level");
		// Try avoid flying square freezed blocks
		if (p.Y > water_level && activate)
			return;

		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();

		auto heat = map->updateBlockHeat(env, p);
		//heater = rare
		content_t c = map->getNodeTry(p - v3pos_t(0,  -1, 0 )).getContent(); // top
		//more chance to freeze if air at top
		bool top_liquid = ndef->get(n).liquid_type > LIQUID_NONE && p.Y > water_level;
		int freeze = ((ItemGroupList) ndef->get(n).groups)["freeze"];
		if (heat <= freeze - 1 && ((!top_liquid && (activate || (heat <= freeze - 50))) || heat <= freeze - 50 ||
		                           (myrand_range(freeze - 50, heat) <= (freeze + (top_liquid ? -42 : c == CONTENT_AIR ? -10 : -40))))) {
			content_t c_self = n.getContent();
			// making freeze not annoying, do not freeze random blocks in center of ocean
			// todo: any block not water (dont freeze _source near _flowing)

			c = map->getNodeTry(p - v3pos_t(0,  1, 0 )).getContent(); // below
			if ((c == CONTENT_AIR || c == CONTENT_IGNORE) && (ndef->get(n.getContent()).liquid_type == LIQUID_FLOWING || ndef->get(n.getContent()).liquid_type == LIQUID_SOURCE))
				return; // do not freeze when falling

			bool allow = activate || (heat < freeze - 40 && p.Y <= water_level);
			// todo: make for(...)
			if (!allow) {
				if (c != c_self && c != CONTENT_IGNORE) allow = 1;
				if (!allow) {
					c = map->getNodeTry(p - v3pos_t(1,  0, 0 )).getContent(); // right
					if (c != c_self && c != CONTENT_IGNORE) allow = 1;
					if (!allow) {
						c = map->getNodeTry(p - v3pos_t(-1, 0, 0 )).getContent(); // left
						if (c != c_self && c != CONTENT_IGNORE) allow = 1;
						if (!allow) {
							c = map->getNodeTry(p - v3pos_t(0,  0, 1 )).getContent(); // back
							if (c != c_self && c != CONTENT_IGNORE) allow = 1;
							if (!allow) {
								c = map->getNodeTry(p - v3pos_t(0,  0, -1)).getContent(); // front
								if (c != c_self && c != CONTENT_IGNORE) allow = 1;
							}
						}
					}
				}
			}
			if (allow) {
				n.freeze_melt(ndef, -1);
				map->setNode(p, n);
			}
		}
	}
};

class MeltWeather : public ActiveBlockModifier {
public:
	MeltWeather(ServerEnvironment *env, NodeDefManager *nodemgr) { }
	virtual const std::vector<std::string> getTriggerContents() const override {
		std::vector<std::string> s;
		s.emplace_back("group:melt");
		return s;
	}
	virtual const std::vector<std::string> getRequiredNeighbors(bool activate) const override {
		std::vector<std::string> s;
		if(!activate) {
			s.emplace_back("air");
			s.emplace_back("group:freeze");
		}
		return s;
	}
	virtual float getTriggerInterval() override
	{ return 10; }
	virtual u32 getTriggerChance() override
	{ return 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual s16 getMinY() override { return -MAX_MAP_GENERATION_LIMIT;};
	virtual s16 getMaxY() override { return MAX_MAP_GENERATION_LIMIT;};
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
	                     u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) override {
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		float heat = map->updateBlockHeat(env, p);
		content_t c = map->getNodeTry(p - v3pos_t(0,  -1, 0 )).getContent(); // top
		int melt = ((ItemGroupList) ndef->get(n).groups)["melt"];
		if (heat >= melt + 1 && (activate || heat >= melt + 40 ||
		                         ((myrand_range(heat, (float)melt + 40)) >= (c == CONTENT_AIR ? melt + 10 : melt + 20)))) {
			if (ndef->get(n.getContent()).liquid_type == LIQUID_FLOWING || ndef->get(n.getContent()).liquid_type == LIQUID_SOURCE) {
				c = map->getNodeTry(p - v3pos_t(0,  1, 0 )).getContent(); // below
				if (c == CONTENT_AIR || c == CONTENT_IGNORE)
					return; // do not melt when falling (dirt->dirt_with_grass on air)
			}
			n.freeze_melt(ndef, +1);
			map->setNode(p, n);
			env->nodeUpdate(p, 2); //enable after making FAST nodeupdate
		}
	}
};

class MeltHot : public ActiveBlockModifier {
public:
	MeltHot(ServerEnvironment *env, NodeDefManager *nodemgr) { }
	virtual const std::vector<std::string> getTriggerContents() const override {
		std::vector<std::string> s;
		s.emplace_back("group:melt");
		return s;
	}
	virtual const std::vector<std::string> getRequiredNeighbors(bool activate) const override {
		std::vector<std::string> s;
		s.emplace_back("group:igniter");
		s.emplace_back("group:hot");
		return s;
	}
	virtual u32 getNeighborsRange() override
	{ return 2; }
	virtual float getTriggerInterval() override
	{ return 10; }
	virtual u32 getTriggerChance() override
	{ return 5; }
	bool getSimpleCatchUp() override { return true; }
	virtual s16 getMinY() override { return -MAX_MAP_GENERATION_LIMIT;};
	virtual s16 getMaxY() override { return MAX_MAP_GENERATION_LIMIT;};
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
	                     u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) override {
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		int hot = ((ItemGroupList) ndef->get(neighbor).groups)["hot"];
		int melt = ((ItemGroupList) ndef->get(n).groups)["melt"];
		if (hot > melt) {
			n.freeze_melt(ndef, +1);
			map->setNode(p, n);
			env->nodeUpdate(p, 2);
		}
	}
};

class LiquidFreezeCold : public ActiveBlockModifier {
public:
	LiquidFreezeCold(ServerEnvironment *env, NodeDefManager *nodemgr) { }
	virtual const std::vector<std::string> getTriggerContents() const override {
		std::vector<std::string> s;
		s.emplace_back("group:freeze");
		return s;
	}
	virtual const std::vector<std::string> getRequiredNeighbors(bool activate) const override {
		std::vector<std::string> s;
		s.emplace_back("group:cold");
		return s;
	}
	virtual u32 getNeighborsRange() override
	{ return 2; }
	virtual float getTriggerInterval() override
	{ return 10; }
	virtual u32 getTriggerChance() override
	{ return 4; }
	bool getSimpleCatchUp() override { return true; }
	virtual s16 getMinY() override { return -MAX_MAP_GENERATION_LIMIT;};
	virtual s16 getMaxY() override { return MAX_MAP_GENERATION_LIMIT;};
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
	                     u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) override {
		ServerMap *map = &env->getServerMap();
		auto *ndef = env->getGameDef()->ndef();
		int cold = ((ItemGroupList) ndef->get(neighbor).groups)["cold"];
		int freeze = ((ItemGroupList) ndef->get(n).groups)["freeze"];
		if (cold < freeze) {
			n.freeze_melt(ndef, -1);
			map->setNode(p, n);
		}
	}
};

void add_legacy_abms(ServerEnvironment *env, NodeDefManager *nodedef) {
	if (g_settings->getBool("liquid_real")) {
		env->addActiveBlockModifier(new LiquidDropABM(env, nodedef));
		env->addActiveBlockModifier(new MeltHot(env, nodedef));
		env->addActiveBlockModifier(new LiquidFreezeCold(env, nodedef));
		if (env->m_use_weather) {
			env->addActiveBlockModifier(new LiquidFreeze(env, nodedef));
			env->addActiveBlockModifier(new MeltWeather(env, nodedef));
		}
	}
}
