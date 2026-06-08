// Freeminer

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "script/lua_api/l_env.h"
#include "script/lua_api/l_internal.h"
#include "script/common/c_converter.h"
#include "environment.h"
#include "itemgroup.h"
#include "log.h"
#include "mapnode.h"
#include "nodedef.h"
#include "porting.h"
#include "script/scripting_server.h"
#include "server.h"
#include "serverenvironment.h"
#include "util/numeric.h"

namespace
{

struct TntBlastEvent {
	v3pos_t pos;
	content_t content = CONTENT_IGNORE;
};

static void set_lua_number_field(lua_State *L, const char *name, lua_Number value)
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, name);
}

static bool lua_node_has_callback(lua_State *L, const std::string &name,
		const char *callback)
{
	const int top = lua_gettop(L);
	bool result = false;

	lua_getglobal(L, "core");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "registered_nodes");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, name.c_str());
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, callback);
				result = lua_isfunction(L, -1);
			}
		}
	}

	lua_settop(L, top);
	return result;
}

static bool lua_is_node_protected(lua_State *L, v3pos_t pos,
		const std::string &owner)
{
	const int top = lua_gettop(L);

	lua_getglobal(L, "core");
	if (!lua_istable(L, -1)) {
		lua_settop(L, top);
		return false;
	}

	lua_getfield(L, -1, "is_protected");
	if (!lua_isfunction(L, -1)) {
		lua_settop(L, top);
		return false;
	}

	push_v3pos(L, pos);
	lua_pushlstring(L, owner.c_str(), owner.size());
	if (lua_pcall(L, 2, 1, 0) != 0) {
		warningstream << "core.tnt_explode: core.is_protected failed: "
				<< lua_tostring(L, -1) << std::endl;
		lua_settop(L, top);
		return true;
	}

	const bool result = lua_toboolean(L, -1);
	lua_settop(L, top);
	return result;
}

static void push_pos_array(lua_State *L, const std::vector<v3pos_t> &positions)
{
	lua_createtable(L, positions.size(), 0);
	int index = 0;
	for (const auto &pos : positions) {
		push_v3pos(L, pos);
		lua_rawseti(L, -2, ++index);
	}
}

static void push_blast_events(lua_State *L, const std::vector<TntBlastEvent> &events,
		const NodeDefManager *ndef, v3pos_t origin, double radius)
{
	lua_createtable(L, events.size(), 0);
	int index = 0;

	for (const auto &event : events) {
		const auto dx = static_cast<double>(event.pos.X - origin.X);
		const auto dy = static_cast<double>(event.pos.Y - origin.Y);
		const auto dz = static_cast<double>(event.pos.Z - origin.Z);
		const auto dist = std::max(1.0, std::sqrt(dx * dx + dy * dy + dz * dz));
		const auto intensity = (radius * radius) / (dist * dist);

		lua_createtable(L, 0, 3);
		push_v3pos(L, event.pos);
		lua_setfield(L, -2, "pos");
		lua_pushstring(L, ndef->get(event.content).name.c_str());
		lua_setfield(L, -2, "name");
		lua_pushnumber(L, intensity);
		lua_setfield(L, -2, "intensity");
		lua_rawseti(L, -2, ++index);
	}
}

} // namespace

void ModApiEnv::InitializeFM(lua_State *L, int top)
{
	registerFunction(L, "tnt_explode", l_tnt_explode, top);
}

// tnt_explode(pos, options) -> {
//   drops = {["node:name"] = count, ...},
//   on_blast = {{pos=pos, name="node:name", intensity=num}, ...},
//   chained_tnt = {pos, ...},
//   radius=num
// }
int ModApiEnv::l_tnt_explode(lua_State *L)
{
	GET_ENV_PTR;

	v3pos_t origin = read_v3pos(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	auto *ndef = env->getGameDef()->ndef();

	double radius = std::max(0.0,
			static_cast<double>(getfloatfield_default(L, 2, "radius", 2.0f)));
	const double radius_max = std::max(radius,
			static_cast<double>(getfloatfield_default(L, 2, "radius_max", 25.0f)));
	const double time_max = std::max(0.0,
			static_cast<double>(getfloatfield_default(L, 2, "time_max", 3.0f)));
	const bool ignore_protection =
			getboolfield_default(L, 2, "ignore_protection", false);
	const bool ignore_on_blast =
			getboolfield_default(L, 2, "ignore_on_blast", false);
	const bool liquid_real = getboolfield_default(L, 2, "liquid_real", false);
	const std::string owner = getstringfield_default(L, 2, "owner", "");

	const int edge_destroy_chance = std::max(0,
			std::min(100, getintfield_default(L, 2, "edge_destroy_chance", 80)));
	const int core_destroy_radius = std::max(0,
			getintfield_default(L, 2, "core_destroy_radius", 2));
	const int melt_chance = std::max(0, getintfield_default(L, 2, "melt_chance", 15));
	const int melt_direction = getintfield_default(L, 2, "melt_direction", 1);
	const double melt_min_radius = static_cast<double>(
			getfloatfield_default(L, 2, "melt_min_radius", 10.0f));
	const double fast_radius = static_cast<double>(
			getfloatfield_default(L, 2, "fast_radius", 6.0f));

	const double chain_radius_1 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_radius_1", 5.0f));
	const double chain_radius_2 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_radius_2", 10.0f));
	const double chain_radius_3 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_radius_3", 20.0f));
	const double chain_step_1 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_step_1", 1.0f));
	const double chain_step_2 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_step_2", 0.5f));
	const double chain_step_3 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_step_3", 0.3f));
	const double chain_step_4 = static_cast<double>(
			getfloatfield_default(L, 2, "chain_step_4", 0.2f));

	const auto read_content = [&](const char *field, const char *fallback) {
		const auto name = getstringfield_default(L, 2, field, fallback);
		content_t id = CONTENT_IGNORE;
		ndef->getId(name, id);
		return id;
	};

	content_t fire_content = read_content("fire_node", "fire:basic_flame");
	content_t boom_content = read_content("boom_node", "tnt:boom");
	content_t tnt_burning_content =
			read_content("tnt_burning_node", "tnt:tnt_burning");

	std::unordered_set<content_t> tnt_contents;
	const auto add_tnt_content = [&](const std::string &name) {
		content_t id = CONTENT_IGNORE;
		if (ndef->getId(name, id) && id != CONTENT_IGNORE)
			tnt_contents.emplace(id);
	};

	lua_getfield(L, 2, "tnt_nodes");
	if (lua_istable(L, -1)) {
		const int count = lua_objlen(L, -1);
		for (int i = 1; i <= count; ++i) {
			lua_rawgeti(L, -1, i);
			if (lua_isstring(L, -1))
				add_tnt_content(lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	} else {
		add_tnt_content(getstringfield_default(L, 2, "tnt_node", "tnt:tnt"));
		add_tnt_content(getstringfield_default(
				L, 2, "tnt_burning_node", "tnt:tnt_burning"));
	}
	lua_pop(L, 1);

	std::unordered_map<std::string, int> drop_counts;
	std::unordered_map<content_t, bool> on_blast_cache;
	std::vector<TntBlastEvent> on_blast_events;
	std::vector<v3pos_t> chained_tnt;

	const auto has_on_blast = [&](content_t content) {
		const auto found = on_blast_cache.find(content);
		if (found != on_blast_cache.end())
			return found->second;
		const auto &cf = ndef->get(content);
		const bool result = !cf.name.empty() &&
				lua_node_has_callback(L, cf.name, "on_blast");
		on_blast_cache.emplace(content, result);
		return result;
	};

	const auto destroy_node = [&](const v3pos_t &pos, MapNode node,
			bool last_shell, bool fast) {
		const content_t content = node.getContent();
		if (content == CONTENT_AIR || content == CONTENT_IGNORE)
			return false;

		const auto &cf = ndef->get(content);
		if (!ignore_protection && lua_is_node_protected(L, pos, owner))
			return false;

		if (!ignore_on_blast && has_on_blast(content)) {
			on_blast_events.push_back({pos, content});
			return true;
		}

		const s16 remove_fast = fast ? 1 : 0;
		const s16 set_fast = fast ? 2 : 0;

		if (itemgroup_get(cf.groups, "flammable") && fire_content != CONTENT_IGNORE) {
			env->removeNode(pos, remove_fast);
			if (last_shell)
				env->getScriptIface()->check_for_falling(pos);
			env->setNode(pos, MapNode(fire_content), set_fast);
			return true;
		}

		env->removeNode(pos, remove_fast);
		if (last_shell)
			env->getScriptIface()->check_for_falling(pos);
		if (!cf.name.empty())
			++drop_counts[cf.name];
		return true;
	};

	const u64 end_ms = time_max > 0.0 ?
			porting::getTimeMs() + static_cast<u64>(time_max * 1000.0) : 0;

	int dr = 0;
	int tnts = 1;
	int destroyed = 0;
	int melted = 0;
	bool last = false;

	const auto chain_step = [&]() {
		if (radius <= chain_radius_1)
			return chain_step_1;
		if (radius <= chain_radius_2)
			return chain_step_2;
		if (radius <= chain_radius_3)
			return chain_step_3;
		return chain_step_4;
	};

	const auto process_pos = [&](int dx, int dy, int dz) {
		const v3pos_t rel(static_cast<pos_t>(dx), static_cast<pos_t>(dy),
				static_cast<pos_t>(dz));
		const v3pos_t node_pos = origin + rel;

		bool pos_ok = false;
		MapNode node = env->getMap().getNode(node_pos, &pos_ok);
		if (!pos_ok)
			return;

		const content_t content = node.getContent();
		if (content == CONTENT_AIR || content == CONTENT_IGNORE)
			return;

		if (tnt_contents.count(content)) {
			if (radius < radius_max && !last && dr < radius) {
				radius = std::min(radius_max, radius + chain_step());
				env->removeNode(node_pos, 2);
				++tnts;
			} else {
				if (tnt_burning_content != CONTENT_IGNORE)
					env->setNode(node_pos, MapNode(tnt_burning_content), 2);
				chained_tnt.push_back(node_pos);
			}
			return;
		}

		if (content == fire_content || content == boom_content)
			return;

		if (liquid_real && last && radius > melt_min_radius && melt_chance > 0 &&
				myrand_range(1, melt_chance) <= 1) {
			MapNode melted_node = node;
			const int changed = melted_node.freeze_melt(ndef, melt_direction);
			melted += changed;
			if (changed)
				env->swapNode(node_pos, melted_node);
			return;
		}

		const bool fast = radius > fast_radius;
		const bool core_hit = std::abs(dx) < core_destroy_radius &&
				std::abs(dy) < core_destroy_radius &&
				std::abs(dz) < core_destroy_radius;
		const bool shell_hit = dr < radius || edge_destroy_chance >= 100 ||
				(edge_destroy_chance > 0 &&
						myrand_range(1, 100) <= edge_destroy_chance);

		if (core_hit || shell_hit) {
			destroy_node(node_pos, node, last, fast);
			++destroyed;
		}
	};

	while (dr < radius) {
		++dr;
		last = (end_ms != 0 && porting::getTimeMs() > end_ms) || dr >= radius;

		for (int dx = -dr; dx <= dr; dx += dr * 2)
		for (int dy = -dr; dy <= dr; ++dy)
		for (int dz = -dr; dz <= dr; ++dz)
			process_pos(dx, dy, dz);

		for (int dy = -dr; dy <= dr; dy += dr * 2)
		for (int dx = -dr + 1; dx <= dr - 1; ++dx)
		for (int dz = -dr; dz <= dr; ++dz)
			process_pos(dx, dy, dz);

		for (int dz = -dr; dz <= dr; dz += dr * 2)
		for (int dx = -dr + 1; dx <= dr - 1; ++dx)
		for (int dy = -dr + 1; dy <= dr - 1; ++dy)
			process_pos(dx, dy, dz);

		if (last)
			break;
	}

	actionstream << tnts << " TNTs owned by " << owner << " detonated at "
			<< origin << " with radius=" << dr
			<< " radius_want=" << radius
			<< " destroyed=" << destroyed
			<< " melted=" << melted << std::endl;

	lua_createtable(L, 0, 4);

	lua_createtable(L, 0, drop_counts.size());
	for (const auto &drop : drop_counts) {
		lua_pushinteger(L, drop.second);
		lua_setfield(L, -2, drop.first.c_str());
	}
	lua_setfield(L, -2, "drops");

	push_blast_events(L, on_blast_events, ndef, origin, radius);
	lua_setfield(L, -2, "on_blast");

	push_pos_array(L, chained_tnt);
	lua_setfield(L, -2, "chained_tnt");

	set_lua_number_field(L, "radius", radius);

	return 1;
}
