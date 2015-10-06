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

#include "environment.h"
#include "gamedef.h"
#include "nodedef.h"
#include "content_sao.h"
#include "settings.h"
#include "mapblock.h" // For getNodeBlockPos
#include "map.h"
#include "scripting_game.h"
#include "log.h"

#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"

class LiquidDropABM : public ActiveBlockModifier {
	private:
		std::set<std::string> contents;

	public:
		LiquidDropABM(ServerEnvironment *env, INodeDefManager *nodemgr) {
			contents.insert("group:liquid_drop");
		}
		virtual std::set<std::string> getTriggerContents()
		{ return contents; }
		virtual std::set<std::string> getRequiredNeighbors(bool activate) {
			std::set<std::string> neighbors;
			neighbors.insert("air");
			return neighbors;
		}
		virtual float getTriggerInterval()
		{ return 20; }
		virtual u32 getTriggerChance()
		{ return 10; }
		virtual void trigger(ServerEnvironment *env, v3POS p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) {
			ServerMap *map = &env->getServerMap();
			if (map->transforming_liquid_size() > map->m_liquid_step_flow)
				return;
			if (   map->getNodeTry(p - v3POS(0,  1, 0 )).getContent() != CONTENT_AIR  // below
			    && map->getNodeTry(p - v3POS(1,  0, 0 )).getContent() != CONTENT_AIR  // right
			    && map->getNodeTry(p - v3POS(-1, 0, 0 )).getContent() != CONTENT_AIR  // left
			    && map->getNodeTry(p - v3POS(0,  0, 1 )).getContent() != CONTENT_AIR  // back
			    && map->getNodeTry(p - v3POS(0,  0, -1)).getContent() != CONTENT_AIR  // front
			   )
				return;
			map->transforming_liquid_push_back(p);
		}
};

class LiquidFreeze : public ActiveBlockModifier {
	public:
		LiquidFreeze(ServerEnvironment *env, INodeDefManager *nodemgr) { }
		virtual std::set<std::string> getTriggerContents() {
			std::set<std::string> s;
			s.insert("group:freeze");
			return s;
		}
		virtual std::set<std::string> getRequiredNeighbors(bool activate) {
			std::set<std::string> s;
			s.insert("air"); //maybe if !activate
			if(!activate) {
				s.insert("group:melt");
			}
			return s;
		}
		virtual float getTriggerInterval()
		{ return 10; }
		virtual u32 getTriggerChance()
		{ return 10; }
		virtual void trigger(ServerEnvironment *env, v3POS p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) {
			ServerMap *map = &env->getServerMap();
			INodeDefManager *ndef = env->getGameDef()->ndef();

			float heat = map->updateBlockHeat(env, p);
			//heater = rare
			content_t c = map->getNodeTry(p - v3POS(0,  -1, 0 )).getContent(); // top
			//more chance to freeze if air at top
			static int water_level = g_settings->getS16("water_level");
			bool top_liquid = ndef->get(n).liquid_type > LIQUID_NONE && p.Y > water_level;
			int freeze = ((ItemGroupList) ndef->get(n).groups)["freeze"];
			if (heat <= freeze-1 && ((!top_liquid && (activate || heat <= freeze-50)) || 
				(myrand_range(freeze-50, heat) <= (freeze + (top_liquid ? -42 : c == CONTENT_AIR ? -10 : -40))))) {
				content_t c_self = n.getContent();
				// making freeze not annoying, do not freeze random blocks in center of ocean
				// todo: any block not water (dont freeze _source near _flowing)
				bool allow = activate || heat < freeze-40;
				// todo: make for(...)
				if (!allow) {
				 c = map->getNodeTry(p - v3POS(0,  1, 0 )).getContent(); // below
				 if (c == CONTENT_AIR || c == CONTENT_IGNORE)
				    if (ndef->get(n.getContent()).liquid_type == LIQUID_FLOWING || ndef->get(n.getContent()).liquid_type == LIQUID_SOURCE)
					return; // do not freeze when falling
				 if (c != c_self && c != CONTENT_IGNORE) allow = 1;
				 if (!allow) {
				  c = map->getNodeTry(p - v3POS(1,  0, 0 )).getContent(); // right
				  if (c != c_self && c != CONTENT_IGNORE) allow = 1;
				  if (!allow) {
				   c = map->getNodeTry(p - v3POS(-1, 0, 0 )).getContent(); // left
				   if (c != c_self && c != CONTENT_IGNORE) allow = 1;
				   if (!allow) {
				    c = map->getNodeTry(p - v3POS(0,  0, 1 )).getContent(); // back
				    if (c != c_self && c != CONTENT_IGNORE) allow = 1;
				    if (!allow) {
				     c = map->getNodeTry(p - v3POS(0,  0, -1)).getContent(); // front
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
		MeltWeather(ServerEnvironment *env, INodeDefManager *nodemgr) { }
		virtual std::set<std::string> getTriggerContents() {
			std::set<std::string> s;
			s.insert("group:melt");
			return s;
		}
		virtual std::set<std::string> getRequiredNeighbors(bool activate) {
			std::set<std::string> s;
			if(!activate) {
				s.insert("air");
				s.insert("group:freeze");
			}
			return s;
		}
		virtual float getTriggerInterval()
		{ return 10; }
		virtual u32 getTriggerChance()
		{ return 10; }
		virtual void trigger(ServerEnvironment *env, v3POS p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) {
			ServerMap *map = &env->getServerMap();
			INodeDefManager *ndef = env->getGameDef()->ndef();
			float heat = map->updateBlockHeat(env, p);
			content_t c = map->getNodeTry(p - v3POS(0,  -1, 0 )).getContent(); // top
			int melt = ((ItemGroupList) ndef->get(n).groups)["melt"];
			if (heat >= melt+1 && (activate || heat >= melt+40 ||
				((myrand_range(heat, melt+40)) >= (c == CONTENT_AIR ? melt+10 : melt+20)))) {
				if (ndef->get(n.getContent()).liquid_type == LIQUID_FLOWING || ndef->get(n.getContent()).liquid_type == LIQUID_SOURCE) {
					 c = map->getNodeTry(p - v3POS(0,  1, 0 )).getContent(); // below
					 if (c == CONTENT_AIR || c == CONTENT_IGNORE)
						return; // do not melt when falling (dirt->dirt_with_grass on air)
				}
				n.freeze_melt(ndef, +1);
				map->setNode(p, n);
				//env->getScriptIface()->node_falling_update(p); //enable after making FAST nodeupdate
			}
		}
};

class MeltHot : public ActiveBlockModifier {
	public:
		MeltHot(ServerEnvironment *env, INodeDefManager *nodemgr) { }
		virtual std::set<std::string> getTriggerContents() {
			std::set<std::string> s;
			s.insert("group:melt");
			return s;
		}
		virtual std::set<std::string> getRequiredNeighbors(bool activate) {
			std::set<std::string> s;
			s.insert("group:igniter");
			s.insert("group:hot");
			return s;
		}
		virtual u32 getNeighborsRange()
		{ return 3; }
		virtual float getTriggerInterval()
		{ return 10; }
		virtual u32 getTriggerChance()
		{ return 5; }
		virtual void trigger(ServerEnvironment *env, v3POS p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) {
			ServerMap *map = &env->getServerMap();
			INodeDefManager *ndef = env->getGameDef()->ndef();
			int hot = ((ItemGroupList) ndef->get(neighbor).groups)["hot"];
			int melt = ((ItemGroupList) ndef->get(n).groups)["melt"];
			if (hot > melt) {
				n.freeze_melt(ndef, +1);
				map->setNode(p, n);
				env->getScriptIface()->node_falling_update(p);
			}
		}
};

class LiquidFreezeCold : public ActiveBlockModifier {
	public:
		LiquidFreezeCold(ServerEnvironment *env, INodeDefManager *nodemgr) { }
		virtual std::set<std::string> getTriggerContents() {
			std::set<std::string> s;
			s.insert("group:freeze");
			return s;
		}
		virtual std::set<std::string> getRequiredNeighbors(bool activate) {
			std::set<std::string> s;
			s.insert("group:cold");
			return s;
		}
		virtual u32 getNeighborsRange()
		{ return 2; }
		virtual float getTriggerInterval()
		{ return 10; }
		virtual u32 getTriggerChance()
		{ return 4; }
		virtual void trigger(ServerEnvironment *env, v3POS p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate) {
			ServerMap *map = &env->getServerMap();
			INodeDefManager *ndef = env->getGameDef()->ndef();
			int cold = ((ItemGroupList) ndef->get(neighbor).groups)["cold"];
			int freeze = ((ItemGroupList) ndef->get(n).groups)["freeze"];
			if (cold < freeze) {
				n.freeze_melt(ndef, -1);
				map->setNode(p, n);
			}
		}
};

void add_legacy_abms(ServerEnvironment *env, INodeDefManager *nodedef) {
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
