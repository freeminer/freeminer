/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "irr_v3d.h"
#include "lua_api/l_base.h"
#include "serverenvironment.h"
#include "raycast.h"

// base class containing helpers
class ModApiEnvBase : public ModApiBase {
protected:

	static void collectNodeIds(lua_State *L, int idx,
		const NodeDefManager *ndef, std::vector<content_t> &filter);

	static void checkArea(v3pos_t &minp, v3pos_t &maxp);

	// F must be (v3pos_t pos) -> MapNode
	template <typename F>
	static int findNodeNear(lua_State *L, v3pos_t pos, int radius,
		const std::vector<content_t> &filter, int start_radius, F &&getNode);

	// F must be (G callback) -> void
	// with G being (v3pos_t p, MapNode n) -> bool
	// and behave like Map::forEachNodeInArea
	template <typename F>
	static int findNodesInArea(lua_State *L,  const NodeDefManager *ndef,
		const std::vector<content_t> &filter, bool grouped, F &&iterate);

	// F must be (v3pos_t pos) -> MapNode
	template <typename F>
	static int findNodesInAreaUnderAir(lua_State *L, v3pos_t minp, v3pos_t maxp,
		const std::vector<content_t> &filter, F &&getNode);

	static const EnumString es_ClearObjectsMode[];
	static const EnumString es_BlockStatusType[];

};

class ModApiEnv : public ModApiEnvBase {
private:
	// set_node(pos, node)
	// pos = {x=num, y=num, z=num}
	static int l_set_node(lua_State *L);

	// bulk_set_node([pos1, pos2, ...], node)
	// pos = {x=num, y=num, z=num}
	static int l_bulk_set_node(lua_State *L);

	static int l_add_node(lua_State *L);

	// remove_node(pos)
	// pos = {x=num, y=num, z=num}
	static int l_remove_node(lua_State *L);

	// swap_node(pos, node)
	// pos = {x=num, y=num, z=num}
	static int l_swap_node(lua_State *L);

	// get_node_raw(x, y, z) -> content, param1, param2, pos_ok
	// Used to implement get_node and get_node_or_nil in lua.
	// This is still faster than doing it from C++ even with optimized pushnode.
	static int l_get_node_raw(lua_State *L);

	// get_node_light(pos, timeofday)
	// pos = {x=num, y=num, z=num}
	// timeofday: nil = current time, 0 = night, 0.5 = day
	static int l_get_node_light(lua_State *L);

	// get_natural_light(pos, timeofday)
	// pos = {x=num, y=num, z=num}
	// timeofday: nil = current time, 0 = night, 0.5 = day
	static int l_get_natural_light(lua_State *L);

	// place_node(pos, node, [placer])
	// pos = {x=num, y=num, z=num}
	static int l_place_node(lua_State *L);

	// dig_node(pos, [digger])
	// pos = {x=num, y=num, z=num}
	static int l_dig_node(lua_State *L);

	// punch_node(pos, [puncher])
	// pos = {x=num, y=num, z=num}
	static int l_punch_node(lua_State *L);

	// get_node_max_level(pos)
	// pos = {x=num, y=num, z=num}
	static int l_get_node_max_level(lua_State *L);

	// get_node_level(pos)
	// pos = {x=num, y=num, z=num}
	static int l_get_node_level(lua_State *L);

	// set_node_level(pos)
	// pos = {x=num, y=num, z=num}
	static int l_set_node_level(lua_State *L);

	// add_node_level(pos)
	// pos = {x=num, y=num, z=num}
	static int l_add_node_level(lua_State *L);

	// get_node_boxes(box_type, pos, [node]) -> table
	// box_type = string
	// pos = {x=num, y=num, z=num}
	// node = {name=string, param1=num, param2=num} or nil
	static int l_get_node_boxes(lua_State *L);

	// find_nodes_with_meta(pos1, pos2)
	static int l_find_nodes_with_meta(lua_State *L);

	// get_meta(pos)
	static int l_get_meta(lua_State *L);

	// get_node_timer(pos)
	static int l_get_node_timer(lua_State *L);

	// add_entity(pos, entityname) -> ObjectRef or nil
	// pos = {x=num, y=num, z=num}
	static int l_add_entity(lua_State *L);

	// add_item(pos, itemstack or itemstring or table) -> ObjectRef or nil
	// pos = {x=num, y=num, z=num}
	static int l_add_item(lua_State *L);

	// get_connected_players()
	static int l_get_connected_players(lua_State *L);

	// get_player_by_name(name)
	static int l_get_player_by_name(lua_State *L);

	// get_objects_inside_radius(pos, radius)
	static int l_get_objects_inside_radius(lua_State *L);

	// get_objects_in_area(pos, minp, maxp)
	static int l_get_objects_in_area(lua_State *L);

	// set_timeofday(val)
	// val = 0...1
	static int l_set_timeofday(lua_State *L);

	// get_timeofday() -> 0...1
	static int l_get_timeofday(lua_State *L);

	// get_gametime()
	static int l_get_gametime(lua_State *L);

	// get_day_count() -> int
	static int l_get_day_count(lua_State *L);

	// find_node_near(pos, radius, nodenames, search_center) -> pos or nil
	// nodenames: eg. {"ignore", "group:tree"} or "default:dirt"
	static int l_find_node_near(lua_State *L);

	// find_nodes_in_area(minp, maxp, nodenames) -> list of positions
	// nodenames: eg. {"ignore", "group:tree"} or "default:dirt"
	static int l_find_nodes_in_area(lua_State *L);

	// find_surface_nodes_in_area(minp, maxp, nodenames) -> list of positions
	// nodenames: eg. {"ignore", "group:tree"} or "default:dirt"
	static int l_find_nodes_in_area_under_air(lua_State *L);

	// fix_light(p1, p2) -> true/false
	static int l_fix_light(lua_State *L);

	// load_area(p1)
	static int l_load_area(lua_State *L);

	// emerge_area(p1, p2)
	static int l_emerge_area(lua_State *L);

	// delete_area(p1, p2) -> true/false
	static int l_delete_area(lua_State *L);

	// get_perlin(seeddiff, octaves, persistence, scale)
	// returns world-specific PerlinNoise
	static int l_get_perlin(lua_State *L);

	// get_perlin_map(noiseparams, size)
	// returns world-specific PerlinNoiseMap
	static int l_get_perlin_map(lua_State *L);

	// get_voxel_manip()
	// returns world-specific voxel manipulator
	static int l_get_voxel_manip(lua_State *L);

	// clear_objects()
	// clear all objects in the environment
	static int l_clear_objects(lua_State *L);

	// spawn_tree(pos, treedef)
	static int l_spawn_tree(lua_State *L);

	// line_of_sight(pos1, pos2) -> true/false
	static int l_line_of_sight(lua_State *L);

	// raycast(pos1, pos2, objects, liquids) -> Raycast
	static int l_raycast(lua_State *L);

	// find_path(pos1, pos2, searchdistance,
	//     max_jump, max_drop, algorithm) -> table containing path
	static int l_find_path(lua_State *L);

	// transforming_liquid_add(pos)
	static int l_transforming_liquid_add(lua_State *L);

	// forceload_block(blockpos)
	// forceloads a block
	static int l_forceload_block(lua_State *L);

	// forceload_free_block(blockpos)
	// stops forceloading a position
	static int l_forceload_free_block(lua_State *L);

	// compare_block_status(nodepos)
	static int l_compare_block_status(lua_State *L);

	// get_translated_string(lang_code, string)
	static int l_get_translated_string(lua_State * L);

public:
	static void Initialize(lua_State *L, int top);
	static void InitializeClient(lua_State *L, int top);
};

/*
 * Duplicates of certain env APIs that operate not on the global
 * map but on a VoxelManipulator. This is for emerge scripting.
 */
class ModApiEnvVM : public ModApiEnvBase {
private:

	// get_node_or_nil(pos)
	static int l_get_node_or_nil(lua_State *L);

	// get_node_max_level(pos)
	static int l_get_node_max_level(lua_State *L);

	// get_node_level(pos)
	static int l_get_node_level(lua_State *L);

	// set_node_level(pos)
	static int l_set_node_level(lua_State *L);

	// add_node_level(pos)
	static int l_add_node_level(lua_State *L);

	// find_node_near(pos, radius, nodenames, [search_center])
	static int l_find_node_near(lua_State *L);

	// find_nodes_in_area(minp, maxp, nodenames, [grouped])
	static int l_find_nodes_in_area(lua_State *L);

	// find_surface_nodes_in_area(minp, maxp, nodenames)
	static int l_find_nodes_in_area_under_air(lua_State *L);

	// spawn_tree(pos, treedef)
	static int l_spawn_tree(lua_State *L);

	// Helper: get the vmanip we're operating on
	static MMVManip *getVManip(lua_State *L);

public:
	static void InitializeEmerge(lua_State *L, int top);
};

class LuaABM : public ActiveBlockModifier {
private:
	int m_id;

	std::vector<std::string> m_trigger_contents;
	std::vector<std::string> m_required_neighbors;
	float m_trigger_interval;
	u32 m_trigger_chance;
	bool m_simple_catch_up;
	pos_t m_min_y;
	pos_t m_max_y;
public:
	LuaABM(lua_State *L, int id,
			const std::vector<std::string> &trigger_contents,
			const std::vector<std::string> &required_neighbors,
			float trigger_interval, u32 trigger_chance, bool simple_catch_up, pos_t min_y, pos_t max_y):
		m_id(id),
		m_trigger_contents(trigger_contents),
		m_required_neighbors(required_neighbors),
		m_trigger_interval(trigger_interval),
		m_trigger_chance(trigger_chance),
		m_simple_catch_up(simple_catch_up),
		m_min_y(min_y),
		m_max_y(max_y)
	{
	}
	virtual const std::vector<std::string> &getTriggerContents() const
	{
		return m_trigger_contents;
	}
	virtual const std::vector<std::string> &getRequiredNeighbors() const
	{
		return m_required_neighbors;
	}
	virtual float getTriggerInterval()
	{
		return m_trigger_interval;
	}
	virtual u32 getTriggerChance()
	{
		return m_trigger_chance;
	}
	virtual bool getSimpleCatchUp()
	{
		return m_simple_catch_up;
	}
	virtual pos_t getMinY()
	{
		return m_min_y;
	}
	virtual pos_t getMaxY()
	{
		return m_max_y;
	}
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider);
};

class LuaLBM : public LoadingBlockModifierDef
{
private:
	int m_id;
public:
	LuaLBM(lua_State *L, int id,
			const std::set<std::string> &trigger_contents,
			const std::string &name,
			bool run_at_every_load):
		m_id(id)
	{
		this->run_at_every_load = run_at_every_load;
		this->trigger_contents = trigger_contents;
		this->name = name;
	}
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n, float dtime_s);
};

//! Lua wrapper for RaycastState objects
class LuaRaycast : public ModApiBase
{
private:
	static const luaL_Reg methods[];
	//! Inner state
	RaycastState state;

	// Exported functions

	// garbage collector
	static int gc_object(lua_State *L);

	/*!
	 * Raycast:next() -> pointed_thing
	 * Returns the next pointed thing on the ray.
	 */
	static int l_next(lua_State *L);
public:
	//! Constructor with the same arguments as RaycastState.
	LuaRaycast(
		const core::line3d<f32> &shootline,
		bool objects_pointable,
		bool liquids_pointable,
		const std::optional<Pointabilities> &pointabilities) :
		state(shootline, objects_pointable, liquids_pointable, pointabilities)
	{}

	//! Creates a LuaRaycast and leaves it on top of the stack.
	static int create_object(lua_State *L);

	//! Registers Raycast as a Lua userdata type.
	static void Register(lua_State *L);

	static const char className[];
};

struct ScriptCallbackState {
	ServerScripting *script;
	int callback_ref;
	int args_ref;
	unsigned int refcount;
	std::string origin;
};
