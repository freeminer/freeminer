/*
script/lua_api/l_noise.h
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

#ifndef L_NOISE_H_
#define L_NOISE_H_

#include "lua_api/l_base.h"
#include "irr_v3d.h"
#include "noise.h"

/*
	LuaPerlinNoise
*/
class LuaPerlinNoise : public ModApiBase {
private:
	NoiseParams np;
	static const char className[];
	static const luaL_reg methods[];

	// Exported functions

	// garbage collector
	static int gc_object(lua_State *L);

	static int l_get2d(lua_State *L);
	static int l_get3d(lua_State *L);

public:
	LuaPerlinNoise(NoiseParams *params);
	~LuaPerlinNoise();

	// LuaPerlinNoise(seed, octaves, persistence, scale)
	// Creates an LuaPerlinNoise and leaves it on top of stack
	static int create_object(lua_State *L);

	static LuaPerlinNoise *checkobject(lua_State *L, int narg);

	static void Register(lua_State *L);
};

/*
	LuaPerlinNoiseMap
*/
class LuaPerlinNoiseMap : public ModApiBase {
	NoiseParams np;
	Noise *noise;
	bool m_is3d;
	static const char className[];
	static const luaL_reg methods[];

	// Exported functions

	// garbage collector
	static int gc_object(lua_State *L);

	static int l_get2dMap(lua_State *L);
	static int l_get2dMap_flat(lua_State *L);
	static int l_get3dMap(lua_State *L);
	static int l_get3dMap_flat(lua_State *L);

public:
	LuaPerlinNoiseMap(NoiseParams *np, int seed, v3s16 size);

	~LuaPerlinNoiseMap();

	// LuaPerlinNoiseMap(np, size)
	// Creates an LuaPerlinNoiseMap and leaves it on top of stack
	static int create_object(lua_State *L);

	static LuaPerlinNoiseMap *checkobject(lua_State *L, int narg);

	static void Register(lua_State *L);
};

/*
	LuaPseudoRandom
*/
class LuaPseudoRandom : public ModApiBase {
private:
	PseudoRandom m_pseudo;

	static const char className[];
	static const luaL_reg methods[];

	// Exported functions

	// garbage collector
	static int gc_object(lua_State *L);

	// next(self, min=0, max=32767) -> get next value
	static int l_next(lua_State *L);

public:
	LuaPseudoRandom(int seed) :
		m_pseudo(seed) {}

	// LuaPseudoRandom(seed)
	// Creates an LuaPseudoRandom and leaves it on top of stack
	static int create_object(lua_State *L);

	static LuaPseudoRandom *checkobject(lua_State *L, int narg);

	static void Register(lua_State *L);
};

/*
	LuaPcgRandom
*/
class LuaPcgRandom : public ModApiBase {
private:
	PcgRandom m_rnd;

	static const char className[];
	static const luaL_reg methods[];

	// Exported functions

	// garbage collector
	static int gc_object(lua_State *L);

	// next(self, min=-2147483648, max=2147483647) -> get next value
	static int l_next(lua_State *L);

	// rand_normal_dist(self, min=-2147483648, max=2147483647, num_trials=6) ->
	// get next normally distributed random value
	static int l_rand_normal_dist(lua_State *L);

public:
	LuaPcgRandom(u64 seed) :
		m_rnd(seed) {}
	LuaPcgRandom(u64 seed, u64 seq) :
		m_rnd(seed, seq) {}

	// LuaPcgRandom(seed)
	// Creates an LuaPcgRandom and leaves it on top of stack
	static int create_object(lua_State *L);

	static LuaPcgRandom *checkobject(lua_State *L, int narg);

	static void Register(lua_State *L);
};

#endif /* L_NOISE_H_ */
