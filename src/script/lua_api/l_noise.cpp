/*
script/lua_api/l_noise.cpp
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

#include "lua_api/l_noise.h"
#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "log.h"

// garbage collector
int LuaPerlinNoise::gc_object(lua_State *L)
{
	LuaPerlinNoise *o = *(LuaPerlinNoise **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


int LuaPerlinNoise::l_get2d(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaPerlinNoise *o = checkobject(L, 1);
	v2f p = read_v2f(L, 2);
	lua_Number val = NoisePerlin2D(&o->np, p.X, p.Y, 0);
	lua_pushnumber(L, val);
	return 1;
}


int LuaPerlinNoise::l_get3d(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaPerlinNoise *o = checkobject(L, 1);
	v3f p = read_v3f(L, 2);
	lua_Number val = NoisePerlin3D(&o->np, p.X, p.Y, p.Z, 0);
	lua_pushnumber(L, val);
	return 1;
}


LuaPerlinNoise::LuaPerlinNoise(NoiseParams *params) :
	np(*params)
{
}


LuaPerlinNoise::~LuaPerlinNoise()
{
}


// LuaPerlinNoise(seed, octaves, persistence, scale)
// Creates an LuaPerlinNoise and leaves it on top of stack
int LuaPerlinNoise::create_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	NoiseParams params;

	if (lua_istable(L, 1)) {
		read_noiseparams(L, 1, &params);
	} else {
		params.seed    = luaL_checkint(L, 1);
		params.octaves = luaL_checkint(L, 2);
		params.persist = luaL_checknumber(L, 3);
		params.spread  = v3f(1, 1, 1) * luaL_checknumber(L, 4);
	}

	LuaPerlinNoise *o = new LuaPerlinNoise(&params);

	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


LuaPerlinNoise* LuaPerlinNoise::checkobject(lua_State *L, int narg)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, className);
	if (!ud)
		luaL_typerror(L, narg, className);
	return *(LuaPerlinNoise**)ud;  // unbox pointer
}


void LuaPerlinNoise::Register(lua_State *L)
{
	lua_newtable(L);
	int methodtable = lua_gettop(L);
	luaL_newmetatable(L, className);
	int metatable = lua_gettop(L);

	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

	lua_pushliteral(L, "__index");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);

	lua_pushliteral(L, "__gc");
	lua_pushcfunction(L, gc_object);
	lua_settable(L, metatable);

	lua_pop(L, 1);  // drop metatable

	luaL_openlib(L, 0, methods, 0);  // fill methodtable
	lua_pop(L, 1);  // drop methodtable

	// Can be created from Lua (PerlinNoise(seed, octaves, persistence)
	lua_register(L, className, create_object);
}


const char LuaPerlinNoise::className[] = "PerlinNoise";
const luaL_reg LuaPerlinNoise::methods[] = {
	luamethod(LuaPerlinNoise, get2d),
	luamethod(LuaPerlinNoise, get3d),
	{0,0}
};


/*
  PerlinNoiseMap
 */

int LuaPerlinNoiseMap::gc_object(lua_State *L)
{
	LuaPerlinNoiseMap *o = *(LuaPerlinNoiseMap **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


int LuaPerlinNoiseMap::l_get2dMap(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	size_t i = 0;

	LuaPerlinNoiseMap *o = checkobject(L, 1);
	v2f p = read_v2f(L, 2);

	Noise *n = o->noise;
	n->perlinMap2D(p.X, p.Y);

	lua_newtable(L);
	for (int y = 0; y != n->sy; y++) {
		lua_newtable(L);
		for (int x = 0; x != n->sx; x++) {
			lua_pushnumber(L, n->result[i++]);
			lua_rawseti(L, -2, x + 1);
		}
		lua_rawseti(L, -2, y + 1);
	}
	return 1;
}


int LuaPerlinNoiseMap::l_get2dMap_flat(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPerlinNoiseMap *o = checkobject(L, 1);
	v2f p = read_v2f(L, 2);

	Noise *n = o->noise;
	n->perlinMap2D(p.X, p.Y);

	size_t maplen = n->sx * n->sy;

	lua_newtable(L);
	for (size_t i = 0; i != maplen; i++) {
		lua_pushnumber(L, n->result[i]);
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}


int LuaPerlinNoiseMap::l_get3dMap(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	size_t i = 0;

	LuaPerlinNoiseMap *o = checkobject(L, 1);
	v3f p = read_v3f(L, 2);

	if (!o->m_is3d)
		return 0;

	Noise *n = o->noise;
	n->perlinMap3D(p.X, p.Y, p.Z);

	lua_newtable(L);
	for (int z = 0; z != n->sz; z++) {
		lua_newtable(L);
		for (int y = 0; y != n->sy; y++) {
			lua_newtable(L);
			for (int x = 0; x != n->sx; x++) {
				lua_pushnumber(L, n->result[i++]);
				lua_rawseti(L, -2, x + 1);
			}
			lua_rawseti(L, -2, y + 1);
		}
		lua_rawseti(L, -2, z + 1);
	}
	return 1;
}


int LuaPerlinNoiseMap::l_get3dMap_flat(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	LuaPerlinNoiseMap *o = checkobject(L, 1);
	v3f p = read_v3f(L, 2);

	if (!o->m_is3d)
		return 0;

	Noise *n = o->noise;
	n->perlinMap3D(p.X, p.Y, p.Z);

	size_t maplen = n->sx * n->sy * n->sz;

	lua_newtable(L);
	for (size_t i = 0; i != maplen; i++) {
		lua_pushnumber(L, n->result[i]);
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}


LuaPerlinNoiseMap::LuaPerlinNoiseMap(NoiseParams *params, int seed, v3s16 size)
{
	m_is3d = size.Z > 1;
	np = *params;
	try {
		noise = new Noise(&np, seed, size.X, size.Y, size.Z);
	} catch (InvalidNoiseParamsException &e) {
		throw LuaError(e.what());
	}
}


LuaPerlinNoiseMap::~LuaPerlinNoiseMap()
{
	delete noise;
}


// LuaPerlinNoiseMap(np, size)
// Creates an LuaPerlinNoiseMap and leaves it on top of stack
int LuaPerlinNoiseMap::create_object(lua_State *L)
{
	NoiseParams np;
	if (!read_noiseparams(L, 1, &np))
		return 0;
	v3s16 size = read_v3s16(L, 2);

	LuaPerlinNoiseMap *o = new LuaPerlinNoiseMap(&np, 0, size);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


LuaPerlinNoiseMap *LuaPerlinNoiseMap::checkobject(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);

	void *ud = luaL_checkudata(L, narg, className);
	if (!ud)
		luaL_typerror(L, narg, className);

	return *(LuaPerlinNoiseMap **)ud;  // unbox pointer
}


void LuaPerlinNoiseMap::Register(lua_State *L)
{
	lua_newtable(L);
	int methodtable = lua_gettop(L);
	luaL_newmetatable(L, className);
	int metatable = lua_gettop(L);

	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

	lua_pushliteral(L, "__index");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);

	lua_pushliteral(L, "__gc");
	lua_pushcfunction(L, gc_object);
	lua_settable(L, metatable);

	lua_pop(L, 1);  // drop metatable

	luaL_openlib(L, 0, methods, 0);  // fill methodtable
	lua_pop(L, 1);  // drop methodtable

	// Can be created from Lua (PerlinNoiseMap(np, size)
	lua_register(L, className, create_object);
}


const char LuaPerlinNoiseMap::className[] = "PerlinNoiseMap";
const luaL_reg LuaPerlinNoiseMap::methods[] = {
	luamethod(LuaPerlinNoiseMap, get2dMap),
	luamethod(LuaPerlinNoiseMap, get2dMap_flat),
	luamethod(LuaPerlinNoiseMap, get3dMap),
	luamethod(LuaPerlinNoiseMap, get3dMap_flat),
	{0,0}
};

/*
	LuaPseudoRandom
*/

// garbage collector
int LuaPseudoRandom::gc_object(lua_State *L)
{
	LuaPseudoRandom *o = *(LuaPseudoRandom **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}


// next(self, min=0, max=32767) -> get next value
int LuaPseudoRandom::l_next(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	LuaPseudoRandom *o = checkobject(L, 1);
	int min = 0;
	int max = 32767;
	lua_settop(L, 3); // Fill 2 and 3 with nil if they don't exist
	if(!lua_isnil(L, 2))
		min = luaL_checkinteger(L, 2);
	if(!lua_isnil(L, 3))
		max = luaL_checkinteger(L, 3);
	if(max < min){
		errorstream<<"PseudoRandom.next(): max="<<max<<" min="<<min<<std::endl;
		throw LuaError("PseudoRandom.next(): max < min");
	}
	if(max - min != 32767 && max - min > 32767/5)
		throw LuaError("PseudoRandom.next() max-min is not 32767"
				" and is > 32768/5. This is disallowed due to"
				" the bad random distribution the"
				" implementation would otherwise make.");
	PseudoRandom &pseudo = o->m_pseudo;
	int val = pseudo.next();
	val = (val % (max-min+1)) + min;
	lua_pushinteger(L, val);
	return 1;
}


LuaPseudoRandom::LuaPseudoRandom(int seed):
	m_pseudo(seed)
{
}


LuaPseudoRandom::~LuaPseudoRandom()
{
}


const PseudoRandom& LuaPseudoRandom::getItem() const
{
	return m_pseudo;
}

PseudoRandom& LuaPseudoRandom::getItem()
{
	return m_pseudo;
}


// LuaPseudoRandom(seed)
// Creates an LuaPseudoRandom and leaves it on top of stack
int LuaPseudoRandom::create_object(lua_State *L)
{
	int seed = luaL_checknumber(L, 1);
	LuaPseudoRandom *o = new LuaPseudoRandom(seed);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}


LuaPseudoRandom* LuaPseudoRandom::checkobject(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, className);
	if (!ud)
		luaL_typerror(L, narg, className);
	return *(LuaPseudoRandom**)ud;  // unbox pointer
}


void LuaPseudoRandom::Register(lua_State *L)
{
	lua_newtable(L);
	int methodtable = lua_gettop(L);
	luaL_newmetatable(L, className);
	int metatable = lua_gettop(L);

	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

	lua_pushliteral(L, "__index");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);

	lua_pushliteral(L, "__gc");
	lua_pushcfunction(L, gc_object);
	lua_settable(L, metatable);

	lua_pop(L, 1);  // drop metatable

	luaL_openlib(L, 0, methods, 0);  // fill methodtable
	lua_pop(L, 1);  // drop methodtable

	// Can be created from Lua (LuaPseudoRandom(seed))
	lua_register(L, className, create_object);
}


const char LuaPseudoRandom::className[] = "PseudoRandom";
const luaL_reg LuaPseudoRandom::methods[] = {
	luamethod(LuaPseudoRandom, next),
	{0,0}
};
