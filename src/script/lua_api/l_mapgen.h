/*
script/lua_api/l_mapgen.h
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

#ifndef L_MAPGEN_H_
#define L_MAPGEN_H_

#include "lua_api/l_base.h"

class INodeDefManager;
class NodeResolver;
class DecoSimple;
class DecoSchematic;

class ModApiMapgen : public ModApiBase {
private:
	// get_mapgen_object(objectname)
	// returns the requested object used during map generation
	static int l_get_mapgen_object(lua_State *L);

	// set_mapgen_params(params)
	// set mapgen parameters
	static int l_set_mapgen_params(lua_State *L);

	// set_noiseparam_defaults({np1={noise params}, ...})
	static int l_set_noiseparam_defaults(lua_State *L);

	// set_gen_notify(flagstring)
	static int l_set_gen_notify(lua_State *L);

	// register_biome({lots of stuff})
	static int l_register_biome(lua_State *L);

	// register_decoration({lots of stuff})
	static int l_register_decoration(lua_State *L);

	// register_ore({lots of stuff})
	static int l_register_ore(lua_State *L);

	// create_schematic(p1, p2, probability_list, filename)
	static int l_create_schematic(lua_State *L);

	// place_schematic(p, schematic, rotation, replacement)
	static int l_place_schematic(lua_State *L);

	static bool regDecoSimple(lua_State *L,
			NodeResolver *resolver, DecoSimple *deco);
	static bool regDecoSchematic(lua_State *L,
			INodeDefManager *ndef, DecoSchematic *deco);

	static struct EnumString es_BiomeTerrainType[];
	static struct EnumString es_DecorationType[];
	static struct EnumString es_MapgenObject[];
	static struct EnumString es_OreType[];
	static struct EnumString es_Rotation[];

public:
	static void Initialize(lua_State *L, int top);
};



#endif /* L_MAPGEN_H_ */
