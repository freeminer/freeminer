/*
script/lua_api/l_particles.cpp
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

#include "lua_api/l_particles.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "server.h"

// add_particle({pos=, velocity=, acceleration=, expirationtime=,
// 		size=, collisiondetection=, vertical=, texture=, player=})
// pos/velocity/acceleration = {x=num, y=num, z=num}
// expirationtime = num (seconds)
// size = num
// collisiondetection = bool
// vertical = bool
// texture = e.g."default_wood.png"
int ModApiParticles::l_add_particle(lua_State *L)
{
	// Get parameters
	v3f pos, vel, acc;
	pos = vel = acc = v3f(0, 0, 0);

	float expirationtime, size;
	expirationtime = size = 1;

	bool collisiondetection, vertical;
	collisiondetection = vertical = false;

	std::string texture = "";
	std::string playername = "";

	if (lua_gettop(L) > 1) // deprecated
	{
		log_deprecated(L, "Deprecated add_particle call with individual parameters instead of definition");
		pos = check_v3f(L, 1);
		vel = check_v3f(L, 2);
		acc = check_v3f(L, 3);
		expirationtime = luaL_checknumber(L, 4);
		size = luaL_checknumber(L, 5);
		collisiondetection = lua_toboolean(L, 6);
		texture = luaL_checkstring(L, 7);
		if (lua_gettop(L) == 8) // only spawn for a single player
			playername = luaL_checkstring(L, 8);
	}
	else if (lua_istable(L, 1))
	{
		lua_getfield(L, 1, "pos");
		pos = lua_istable(L, -1) ? check_v3f(L, -1) : v3f();
		lua_pop(L, 1);

		lua_getfield(L, 1, "vel");
		if (lua_istable(L, -1)) {
			vel = check_v3f(L, -1);
			log_deprecated(L, "The use of vel is deprecated. "
				"Use velocity instead");
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "velocity");
		vel = lua_istable(L, -1) ? check_v3f(L, -1) : vel;
		lua_pop(L, 1);

		lua_getfield(L, 1, "acc");
		if (lua_istable(L, -1)) {
			acc = check_v3f(L, -1);
			log_deprecated(L, "The use of acc is deprecated. "
				"Use acceleration instead");
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "acceleration");
		acc = lua_istable(L, -1) ? check_v3f(L, -1) : acc;
		lua_pop(L, 1);

		expirationtime = getfloatfield_default(L, 1, "expirationtime", 1);
		size = getfloatfield_default(L, 1, "size", 1);
		collisiondetection = getboolfield_default(L, 1,
			"collisiondetection", collisiondetection);
		vertical = getboolfield_default(L, 1, "vertical", vertical);
		texture = getstringfield_default(L, 1, "texture", "");
		playername = getstringfield_default(L, 1, "playername", "");
	}
	getServer(L)->spawnParticle(playername, pos, vel, acc,
			expirationtime, size, collisiondetection, vertical, texture);
	return 1;
}

// add_particlespawner({amount=, time=,
//				minpos=, maxpos=,
//				minvel=, maxvel=,
//				minacc=, maxacc=,
//				minexptime=, maxexptime=,
//				minsize=, maxsize=,
//				collisiondetection=,
//				vertical=,
//				texture=,
//				player=})
// minpos/maxpos/minvel/maxvel/minacc/maxacc = {x=num, y=num, z=num}
// minexptime/maxexptime = num (seconds)
// minsize/maxsize = num
// collisiondetection = bool
// vertical = bool
// texture = e.g."default_wood.png"
int ModApiParticles::l_add_particlespawner(lua_State *L)
{
	// Get parameters
	u16 amount = 1;
	v3f minpos, maxpos, minvel, maxvel, minacc, maxacc;
	    minpos= maxpos= minvel= maxvel= minacc= maxacc= v3f(0, 0, 0);
	float time, minexptime, maxexptime, minsize, maxsize;
	      time= minexptime= maxexptime= minsize= maxsize= 1;
	bool collisiondetection, vertical;
	     collisiondetection= vertical= false;
	std::string texture = "";
	std::string playername = "";

	if (lua_gettop(L) > 1) //deprecated
	{
		log_deprecated(L,"Deprecated add_particlespawner call with individual parameters instead of definition");
		amount = luaL_checknumber(L, 1);
		time = luaL_checknumber(L, 2);
		minpos = check_v3f(L, 3);
		maxpos = check_v3f(L, 4);
		minvel = check_v3f(L, 5);
		maxvel = check_v3f(L, 6);
		minacc = check_v3f(L, 7);
		maxacc = check_v3f(L, 8);
		minexptime = luaL_checknumber(L, 9);
		maxexptime = luaL_checknumber(L, 10);
		minsize = luaL_checknumber(L, 11);
		maxsize = luaL_checknumber(L, 12);
		collisiondetection = lua_toboolean(L, 13);
		texture = luaL_checkstring(L, 14);
		if (lua_gettop(L) == 15) // only spawn for a single player
			playername = luaL_checkstring(L, 15);
	}
	else if (lua_istable(L, 1))
	{
		amount = getintfield_default(L, 1, "amount", amount);
		time = getfloatfield_default(L, 1, "time", time);

		lua_getfield(L, 1, "minpos");
		minpos = lua_istable(L, -1) ? check_v3f(L, -1) : minpos;
		lua_pop(L, 1);

		lua_getfield(L, 1, "maxpos");
		maxpos = lua_istable(L, -1) ? check_v3f(L, -1) : maxpos;
		lua_pop(L, 1);

		lua_getfield(L, 1, "minvel");
		minvel = lua_istable(L, -1) ? check_v3f(L, -1) : minvel;
		lua_pop(L, 1);

		lua_getfield(L, 1, "maxvel");
		maxvel = lua_istable(L, -1) ? check_v3f(L, -1) : maxvel;
		lua_pop(L, 1);

		lua_getfield(L, 1, "minacc");
		minacc = lua_istable(L, -1) ? check_v3f(L, -1) : minacc;
		lua_pop(L, 1);

		lua_getfield(L, 1, "maxacc");
		maxacc = lua_istable(L, -1) ? check_v3f(L, -1) : maxacc;
		lua_pop(L, 1);

		minexptime = getfloatfield_default(L, 1, "minexptime", minexptime);
		maxexptime = getfloatfield_default(L, 1, "maxexptime", maxexptime);
		minsize = getfloatfield_default(L, 1, "minsize", minsize);
		maxsize = getfloatfield_default(L, 1, "maxsize", maxsize);
		collisiondetection = getboolfield_default(L, 1,
			"collisiondetection", collisiondetection);
		vertical = getboolfield_default(L, 1, "vertical", vertical);
		texture = getstringfield_default(L, 1, "texture", "");
		playername = getstringfield_default(L, 1, "playername", "");
	}

	u32 id = getServer(L)->addParticleSpawner(amount, time,
			minpos, maxpos,
			minvel, maxvel,
			minacc, maxacc,
			minexptime, maxexptime,
			minsize, maxsize,
			collisiondetection,
			vertical,
			texture, playername);
	lua_pushnumber(L, id);

	return 1;
}

// delete_particlespawner(id, player)
// player (string) is optional
int ModApiParticles::l_delete_particlespawner(lua_State *L)
{
	// Get parameters
	u32 id = luaL_checknumber(L, 1);
	std::string playername = "";
	if (lua_gettop(L) == 2) {
		playername = luaL_checkstring(L, 2);
	}

	getServer(L)->deleteParticleSpawner(playername, id);
	return 1;
}

void ModApiParticles::Initialize(lua_State *L, int top)
{
	API_FCT(add_particle);
	API_FCT(add_particlespawner);
	API_FCT(delete_particlespawner);
}

