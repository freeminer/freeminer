/*
 * Epixel
 * Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include "script/lua_api/l_env.h"
#include "irr_v3d.h"
#include "script/lua_api/l_internal.h"
#include "script/lua_api/l_base.h"
#include "script/common/c_converter.h"
#include "script/common/c_content.h"
#if 0
#include "contrib/creature.h"
#endif
#include "contrib/itemsao.h"
#if 0
#include "contrib/playersao.h"
#endif
//#include "scripting_game.h"
#include "environment.h"
//#include "content_sao.h"
#include "nodedef.h"
#include "server.h"

#if 0
int ModApiEnvMod::l_add_creature(lua_State *L)
{
	GET_ENV_PTR;

	// pos
	v3f pos = checkFloatPos(L, 1);
	// content
	std::string name = luaL_checkstring(L, 2);
	// Do it
	epixel::Creature *obj = epixel::Creature::createCreatureObj(env, pos, name);
	int objectid = env->addActiveObject(obj);
	// If failed to add, return nothing (reads as nil)
	if(objectid == 0)
		return 0;
	// Return ObjectRef
	getScriptApiBase(L)->objectrefGetOrCreate(L, obj);
	return 1;
}

#endif

int ModApiEnv::l_spawn_item_activeobject(lua_State *L)
{
	GET_ENV_PTR;
	// pos
	v3opos_t pos = checkOposPos(L, 1);
	// item
	//std::string itemstring = lua_tostring(L, 2);
	ItemStack item = read_item(L, 2,getServer(L)->idef());
	if(item.empty() || !item.isKnown(getServer(L)->idef()))
		return 0;

	u16 stacksize = 1;
	if (lua_isnumber(L, 3)) {
		stacksize = lua_tonumber(L, 3);
	}

	v3f v = v3f(myrand_range(-1, 1) * BS, 5 * BS, myrand_range(-1, 1) * BS);
	if (lua_istable(L, 4)) {
		v = checkFloatPos(L, 4);
	}

	/*
	ItemStack item;
	item.deSerialize(itemstring);
	*/
	item.add(stacksize - 1);

	// Drop item on the floor
	if (auto obj = env->spawnItemActiveObject(item.getItemString(), pos, item)) {
		obj->setVelocity(v);
	}
	return 1;
}

int ModApiEnv::l_spawn_falling_node(lua_State *L)
{
	GET_ENV_PTR;

	auto *ndef = env->getGameDef()->ndef();

	// pos
	v3opos_t pos = checkOposPos(L, 1);
	MapNode n = readnode(L, 2);

	// Drop item on the floor
	env->spawnFallingActiveObject(ndef->get(n).name, pos, n);
	return 1;
}

/**
 * @brief ModApiEnvMod::l_nodeupdate
 * @param L
 * @return always 1
 *
 * Trigger a node update on selected position
 */
int ModApiEnv::l_nodeupdate(lua_State *L)
{
	GET_ENV_PTR;

	// pos
	v3f pos = checkFloatPos(L, 1);
	int destroy = luaL_optnumber(L, 2, 0);

	// Drop item on the floor
	env->nodeUpdate(floatToInt(pos, BS), 5, 1, destroy);
	return 1;
}

#if 0
/**
 * @brief ObjectRef::l_set_timeofday
 * @param L
 * @return  0 on fail, 1 on success
 *
 * Set time of day if the player have 'time' privilege
 * This usage is reserved to mods which needs some flexibility
 */
int ModApiEnvMod::l_set_timeofday(lua_State *L)
{
	GET_ENV_PTR;

	if (!lua_isnone(L, 2)) {
		const char *name = luaL_checkstring(L, 2);
		RemotePlayer *player = env->getPlayer(name);
		if(player == NULL) {
			lua_pushnil(L);
			return 0;
		}

		PlayerSAO *sao = player->getPlayerSAO();
		if(sao == NULL) {
			lua_pushnil(L);
			return 0;
		}

		if (!sao->hasPriv("time")){
			getServer(L)->SendChatMessage(sao->getPeerID(), L"You don't have the 'time' privilege");
			return 1;
		}

	}

	// Compatibility with other mods
	float timeofday_f = luaL_checknumber(L, 1);
	sanity_check(timeofday_f >= 0.0 && timeofday_f <= 1.0);
	int timeofday_mh = (int)(timeofday_f * 24000.0);
	getServer(L)->setTimeOfDay(timeofday_mh);
	return 1;
}

int ModApiEnvMod::l_make_explosion(lua_State *L)
{
	GET_ENV_PTR;

	// pos
	v3f pos = checkFloatPos(L, 1);
	float radius = luaL_checknumber(L, 2);

	// Drop item on the floor
	env->makeExplosion(floatToInt(pos, BS), radius, 0);
	return 1;
}

#endif
