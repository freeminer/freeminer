#include <limits>
#include "common/c_converter.h"
#include "emerge.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_mapgen.h"
#include "mapgen/mapgen_earth.h"
#include "mapgen/mg_biome.h"
#include "mapgen/mg_decoration.h"
#include "mapgen/mg_ore.h"
#include "objdef.h"
#include "server.h"
#include "servermap.h"

ObjDef *get_objdef(lua_State *L, int index, const ObjDefManager *objmgr);

// Update the vertical bounds of an existing mapgen definition without
// changing its handle or its position in the registration order.
int ModApiMapgen::l_set_mapgen_y_bounds(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const std::string type = luaL_checkstring(L, 1);
	auto *emerge = getServer(L)->getEmergeManager();
	if (!emerge->m_mapgens.empty())
		return luaL_error(L, "mapgen bounds can only be changed during mod loading");
	ObjDefManager *manager;
	if (type == "biome")
		manager = emerge->getWritableBiomeManager();
	else if (type == "ore")
		manager = emerge->getWritableOreManager();
	else if (type == "decoration")
		manager = emerge->getWritableDecorationManager();
	else
		return luaL_error(L, "unknown mapgen definition type");

	ObjDef *object = get_objdef(L, 2, manager);
	if (!object)
		return luaL_error(L, "mapgen definition not found");

	const auto set_bounds = [&](auto &min_y, auto &max_y) {
		if (!lua_isnil(L, 3)) {
			pos_t value = luaL_checkinteger(L, 3);
			if (value < std::numeric_limits<s16>::min() ||
					value > std::numeric_limits<s16>::max())
				luaL_error(L, "mapgen y_min is out of range");
			min_y = value;
		}
		if (!lua_isnil(L, 4)) {
			pos_t value = luaL_checkinteger(L, 4);
			if (value < std::numeric_limits<s16>::min() ||
					value > std::numeric_limits<s16>::max())
				luaL_error(L, "mapgen y_max is out of range");
			max_y = value;
		}
	};

	if (type == "biome") {
		auto *biome = static_cast<Biome *>(object);
		set_bounds(biome->min_pos.Y, biome->max_pos.Y);
	} else if (type == "ore") {
		auto *ore = static_cast<Ore *>(object);
		set_bounds(ore->y_min, ore->y_max);
	} else {
		auto *decoration = static_cast<Decoration *>(object);
		set_bounds(decoration->y_min, decoration->y_max);
	}
	return 0;
}

// get_wind(pos)
// returns the wind at the position
int ModApiMapgen::l_get_wind(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const auto pos = read_v3pos(L, 1);
	const auto block_add = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : true;
	GET_ENV_PTR_NO_MAP_LOCK;
	push_v3f(L,
			env->getServerMap().updateBlockWind(env, pos, nullptr, nullptr, block_add));
	return 1;
}

// fm:
// get_ground_level(x = num, z = num)
int ModApiMapgen::l_get_ground_level(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	pos_t x = luaL_checkinteger(L, 1);
	pos_t z = luaL_checkinteger(L, 2);

	EmergeManager *emerge = getServer(L)->getEmergeManager();
	lua_pushinteger(L, emerge->getGroundLevelAtPoint(v2pos_t(x, z)));
	return 1;
}

int ModApiMapgen::l_earth_pos_to_ll(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	auto *mg = getMapgen(L);
	if (!mg) {
		auto *emerge = getServer(L)->getEmergeManager();
		if (!emerge->m_mapgens.empty())
			mg = emerge->m_mapgens.front();
	}
	auto *earth = dynamic_cast<MapgenEarth *>(mg);
	if (!earth)
		return luaL_error(L, "earth projection API requires the earth mapgen");
	const auto sample = earth->sampleEarth(check_v3pos(L, 1));
	lua_newtable(L);
	lua_pushnumber(L, sample.lat);
	lua_setfield(L, -2, "lat");
	lua_pushnumber(L, sample.lon);
	lua_setfield(L, -2, "lon");
	lua_pushnumber(L, sample.altitude);
	lua_setfield(L, -2, "altitude");
	return 1;
}

int ModApiMapgen::l_earth_ll_to_pos(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	auto *mg = getMapgen(L);
	if (!mg) {
		auto *emerge = getServer(L)->getEmergeManager();
		if (!emerge->m_mapgens.empty())
			mg = emerge->m_mapgens.front();
	}
	auto *earth = dynamic_cast<MapgenEarth *>(mg);
	if (!earth)
		return luaL_error(L, "earth projection API requires the earth mapgen");
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "lat");
	const double lat = luaL_checknumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 1, "lon");
	const double lon = luaL_checknumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, 1, "altitude");
	const bool automatic_altitude = lua_isnil(L, -1) && lua_toboolean(L, 2);
	double altitude = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
	lua_pop(L, 1);
	if (automatic_altitude)
		altitude = std::max(earth->projectedElevation({lat, lon, 0}, 0),
						   double(earth->water_level)) +
				   2.0;
	const auto pos = earth->ll_to_pos3({lat, lon}, altitude);
	lua_newtable(L);
	lua_pushnumber(L, pos.X);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, pos.Y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, pos.Z);
	lua_setfield(L, -2, "z");
	lua_pushboolean(L, earth->projection.curved);
	lua_setfield(L, -2, "curved");
	return 1;
}

// ===
