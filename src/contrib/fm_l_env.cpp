/*
Copyright (C) 2026 proller <proler@gmail.com>
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
#include "util/unordered_map_hash.h"

namespace
{

struct TntBlastEvent {
	v3pos_t pos;
	content_t content = CONTENT_IGNORE;
	double intensity = 0.0;
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
		const NodeDefManager *ndef)
{
	lua_createtable(L, events.size(), 0);
	int index = 0;

	for (const auto &event : events) {
		lua_createtable(L, 0, 3);
		push_v3pos(L, event.pos);
		lua_setfield(L, -2, "pos");
		lua_pushstring(L, ndef->get(event.content).name.c_str());
		lua_setfield(L, -2, "name");
		lua_pushnumber(L, event.intensity);
		lua_setfield(L, -2, "intensity");
		lua_rawseti(L, -2, ++index);
	}
}

static double step_distance(const v3pos_t &dir)
{
	const auto dx = static_cast<double>(dir.X);
	const auto dy = static_cast<double>(dir.Y);
	const auto dz = static_cast<double>(dir.Z);
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static void add_layer_weight(unordered_map_v3pos<double> &weights,
		const v3pos_t &pos, double strength)
{
	const auto found = weights.find(pos);
	if (found != weights.end() && found->second >= strength)
		return;

	weights[pos] = strength;
}

static void add_to_layer_weights(unordered_map_v3pos<double> &weights,
		double strength)
{
	if (strength <= 0.0)
		return;

	for (auto &weight : weights)
		weight.second += strength;
}

static pos_t radial_parent_component(pos_t value, int shell)
{
	if (shell <= 1)
		return 0;

	const auto scaled = static_cast<double>(value) *
			static_cast<double>(shell - 1) / static_cast<double>(shell);
	return static_cast<pos_t>(std::round(scaled));
}

static v3pos_t radial_parent_rel(const v3pos_t &rel, int shell)
{
	return v3pos_t(
			radial_parent_component(rel.X, shell),
			radial_parent_component(rel.Y, shell),
			radial_parent_component(rel.Z, shell));
}

static int shell_distance(const v3pos_t &rel)
{
	int distance = std::abs(static_cast<int>(rel.X));
	distance = std::max(distance, std::abs(static_cast<int>(rel.Y)));
	distance = std::max(distance, std::abs(static_cast<int>(rel.Z)));
	return distance;
}

static v3pos_t radial_child_rel(const v3pos_t &rel, int shell)
{
	const int parent_shell = shell_distance(rel);
	if (parent_shell <= 0)
		return rel;

	const double scale = static_cast<double>(shell) /
			static_cast<double>(parent_shell);
	return v3pos_t(
			static_cast<pos_t>(std::round(static_cast<double>(rel.X) * scale)),
			static_cast<pos_t>(std::round(static_cast<double>(rel.Y) * scale)),
			static_cast<pos_t>(std::round(static_cast<double>(rel.Z) * scale)));
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
//   radius=num,
//   strength=num,
//   strength_left=num,
//   stopped=string
// }
int ModApiEnv::l_tnt_explode(lua_State *L)
{
	GET_ENV_PTR;

	v3pos_t origin = read_v3pos(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	auto *ndef = env->getGameDef()->ndef();

	const double radius = std::max(0.0,
			static_cast<double>(getfloatfield_default(L, 2, "radius", 4.0f)));
	const double time_max = std::max(0.0,
			static_cast<double>(getfloatfield_default(L, 2, "time_max", 5.0f)));
	const bool ignore_protection =
			getboolfield_default(L, 2, "ignore_protection", false);
	const bool ignore_on_blast =
			getboolfield_default(L, 2, "ignore_on_blast", false);
	const bool liquid_real = getboolfield_default(L, 2, "liquid_real", false);
	const std::string owner = getstringfield_default(L, 2, "owner", "");

	const int melt_chance = std::max(0,
			static_cast<int>(getintfield_default(L, 2, "melt_chance", 15)));
	const int melt_direction = getintfield_default(L, 2, "melt_direction", 1);
	const double melt_min_radius = static_cast<double>(
			getfloatfield_default(L, 2, "melt_min_radius", 10.0f));
	const double fast_radius = static_cast<double>(
			getfloatfield_default(L, 2, "fast_radius", 6.0f));

	const double default_blast_diameter = radius * 2.0 + 1.0;
	const double default_blast_strength = radius > 0.0 ?
			default_blast_diameter * default_blast_diameter *
					default_blast_diameter : 0.0;
	const double blast_strength = std::max(0.0,
			static_cast<double>(getfloatfield_default(
					L, 2, "blast_strength", default_blast_strength)));
	const double blast_tnt_strength = std::max(0.0,
			static_cast<double>(getfloatfield_default(
					L, 2, "blast_tnt_strength", blast_strength)));
	const double blast_distance_loss = std::max(0.01,
			static_cast<double>(getfloatfield_default(
					L, 2, "blast_distance_loss", 0.02f)));
	const double blast_resistance_scale = std::max(0.0,
			static_cast<double>(getfloatfield_default(
					L, 2, "blast_resistance_scale", 1.0f)));
	const double blast_default_resistance = std::max(0.0,
			static_cast<double>(getfloatfield_default(
					L, 2, "blast_default_resistance", 1.0f)));
	const double blast_min_strength = std::max(0.0,
			static_cast<double>(getfloatfield_default(
					L, 2, "blast_min_strength", 0.15f)));

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
	std::unordered_map<content_t, double> resistance_cache;
	std::vector<TntBlastEvent> on_blast_events;
	std::vector<v3pos_t> chained_tnt;
	unordered_set_v3pos terminal_tnt_ignited;

	const auto blast_strength_from_radius = [](int radius) {
		const double diameter = static_cast<double>(radius) * 2.0 + 1.0;
		return diameter * diameter * diameter;
	};

	const auto tnt_node_blast_strength = [&](content_t content) {
		const auto &cf = ndef->get(content);

		const int tnt_strength =
				itemgroup_get(cf.groups, "tnt_blast_tnt_strength");
		if (tnt_strength > 0)
			return static_cast<double>(tnt_strength);

		const int strength = itemgroup_get(cf.groups, "tnt_blast_strength");
		if (strength > 0)
			return static_cast<double>(strength);

		const int radius = itemgroup_get(cf.groups, "tnt_radius");
		if (radius > 0)
			return blast_strength_from_radius(radius);

		return blast_tnt_strength;
	};

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

	const auto read_lua_node_resistance = [&](const std::string &name,
			double fallback) {
		const int top = lua_gettop(L);
		double result = fallback;

		lua_getglobal(L, "core");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "registered_nodes");
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, name.c_str());
				if (lua_istable(L, -1)) {
					const char *fields[] = {
						"tnt_resistance",
						"blast_resistance",
						"_tnt_loss",
					};

					for (const char *field : fields) {
						lua_getfield(L, -1, field);
						if (lua_isnumber(L, -1)) {
							result = lua_tonumber(L, -1);
							lua_pop(L, 1);
							break;
						}
						lua_pop(L, 1);
					}
				}
			}
		}

		lua_settop(L, top);
		return std::max(0.0, result);
	};

	const auto group_resistance = [&](const ContentFeatures &cf) {
		const auto material_resistance = [](int group, double base) {
			if (group <= 0)
				return 0.0;

			return std::max(1.0,
					base * static_cast<double>(std::max(1, 4 - group)));
		};

		double resistance = 0.0;
		bool has_material_group = false;

		const int stone = itemgroup_get(cf.groups, "stone");
		if (stone > 0) {
			resistance = std::max(resistance, material_resistance(stone, 1.5));
			has_material_group = true;
		}

		const int cracky = itemgroup_get(cf.groups, "cracky");
		if (cracky > 0) {
			resistance = std::max(resistance, material_resistance(cracky, 1.4));
			has_material_group = true;
		}

		const int choppy = itemgroup_get(cf.groups, "choppy");
		if (choppy > 0) {
			resistance = std::max(resistance, material_resistance(choppy, 1.3));
			has_material_group = true;
		}

		const int crumbly = itemgroup_get(cf.groups, "crumbly");
		if (crumbly > 0) {
			resistance = std::max(resistance, material_resistance(crumbly, 1.2));
			has_material_group = true;
		}

		if (!has_material_group && cf.liquid_type != LIQUID_NONE) {
			const int liquid_group = itemgroup_get(cf.groups, "liquid");
			const int liquid = liquid_group > 0 ? liquid_group : 3;
			resistance = material_resistance(liquid, 2.0);
			has_material_group = true;
		}

		if (!has_material_group)
			resistance = blast_default_resistance;

		return resistance;
	};

	const auto node_resistance = [&](content_t content) {
		const auto found = resistance_cache.find(content);
		if (found != resistance_cache.end())
			return found->second;

		const auto &cf = ndef->get(content);
		double resistance = 0.0;
		if (!cf.name.empty() && content != CONTENT_AIR && content != CONTENT_IGNORE)
			resistance = read_lua_node_resistance(cf.name, group_resistance(cf)) *
					blast_resistance_scale;

		resistance_cache.emplace(content, resistance);
		return resistance;
	};

	const auto destroy_node = [&](const v3pos_t &pos, MapNode node,
			bool last_shell, bool fast, double intensity) {
		const content_t content = node.getContent();
		if (content == CONTENT_AIR || content == CONTENT_IGNORE)
			return false;

		const auto &cf = ndef->get(content);
		if (!ignore_protection && lua_is_node_protected(L, pos, owner))
			return false;

		if (!ignore_on_blast && has_on_blast(content)) {
			on_blast_events.push_back({pos, content, intensity});
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
	int ignited_tnts = 0;
	int destroyed = 0;
	int melted = 0;
	bool last = false;
	bool stopped_by_time = false;
	bool stopped_by_diffusion = false;
	bool stopped_by_blocked = false;
	bool stopped_by_frontier = false;
	size_t last_active_rays = 0;
	size_t last_blocked_rays = 0;
	size_t last_frontier_rays = 0;
	double last_ray_strength = 0.0;
	double total_strength = blast_strength;
	double remaining_strength = blast_strength;

	unordered_map_v3pos<double> layer_weights;
	unordered_map_v3pos<double> next_layer_weights;

	struct BlastCandidate {
		int dx = 0;
		int dy = 0;
		int dz = 0;
		v3pos_t rel;
		v3pos_t node_pos;
		MapNode node;
		double strength = 0.0;
		double step_cost = 0.0;
	};

	const auto charge_strength = [&](double cost) {
		if (cost <= 0.0)
			return true;
		if (remaining_strength <= 0.0)
			return false;
		if (remaining_strength + 0.000001 < cost) {
			remaining_strength = 0.0;
			return false;
		}

		remaining_strength -= cost;
		return true;
	};

	const auto can_charge_strength = [&](double cost) {
		return cost <= 0.0 || remaining_strength + 0.000001 >= cost;
	};

	const auto ignite_terminal_tnt = [&](const v3pos_t &pos, MapNode node) {
		const content_t content = node.getContent();
		if (!tnt_contents.count(content) || content == tnt_burning_content ||
				tnt_burning_content == CONTENT_IGNORE ||
				terminal_tnt_ignited.count(pos))
			return false;

		terminal_tnt_ignited.emplace(pos);
		env->setNode(pos, MapNode(tnt_burning_content), 2);
		++ignited_tnts;
		return true;
	};

	const auto ignite_terminal_tnt_at = [&](const v3pos_t &pos) {
		bool pos_ok = false;
		MapNode node = env->getMap().getNode(pos, &pos_ok);
		if (!pos_ok || node.getContent() == CONTENT_IGNORE)
			return false;

		return ignite_terminal_tnt(pos, node);
	};

	const auto add_shell_weight = [&](int shell, int dx, int dy, int dz,
			const unordered_map_v3pos<double> &previous_weights) {
		const v3pos_t rel(static_cast<pos_t>(dx), static_cast<pos_t>(dy),
				static_cast<pos_t>(dz));
		const v3pos_t parent_rel = radial_parent_rel(rel, shell);
		const v3pos_t parent_pos = origin + parent_rel;
		const auto parent = previous_weights.find(parent_pos);
		if (parent == previous_weights.end())
			return;

		const double strength = parent->second -
				blast_distance_loss * step_distance(rel - parent_rel);
		if (strength > blast_min_strength)
			add_layer_weight(layer_weights, origin + rel, strength);
		else
			ignite_terminal_tnt_at(origin + rel);
	};

	const auto build_layer_weights = [&](int shell,
			const unordered_map_v3pos<double> &previous_weights) {
		layer_weights.clear();

		for (int dx = -shell; dx <= shell; dx += shell * 2)
		for (int dy = -shell; dy <= shell; ++dy)
		for (int dz = -shell; dz <= shell; ++dz)
			add_shell_weight(shell, dx, dy, dz, previous_weights);

		for (int dy = -shell; dy <= shell; dy += shell * 2)
		for (int dx = -shell + 1; dx <= shell - 1; ++dx)
		for (int dz = -shell; dz <= shell; ++dz)
			add_shell_weight(shell, dx, dy, dz, previous_weights);

		for (int dz = -shell; dz <= shell; dz += shell * 2)
		for (int dx = -shell + 1; dx <= shell - 1; ++dx)
		for (int dy = -shell + 1; dy <= shell - 1; ++dy)
			add_shell_weight(shell, dx, dy, dz, previous_weights);

		for (const auto &previous : previous_weights) {
			const v3pos_t parent_rel = previous.first - origin;
			if (shell_distance(parent_rel) <= 0)
				continue;

			const v3pos_t rel = radial_child_rel(parent_rel, shell);
			const double strength = previous.second -
					blast_distance_loss * step_distance(rel - parent_rel);
			if (strength > blast_min_strength)
				add_layer_weight(layer_weights, origin + rel, strength);
			else
				ignite_terminal_tnt_at(origin + rel);
		}
	};

	if (blast_strength > blast_min_strength)
		next_layer_weights[origin] = blast_strength;

	std::vector<BlastCandidate> shell_candidates;

	const auto collect_pos = [&](int dx, int dy, int dz) {
		const v3pos_t rel(static_cast<pos_t>(dx), static_cast<pos_t>(dy),
				static_cast<pos_t>(dz));
		const v3pos_t node_pos = origin + rel;

		const auto weight_found = layer_weights.find(node_pos);
		if (weight_found == layer_weights.end())
			return;

		double strength = weight_found->second;
		if (strength <= blast_min_strength)
			return;

		bool pos_ok = false;
		MapNode node = env->getMap().getNode(node_pos, &pos_ok);
		if (!pos_ok) {
			++last_frontier_rays;
			return;
		}

		const content_t content = node.getContent();
		if (content == CONTENT_IGNORE) {
			++last_frontier_rays;
			return;
		}

		BlastCandidate candidate;
		candidate.dx = dx;
		candidate.dy = dy;
		candidate.dz = dz;
		candidate.rel = rel;
		candidate.node_pos = node_pos;
		candidate.node = node;
		candidate.strength = strength;
		candidate.step_cost = blast_distance_loss *
				step_distance(rel - radial_parent_rel(rel, dr));
		shell_candidates.push_back(candidate);
	};

	const auto process_tnt_candidates = [&](double ray_strength) {
		double added_strength = 0.0;

		for (const auto &candidate : shell_candidates) {
			const content_t content = candidate.node.getContent();
			if (!tnt_contents.count(content))
				continue;

			if (!last) {
				if (ray_strength <= blast_min_strength + candidate.step_cost) {
					ignite_terminal_tnt(candidate.node_pos, candidate.node);
					continue;
				}

				env->removeNode(candidate.node_pos, 2);
				const double node_strength = tnt_node_blast_strength(content);
				added_strength += node_strength;
				remaining_strength += node_strength;
				total_strength += node_strength;
				++tnts;
			} else {
				if (tnt_burning_content != CONTENT_IGNORE)
					env->setNode(candidate.node_pos,
							MapNode(tnt_burning_content), 2);
				chained_tnt.push_back(candidate.node_pos);
			}
		}

		if (added_strength <= 0.0)
			return;

		add_to_layer_weights(layer_weights, added_strength);
		for (auto &candidate : shell_candidates) {
			candidate.strength += added_strength;
			if (!last && tnt_contents.count(candidate.node.getContent()))
				add_layer_weight(next_layer_weights, candidate.node_pos,
						candidate.strength);
		}
	};

	const auto process_candidate = [&](const BlastCandidate &candidate,
			double ray_strength) {
		const content_t content = candidate.node.getContent();
		if (tnt_contents.count(content))
			return;

		const double available_strength = ray_strength;
		if (available_strength <= blast_min_strength)
			return;

		const auto &cf = ndef->get(content);
		const bool empty_node = content == CONTENT_AIR ||
				content == fire_content || content == boom_content;
		const bool blast_transparent = empty_node ||
				(!cf.walkable && cf.liquid_type == LIQUID_NONE);
		const bool destroyable = !empty_node;
		const bool blocks_wave = destroyable && !blast_transparent;
		const double resistance = blocks_wave ? node_resistance(content) : 0.0;
		const double pass_loss = blocks_wave ? resistance : candidate.step_cost;
		const double hit_strength = std::max(0.0, available_strength - pass_loss);
		const bool weak_edge = available_strength <=
				blast_distance_loss + resistance + 1.0;

		if (blocks_wave && hit_strength <= blast_min_strength) {
			++last_blocked_rays;
			return;
		}

		if (!blocks_wave && hit_strength <= blast_min_strength)
			return;

		if (!blocks_wave && !charge_strength(candidate.step_cost))
			return;

		if (blocks_wave && hit_strength > 0.0 && liquid_real && (last || weak_edge) &&
				dr > melt_min_radius && melt_chance > 0 &&
				myrand_range(1, melt_chance) <= 1) {
			if (!can_charge_strength(resistance))
				return;

			MapNode melted_node = candidate.node;
			const int changed = melted_node.freeze_melt(ndef, melt_direction);
			melted += changed;
			if (changed) {
				charge_strength(resistance);
				env->swapNode(candidate.node_pos, melted_node);
			}
		} else if (destroyable) {
			const bool fast = dr > fast_radius;
			if (hit_strength >= 1.0) {
				if (blocks_wave && !can_charge_strength(resistance))
					return;

				if (destroy_node(candidate.node_pos, candidate.node,
						last || weak_edge, fast, available_strength)) {
					if (blocks_wave)
						charge_strength(resistance);
					++destroyed;
				}
			}
		}

		if (!last && remaining_strength > blast_min_strength &&
				hit_strength > blast_min_strength)
			add_layer_weight(next_layer_weights, candidate.node_pos, hit_strength);
	};

	while (remaining_strength > blast_min_strength && !next_layer_weights.empty()) {
		++dr;
		const size_t previous_active_rays = next_layer_weights.size();
		build_layer_weights(dr, next_layer_weights);
		if (layer_weights.empty()) {
			if (remaining_strength > blast_min_strength) {
				stopped_by_diffusion = true;
				last_active_rays = previous_active_rays;
				last_ray_strength = previous_active_rays > 0 ?
						remaining_strength / static_cast<double>(
								previous_active_rays) : 0.0;
			}
			break;
		}

		next_layer_weights.clear();
		last = end_ms != 0 && porting::getTimeMs() > end_ms;
		if (last)
			stopped_by_time = true;
		shell_candidates.clear();
		last_blocked_rays = 0;
		last_frontier_rays = 0;

		for (int dx = -dr; dx <= dr; dx += dr * 2)
		for (int dy = -dr; dy <= dr; ++dy)
		for (int dz = -dr; dz <= dr; ++dz)
			collect_pos(dx, dy, dz);

		for (int dy = -dr; dy <= dr; dy += dr * 2)
		for (int dx = -dr + 1; dx <= dr - 1; ++dx)
		for (int dz = -dr; dz <= dr; ++dz)
			collect_pos(dx, dy, dz);

		for (int dz = -dr; dz <= dr; dz += dr * 2)
		for (int dx = -dr + 1; dx <= dr - 1; ++dx)
		for (int dy = -dr + 1; dy <= dr - 1; ++dy)
			collect_pos(dx, dy, dz);

		if (!shell_candidates.empty()) {
			const size_t active_rays = shell_candidates.size();
			last_active_rays = active_rays;
			last_ray_strength = active_rays > 0 ?
					remaining_strength / static_cast<double>(active_rays) : 0.0;

			process_tnt_candidates(last_ray_strength);

			const size_t start = static_cast<size_t>(myrand_range(0,
					static_cast<int>(shell_candidates.size() - 1)));
			for (size_t i = 0; i < shell_candidates.size(); ++i) {
				if (remaining_strength <= blast_min_strength)
					break;

				const auto &candidate = shell_candidates[
						(start + i) % shell_candidates.size()];
				if (tnt_contents.count(candidate.node.getContent()))
					continue;

				process_candidate(candidate, last_ray_strength);
			}
		}

		if (!last && next_layer_weights.empty() &&
				remaining_strength > blast_min_strength) {
			if (last_blocked_rays > 0)
				stopped_by_blocked = true;
			else if (last_frontier_rays > 0 && shell_candidates.empty())
				stopped_by_frontier = true;
			else
				stopped_by_diffusion = true;
		}

		if (last)
			break;
	}

	const bool stopped_by_strength = remaining_strength <= blast_min_strength ||
			(stopped_by_diffusion &&
					last_ray_strength <= blast_min_strength + blast_distance_loss);
	const char *stopped = stopped_by_time ? "time" :
			stopped_by_blocked ? "blocked" :
			stopped_by_frontier ? "frontier" :
			stopped_by_strength ? "strength" :
			stopped_by_diffusion ? "diffused" : "frontier";

	actionstream << tnts << " TNTs owned by " << owner << " detonated at "
			<< origin << " with radius=" << dr
			<< " strength=" << total_strength
			<< " strength_left=" << remaining_strength
			<< " active_rays=" << last_active_rays
			<< " blocked_rays=" << last_blocked_rays
			<< " frontier_rays=" << last_frontier_rays
			<< " ray_strength=" << last_ray_strength
			<< " stopped=" << stopped
			<< " ignited=" << ignited_tnts
			<< " destroyed=" << destroyed
			<< " melted=" << melted << std::endl;

	lua_createtable(L, 0, 7);

	lua_createtable(L, 0, drop_counts.size());
	for (const auto &drop : drop_counts) {
		lua_pushinteger(L, drop.second);
		lua_setfield(L, -2, drop.first.c_str());
	}
	lua_setfield(L, -2, "drops");

	push_blast_events(L, on_blast_events, ndef);
	lua_setfield(L, -2, "on_blast");

	push_pos_array(L, chained_tnt);
	lua_setfield(L, -2, "chained_tnt");

	set_lua_number_field(L, "radius", dr);
	set_lua_number_field(L, "strength", total_strength);
	set_lua_number_field(L, "strength_left", remaining_strength);
	lua_pushstring(L, stopped);
	lua_setfield(L, -2, "stopped");

	return 1;
}
