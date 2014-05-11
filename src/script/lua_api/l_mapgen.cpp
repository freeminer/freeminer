/*
script/lua_api/l_mapgen.cpp
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

#include "lua_api/l_mapgen.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_vmanip.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"
#include "environment.h"
#include "biome.h"
#include "emerge.h"
#include "mapgen_v7.h"
#include "main.h"


struct EnumString ModApiMapgen::es_BiomeTerrainType[] =
{
	{BIOME_TERRAIN_NORMAL, "normal"},
	{BIOME_TERRAIN_LIQUID, "liquid"},
	{BIOME_TERRAIN_NETHER, "nether"},
	{BIOME_TERRAIN_AETHER, "aether"},
	{BIOME_TERRAIN_FLAT,   "flat"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_DecorationType[] =
{
	{DECO_SIMPLE,    "simple"},
	{DECO_SCHEMATIC, "schematic"},
	{DECO_LSYSTEM,   "lsystem"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_MapgenObject[] =
{
	{MGOBJ_VMANIP,    "voxelmanip"},
	{MGOBJ_HEIGHTMAP, "heightmap"},
	{MGOBJ_BIOMEMAP,  "biomemap"},
	{MGOBJ_HEATMAP,   "heatmap"},
	{MGOBJ_HUMIDMAP,  "humiditymap"},
	{MGOBJ_GENNOTIFY, "gennotify"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_OreType[] =
{
	{ORE_SCATTER,  "scatter"},
	{ORE_SHEET,    "sheet"},
	{ORE_CLAYLIKE, "claylike"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_Rotation[] =
{
	{ROTATE_0,    "0"},
	{ROTATE_90,   "90"},
	{ROTATE_180,  "180"},
	{ROTATE_270,  "270"},
	{ROTATE_RAND, "random"},
	{0, NULL},
};


// get_mapgen_object(objectname)
// returns the requested object used during map generation
int ModApiMapgen::l_get_mapgen_object(lua_State *L)
{
	const char *mgobjstr = lua_tostring(L, 1);

	int mgobjint;
	if (!string_to_enum(es_MapgenObject, mgobjint, mgobjstr ? mgobjstr : ""))
		return 0;

	enum MapgenObject mgobj = (MapgenObject)mgobjint;

	EmergeManager *emerge = getServer(L)->getEmergeManager();
	Mapgen *mg = emerge->getCurrentMapgen();
	if (!mg)
		return 0;

	size_t maplen = mg->csize.X * mg->csize.Z;

	switch (mgobj) {
		case MGOBJ_VMANIP: {
			ManualMapVoxelManipulator *vm = mg->vm;

			// VoxelManip object
			LuaVoxelManip *o = new LuaVoxelManip(vm, true);
			*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
			luaL_getmetatable(L, "VoxelManip");
			lua_setmetatable(L, -2);

			// emerged min pos
			push_v3s16(L, vm->m_area.MinEdge);

			// emerged max pos
			push_v3s16(L, vm->m_area.MaxEdge);

			return 3; }
		case MGOBJ_HEIGHTMAP: {
			if (!mg->heightmap)
				return 0;

			lua_newtable(L);
			for (size_t i = 0; i != maplen; i++) {
				lua_pushinteger(L, mg->heightmap[i]);
				lua_rawseti(L, -2, i + 1);
			}

			return 1; }
		case MGOBJ_BIOMEMAP: {
			if (!mg->biomemap)
				return 0;

			lua_newtable(L);
			for (size_t i = 0; i != maplen; i++) {
				lua_pushinteger(L, mg->biomemap[i]);
				lua_rawseti(L, -2, i + 1);
			}

			return 1; }
		case MGOBJ_HEATMAP: { // Mapgen V7 specific objects
		case MGOBJ_HUMIDMAP:
			if (strcmp(emerge->params.mg_name.c_str(), "v7"))
				return 0;

			MapgenV7 *mgv7 = (MapgenV7 *)mg;

			float *arr = (mgobj == MGOBJ_HEATMAP) ?
				mgv7->noise_heat->result : mgv7->noise_humidity->result;
			if (!arr)
				return 0;

			lua_newtable(L);
			for (size_t i = 0; i != maplen; i++) {
				lua_pushnumber(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}

			return 1; }
		case MGOBJ_GENNOTIFY: {
			lua_newtable(L);
			for (int i = 0; flagdesc_gennotify[i].name; i++) {
				if (!(emerge->gennotify & flagdesc_gennotify[i].flag))
					continue;

				std::vector<v3s16> *posvec = mg->gen_notifications[i];
				if (!posvec)
					return 0;

				lua_newtable(L);
				for (unsigned int j = 0; j != posvec->size(); j++) {
					push_v3s16(L, (*posvec)[j]);
					lua_rawseti(L, -2, j + 1);
				}
				lua_setfield(L, -2, flagdesc_gennotify[i].name);

				posvec->clear();
			}

			return 1; }
	}

	return 0;
}

// set_mapgen_params(params)
// set mapgen parameters
int ModApiMapgen::l_set_mapgen_params(lua_State *L)
{
	if (!lua_istable(L, 1))
		return 0;

	EmergeManager *emerge = getServer(L)->getEmergeManager();
	assert(emerge);

	std::string flagstr;
	u32 flags = 0, flagmask = 0;

	lua_getfield(L, 1, "mgname");
	if (lua_isstring(L, -1)) {
		emerge->params.mg_name = std::string(lua_tostring(L, -1));
		delete emerge->params.sparams;
		emerge->params.sparams = NULL;
	}

	lua_getfield(L, 1, "seed");
	if (lua_isnumber(L, -1))
		emerge->params.seed = lua_tointeger(L, -1);

	lua_getfield(L, 1, "water_level");
	if (lua_isnumber(L, -1))
		emerge->params.water_level = lua_tointeger(L, -1);

	lua_getfield(L, 1, "flagmask");
	if (lua_isstring(L, -1)) {
		flagstr = lua_tostring(L, -1);
		emerge->params.flags &= ~readFlagString(flagstr, flagdesc_mapgen, NULL);
		errorstream << "set_mapgen_params(): flagmask field is deprecated, "
			"see lua_api.txt" << std::endl;
	}

	if (getflagsfield(L, 1, "flags", flagdesc_mapgen, &flags, &flagmask)) {
		emerge->params.flags &= ~flagmask;
		emerge->params.flags |= flags;
	}

	return 0;
}

// set_noiseparam_defaults({np1={noise params}, ...})
// set default values for noise parameters if not present in global settings
int ModApiMapgen::l_set_noiseparam_defaults(lua_State *L)
{
	NoiseParams np;
	std::string val, name;

	if (!lua_istable(L, 1))
		return 0;

	lua_pushnil(L);
	while (lua_next(L, 1)) {
		if (read_noiseparams_nc(L, -1, &np)) {
			if (!serializeStructToString(&val, NOISEPARAMS_FMT_STR, &np))
				continue;
			if (!lua_isstring(L, -2))
				continue;

			name = lua_tostring(L, -2);
			g_settings->setDefault(name, val);
		}
		lua_pop(L, 1);
	}

	return 0;
}

// set_gen_notify(string)
int ModApiMapgen::l_set_gen_notify(lua_State *L)
{
	u32 flags = 0, flagmask = 0;

	if (read_flags(L, 1, flagdesc_gennotify, &flags, &flagmask)) {
		EmergeManager *emerge = getServer(L)->getEmergeManager();
		emerge->gennotify = flags;
	}

	return 0;
}

// register_biome({lots of stuff})
int ModApiMapgen::l_register_biome(lua_State *L)
{
	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	BiomeDefManager *bmgr = getServer(L)->getEmergeManager()->biomedef;
	if (!bmgr) {
		verbosestream << "register_biome: BiomeDefManager not active" << std::endl;
		return 0;
	}

	enum BiomeTerrainType terrain = (BiomeTerrainType)getenumfield(L, index,
				"terrain_type", es_BiomeTerrainType, BIOME_TERRAIN_NORMAL);
	Biome *b = bmgr->createBiome(terrain);

	b->name         = getstringfield_default(L, index, "name",
												"<no name>");
	b->nname_top    = getstringfield_default(L, index, "node_top",
												"mapgen_dirt_with_grass");
	b->nname_filler = getstringfield_default(L, index, "node_filler",
												"mapgen_dirt");
	b->nname_water  = getstringfield_default(L, index, "node_water",
												"mapgen_water_source");
	b->nname_ice  = getstringfield_default(L, index, "node_ice",
												"mapgen_ice");
	b->nname_dust   = getstringfield_default(L, index, "node_dust",
												"air");
	b->nname_dust_water = getstringfield_default(L, index, "node_dust_water",
												"mapgen_water_source");

	b->depth_top      = getintfield_default(L, index, "depth_top",    1);
	b->depth_filler   = getintfield_default(L, index, "depth_filler", 3);
	b->height_min     = getintfield_default(L, index, "height_min",   0);
	b->height_max     = getintfield_default(L, index, "height_max",   0);
	b->heat_point     = getfloatfield_default(L, index, "heat_point",     0.);
	b->humidity_point = getfloatfield_default(L, index, "humidity_point", 0.);

	b->flags        = 0; //reserved
	b->c_top        = CONTENT_IGNORE;
	b->c_filler     = CONTENT_IGNORE;
	b->c_water      = CONTENT_IGNORE;
	b->c_ice        = CONTENT_IGNORE;
	b->c_dust       = CONTENT_IGNORE;
	b->c_dust_water = CONTENT_IGNORE;

	verbosestream << "register_biome: " << b->name << std::endl;
	bmgr->addBiome(b);

	return 0;
}

// register_decoration({lots of stuff})
int ModApiMapgen::l_register_decoration(lua_State *L)
{
	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	EmergeManager *emerge = getServer(L)->getEmergeManager();
	BiomeDefManager *bdef = emerge->biomedef;

	enum DecorationType decotype = (DecorationType)getenumfield(L, index,
				"deco_type", es_DecorationType, 0);
	if (decotype == 0) {
		errorstream << "register_decoration: unrecognized "
			"decoration placement type";
		return 0;
	}

	Decoration *deco = createDecoration(decotype);
	if (!deco) {
		errorstream << "register_decoration: decoration placement type "
			<< decotype << " not implemented";
		return 0;
	}

	deco->c_place_on    = CONTENT_IGNORE;
	deco->place_on_name = getstringfield_default(L, index, "place_on", "ignore");
	deco->fill_ratio    = getfloatfield_default(L, index, "fill_ratio", 0.02);
	deco->sidelen       = getintfield_default(L, index, "sidelen", 8);
	if (deco->sidelen <= 0) {
		errorstream << "register_decoration: sidelen must be "
			"greater than 0" << std::endl;
		delete deco;
		return 0;
	}

	lua_getfield(L, index, "noise_params");
	deco->np = read_noiseparams(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, index, "biomes");
	if (lua_istable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			const char *s = lua_tostring(L, -1);
			u8 biomeid = bdef->getBiomeIdByName(s);
			if (biomeid)
				deco->biomes.insert(biomeid);

			lua_pop(L, 1);
		}
		lua_pop(L, 1);
	}

	switch (decotype) {
		case DECO_SIMPLE: {
			DecoSimple *dsimple = (DecoSimple *)deco;
			dsimple->c_deco     = CONTENT_IGNORE;
			dsimple->c_spawnby  = CONTENT_IGNORE;
			dsimple->spawnby_name    = getstringfield_default(L, index, "spawn_by", "air");
			dsimple->deco_height     = getintfield_default(L, index, "height", 1);
			dsimple->deco_height_max = getintfield_default(L, index, "height_max", 0);
			dsimple->nspawnby        = getintfield_default(L, index, "num_spawn_by", -1);

			lua_getfield(L, index, "decoration");
			if (lua_istable(L, -1)) {
				lua_pushnil(L);
				while (lua_next(L, -2)) {
					const char *s = lua_tostring(L, -1);
					std::string str(s);
					dsimple->decolist_names.push_back(str);

					lua_pop(L, 1);
				}
			} else if (lua_isstring(L, -1)) {
				dsimple->deco_name = std::string(lua_tostring(L, -1));
			} else {
				dsimple->deco_name = std::string("air");
			}
			lua_pop(L, 1);

			if (dsimple->deco_height <= 0) {
				errorstream << "register_decoration: simple decoration height"
					" must be greater than 0" << std::endl;
				delete dsimple;
				return 0;
			}

			break; }
		case DECO_SCHEMATIC: {
			DecoSchematic *dschem = (DecoSchematic *)deco;

			dschem->flags = 0;
			getflagsfield(L, index, "flags", flagdesc_deco_schematic,
				&dschem->flags, NULL);

			dschem->rotation = (Rotation)getenumfield(L, index,
				"rotation", es_Rotation, ROTATE_0);

			lua_getfield(L, index, "replacements");
			if (lua_istable(L, -1)) {
				int i = lua_gettop(L);
				lua_pushnil(L);
				while (lua_next(L, i) != 0) {
					// key at index -2 and value at index -1
					lua_rawgeti(L, -1, 1);
					std::string replace_from = lua_tostring(L, -1);
					lua_pop(L, 1);
					lua_rawgeti(L, -1, 2);
					std::string replace_to = lua_tostring(L, -1);
					lua_pop(L, 1);
					dschem->replacements[replace_from] = replace_to;
					// removes value, keeps key for next iteration
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);

			lua_getfield(L, index, "schematic");
			if (!read_schematic(L, -1, dschem, getServer(L))) {
				delete dschem;
				return 0;
			}
			lua_pop(L, -1);

			if (!dschem->filename.empty() && !dschem->loadSchematicFile()) {
				errorstream << "register_decoration: failed to load schematic file '"
					<< dschem->filename << "'" << std::endl;
				delete dschem;
				return 0;
			}
			break; }
		case DECO_LSYSTEM: {
			//DecoLSystem *decolsystem = (DecoLSystem *)deco;

			break; }
	}

	emerge->decorations.push_back(deco);

	verbosestream << "register_decoration: decoration '" << deco->getName()
		<< "' registered" << std::endl;
	return 0;
}

// register_ore({lots of stuff})
int ModApiMapgen::l_register_ore(lua_State *L)
{
	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	EmergeManager *emerge = getServer(L)->getEmergeManager();

	enum OreType oretype = (OreType)getenumfield(L, index,
				"ore_type", es_OreType, ORE_SCATTER);
	Ore *ore = createOre(oretype);
	if (!ore) {
		errorstream << "register_ore: ore_type "
			<< oretype << " not implemented";
		return 0;
	}

	ore->ore_name       = getstringfield_default(L, index, "ore", "");
	ore->ore_param2     = (u8)getintfield_default(L, index, "ore_param2", 0);
	ore->clust_scarcity = getintfield_default(L, index, "clust_scarcity", 1);
	ore->clust_num_ores = getintfield_default(L, index, "clust_num_ores", 1);
	ore->clust_size     = getintfield_default(L, index, "clust_size", 0);
	ore->height_min     = getintfield_default(L, index, "height_min", 0);
	ore->height_max     = getintfield_default(L, index, "height_max", 0);
	ore->nthresh        = getfloatfield_default(L, index, "noise_threshhold", 0.);
	ore->flags          = 0;
	getflagsfield(L, index, "flags", flagdesc_ore, &ore->flags, NULL);

	lua_getfield(L, index, "wherein");
	if (lua_istable(L, -1)) {
		int  i = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, i) != 0) {
			ore->wherein_names.push_back(lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, -1)) {
		ore->wherein_names.push_back(lua_tostring(L, -1));
	} else {
		ore->wherein_names.push_back("");
	}
	lua_pop(L, 1);

	lua_getfield(L, index, "noise_params");
	ore->np = read_noiseparams(L, -1);
	lua_pop(L, 1);

	ore->noise = NULL;

	if (ore->clust_scarcity <= 0 || ore->clust_num_ores <= 0) {
		errorstream << "register_ore: clust_scarcity and clust_num_ores"
			"must be greater than 0" << std::endl;
		delete ore;
		return 0;
	}

	emerge->ores.push_back(ore);

	verbosestream << "register_ore: ore '" << ore->ore_name
		<< "' registered" << std::endl;
	return 0;
}

// create_schematic(p1, p2, probability_list, filename)
int ModApiMapgen::l_create_schematic(lua_State *L)
{
	DecoSchematic dschem;

	Map *map = &(getEnv(L)->getMap());
	INodeDefManager *ndef = getServer(L)->getNodeDefManager();

	v3s16 p1 = read_v3s16(L, 1);
	v3s16 p2 = read_v3s16(L, 2);
	sortBoxVerticies(p1, p2);

	std::vector<std::pair<v3s16, u8> > prob_list;
	if (lua_istable(L, 3)) {
		lua_pushnil(L);
		while (lua_next(L, 3)) {
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "pos");
				v3s16 pos = read_v3s16(L, -1);
				lua_pop(L, 1);

				u8 prob = getintfield_default(L, -1, "prob", MTSCHEM_PROB_ALWAYS);
				prob_list.push_back(std::make_pair(pos, prob));
			}

			lua_pop(L, 1);
		}
	}

	std::vector<std::pair<s16, u8> > slice_prob_list;
	if (lua_istable(L, 5)) {
		lua_pushnil(L);
		while (lua_next(L, 5)) {
			if (lua_istable(L, -1)) {
				s16 ypos = getintfield_default(L, -1, "ypos", 0);
				u8 prob  = getintfield_default(L, -1, "prob", MTSCHEM_PROB_ALWAYS);
				slice_prob_list.push_back(std::make_pair(ypos, prob));
			}

			lua_pop(L, 1);
		}
	}

	const char *s = lua_tostring(L, 4);
	dschem.filename = std::string(s ? s : "");

	if (!dschem.getSchematicFromMap(map, p1, p2)) {
		errorstream << "create_schematic: failed to get schematic "
			"from map" << std::endl;
		return 0;
	}

	dschem.applyProbabilities(p1, &prob_list, &slice_prob_list);

	dschem.saveSchematicFile(ndef);
	actionstream << "create_schematic: saved schematic file '"
		<< dschem.filename << "'." << std::endl;

	return 1;
}


// place_schematic(p, schematic, rotation, replacement)
int ModApiMapgen::l_place_schematic(lua_State *L)
{
	DecoSchematic dschem;

	Map *map = &(getEnv(L)->getMap());
	INodeDefManager *ndef = getServer(L)->getNodeDefManager();

	v3s16 p = read_v3s16(L, 1);
	if (!read_schematic(L, 2, &dschem, getServer(L)))
		return 0;

	int rot = ROTATE_0;
	if (lua_isstring(L, 3))
		string_to_enum(es_Rotation, rot, std::string(lua_tostring(L, 3)));

	dschem.rotation = (Rotation)rot;

	if (lua_istable(L, 4)) {
		lua_pushnil(L);
		while (lua_next(L, 4) != 0) {
			// key at index -2 and value at index -1
			lua_rawgeti(L, -1, 1);
			std::string replace_from = lua_tostring(L, -1);
			lua_pop(L, 1);
			lua_rawgeti(L, -1, 2);
			std::string replace_to = lua_tostring(L, -1);
			lua_pop(L, 1);
			dschem.replacements[replace_from] = replace_to;
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	}

	bool force_placement = true;
	if (lua_isboolean(L, 5))
		force_placement = lua_toboolean(L, 5);

	if (!dschem.filename.empty()) {
		if (!dschem.loadSchematicFile()) {
			errorstream << "place_schematic: failed to load schematic file '"
				<< dschem.filename << "'" << std::endl;
			return 0;
		}
		dschem.resolveNodeNames(ndef);
	}

	dschem.placeStructure(map, p, force_placement);

	return 1;
}

void ModApiMapgen::Initialize(lua_State *L, int top)
{
	API_FCT(get_mapgen_object);

	API_FCT(set_mapgen_params);
	API_FCT(set_noiseparam_defaults);
	API_FCT(set_gen_notify);

	API_FCT(register_biome);
	API_FCT(register_decoration);
	API_FCT(register_ore);

	API_FCT(create_schematic);
	API_FCT(place_schematic);
}
