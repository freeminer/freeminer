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
#include <limits>
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
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "tool.h"
#include "fm_blast_shell.h"
#include "fm_blast_angular.h"
#include "voxelalgorithms.h"
#include "serverenvironment.h"
#include "util/numeric.h"
#include "util/unordered_map_hash.h"

namespace
{

struct TntBlastEvent
{
	v3pos_t pos;
	content_t content = CONTENT_IGNORE;
	double intensity = 0.0;
};

static void set_lua_number_field(lua_State *L, const char *name, lua_Number value)
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, name);
}

static bool lua_node_has_callback(
		lua_State *L, const std::string &name, const char *callback)
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

static bool lua_is_node_protected(lua_State *L, v3pos_t pos, const std::string &owner)
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

// Opt-in object effects. Distances and velocities in the Lua API use node units.
static double blast_multiplier(lua_State *L, const char *name, double fallback)
{
	const double value = getfloatfield_default(L, 2, name, fallback);
	return std::isfinite(value) ? std::max(0.0, value) : 0.0;
}

// Check the surviving map, ignoring the source TNT/boom node itself.
// Solid nodeboxes are conservatively treated as whole-node shields.
static bool blast_object_visible(
		ServerEnvironment *env, const v3opos_t &center, const ServerActiveObject &object)
{
	v3opos_t target = object.getBasePosition();
	aabb3o box;
	if (object.getCollisionBox(&box))
		target = box.getCenter();
	const auto *ndef = env->getGameDef()->ndef();
	voxalgo::VoxelLineIterator ray(center / BS, oposToV3f((target - center) / BS));
	while (ray.hasNext()) {
		ray.next();
		const v3pos_t pos = ray.m_current_node_pos;
		bool valid = false;
		const MapNode node = env->getMap().getNode(pos, &valid);
		if (!valid || node.getContent() == CONTENT_IGNORE || ndef->get(node).walkable)
			return false;
	}
	return true;
}

template <typename PushObject>
static void blast_objects(lua_State *L, ServerEnvironment *env, const v3pos_t &origin,
		int radius, const PushObject &push_object)
{
	const double damage_multiplier = blast_multiplier(L, "damage_multiplier", 0.0);
	const double knockback_multiplier = blast_multiplier(L, "knockback_multiplier", 0.0);
	const double effect_radius =
			radius * blast_multiplier(L, "object_radius_multiplier", 3.0);
	if (effect_radius <= 0.0 || (damage_multiplier <= 0.0 && knockback_multiplier <= 0.0))
		return;

	const bool wall_shield = getboolfield_default(L, 2, "object_wall_shield", true);
	const double player_damage = blast_multiplier(L, "player_damage_multiplier", 1.0);
	const double player_knockback =
			blast_multiplier(L, "player_knockback_multiplier", 1.0);
	const v3opos_t center = intToFloat(origin, BS);
	std::vector<ServerActiveObjectPtr> objects;
	env->getObjectsInsideRadius(objects, center,
			std::min(effect_radius * BS,
					static_cast<double>(std::numeric_limits<float>::max())),
			[](const ServerActiveObjectPtr &obj) { return !obj->isGone(); });

	for (const auto &object : objects) {
		if (object->isGone())
			continue;
		auto *player = dynamic_cast<PlayerSAO *>(object.get());
		auto *entity = dynamic_cast<LuaEntitySAO *>(object.get());
		if (!player && !entity)
			continue;

		if (wall_shield && !blast_object_visible(env, center, *object))
			continue;

		v3opos_t direction = object->getBasePosition() - center;
		const double distance =
				std::max(1.0, static_cast<double>(direction.getLength()) / BS);
		direction.normalize();
		const double damage = std::min(player ? 65535.0 : 32767.0,
				20.0 * effect_radius / distance * damage_multiplier *
						(player ? player_damage : 1.0));
		bool do_damage = damage > 0.0;
		bool do_knockback = knockback_multiplier > 0.0;

		// The caller may preserve game-specific on_blast handling and collect drops.
		lua_getfield(L, 2, "on_blast_object");
		if (lua_isfunction(L, -1)) {
			push_object(object.get());
			lua_pushnumber(L, damage);
			if (lua_pcall(L, 2, 2, 0) != 0) {
				warningstream << "core.tnt_explode: on_blast_object failed: "
							  << lua_tostring(L, -1) << std::endl;
				lua_pop(L, 1);
				continue;
			}
			do_damage = do_damage && lua_toboolean(L, -2);
			do_knockback = do_knockback && lua_toboolean(L, -1);
			lua_pop(L, 2);
		} else {
			lua_pop(L, 1);
		}
		if (object->isGone())
			continue;

		if (do_knockback) {
			const double speed = std::min(
					250.0, 10.0 * effect_radius / distance * knockback_multiplier *
								   (player ? player_knockback : 1.0));
			const v3opos_t impulse = direction * static_cast<opos_t>(speed * BS);
			if (player && speed > 0.0) {
				player->setMaxSpeedOverride(oposToV3f(impulse));
				env->getServer()->SendPlayerSpeed(
						player->getPeerID(), oposToV3f(impulse));
			} else if (entity && speed > 0.0) {
				v3opos_t velocity = v3fToOpos(entity->getVelocity()) + impulse;
				const auto length = velocity.getLength();
				if (length > 250.0 * BS)
					velocity *= (250.0 * BS) / length;
				entity->setVelocity(oposToV3f(velocity));
			}
		}
		if (do_damage) {
			if (player) {
				PlayerHPChangeReason reason(PlayerHPChangeReason::SET_HP);
				reason.from_mod = true;
				player->setHP(
						static_cast<s32>(player->getHP()) - static_cast<s32>(damage),
						reason);
			} else {
				ToolCapabilities tool;
				tool.full_punch_interval = 1.0;
				tool.damageGroups["fleshy"] = static_cast<int>(damage);
				entity->punch(oposToV3f(direction), tool, entity, 1.0);
			}
		}
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
//   radius=num, // furthest shell visited
//   effect_radius=num, // bulk effects, bounded by total explosive power
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

	const double radius = std::max(
			0.0, static_cast<double>(getfloatfield_default(L, 2, "radius", 4.0f)));
	const double time_max = std::max(
			0.0, static_cast<double>(getfloatfield_default(L, 2, "time_max", 5.0f)));
	const bool ignore_protection = getboolfield_default(L, 2, "ignore_protection", false);
	const bool ignore_on_blast = getboolfield_default(L, 2, "ignore_on_blast", false);
	const bool liquid_real = getboolfield_default(L, 2, "liquid_real", false);
	const std::string owner = getstringfield_default(L, 2, "owner", "");

	const int melt_chance =
			std::max(0, static_cast<int>(getintfield_default(L, 2, "melt_chance", 15)));
	const int melt_direction = getintfield_default(L, 2, "melt_direction", 1);
	const double melt_min_radius =
			static_cast<double>(getfloatfield_default(L, 2, "melt_min_radius", 10.0f));
	const double fast_radius =
			static_cast<double>(getfloatfield_default(L, 2, "fast_radius", 6.0f));

	const double default_blast_diameter = radius * 2.0 + 1.0;
	const double default_blast_strength = radius > 0.0 ? default_blast_diameter *
																 default_blast_diameter *
																 default_blast_diameter
													   : 0.0;
	const double blast_strength =
			std::max(0.0, static_cast<double>(getfloatfield_default(
								  L, 2, "blast_strength", default_blast_strength)));
	const double blast_tnt_strength = std::max(0.0,
			static_cast<double>(
					getfloatfield_default(L, 2, "blast_tnt_strength", blast_strength)));
	const double blast_distance_loss = std::max(
			0.01, static_cast<double>(
						  getfloatfield_default(L, 2, "blast_distance_loss", 0.1f)));
	const double blast_resistance_scale = std::max(
			0.0, static_cast<double>(
						 getfloatfield_default(L, 2, "blast_resistance_scale", 1.0f)));
	const double blast_default_resistance = std::max(
			0.0, static_cast<double>(
						 getfloatfield_default(L, 2, "blast_default_resistance", 1.0f)));
	const double blast_min_strength = std::max(
			0.0, static_cast<double>(
						 getfloatfield_default(L, 2, "blast_min_strength", 0.15f)));

	const double blast_tnt_absorb_strength =
			blast_multiplier(L, "blast_tnt_absorb_strength", 1.0);

	const double blast_tnt_ray_fraction = fm_blast_fraction(
			getfloatfield_default(L, 2, "blast_tnt_ray_fraction", 0.8), 0.8);
	const double blast_core_radius =
			fm_blast_core_radius(getfloatfield_default(L, 2, "blast_core_radius",
										 fm_blast_full_shell_radius(blast_strength)),
					blast_strength);
	const double blast_core_shell_fraction = fm_blast_fraction(
			getfloatfield_default(L, 2, "blast_core_shell_fraction", 0.0), 0.0);

	const auto read_content = [&](const char *field, const char *fallback) {
		const auto name = getstringfield_default(L, 2, field, fallback);
		content_t id = CONTENT_IGNORE;
		ndef->getId(name, id);
		return id;
	};

	content_t fire_content = read_content("fire_node", "fire:basic_flame");
	content_t boom_content = read_content("boom_node", "tnt:boom");
	content_t tnt_burning_content = read_content("tnt_burning_node", "tnt:tnt_burning");

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
		add_tnt_content(
				getstringfield_default(L, 2, "tnt_burning_node", "tnt:tnt_burning"));
	}
	lua_pop(L, 1);

	std::unordered_map<std::string, uint64_t> drop_counts;
	std::unordered_map<content_t, bool> on_blast_cache;
	std::unordered_map<content_t, double> resistance_cache;
	std::vector<TntBlastEvent> on_blast_events;
	std::vector<v3pos_t> chained_tnt;
	FmBlastSet terminal_tnt_ignited;

	const auto blast_strength_from_radius = [](int radius) {
		const double diameter = static_cast<double>(radius) * 2.0 + 1.0;
		return diameter * diameter * diameter;
	};

	const auto tnt_node_blast_strength = [&](content_t content) {
		const auto &cf = ndef->get(content);

		const int tnt_strength = itemgroup_get(cf.groups, "tnt_blast_tnt_strength");
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
		const bool result =
				!cf.name.empty() && lua_node_has_callback(L, cf.name, "on_blast");
		on_blast_cache.emplace(content, result);
		return result;
	};

	const auto read_lua_node_resistance = [&](const std::string &name, double fallback) {
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

			return std::max(1.0, base * static_cast<double>(std::max(1, 4 - group)));
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

	const auto remove_node = [&](const v3pos_t &pos, pos_t fast) {
		if (!env->removeNode(pos, fast))
			return false;
		if (fast) {
			// Fast removeNode currently swallows map-write failures.
			if (env->getMap().getNode(pos).getContent() != CONTENT_AIR)
				return false;
			env->getMap().removeNodeMetadata(pos);
			env->getMap().removeNodeTimer(pos);
		}
		return true;
	};
	const auto destroy_node = [&](const v3pos_t &pos, MapNode node, bool last_shell,
									  bool fast, double intensity) {
		const auto &cf = ndef->get(node);
		if (!ignore_on_blast && has_on_blast(node.getContent())) {
			on_blast_events.push_back({pos, node.getContent(), intensity});
			return FmBlastOutcome::CallbackPending;
		}
		if (!remove_node(pos, fast ? 1 : 0))
			return FmBlastOutcome::Blocked;
		if (last_shell)
			env->getScriptIface()->check_for_falling(pos);
		if (itemgroup_get(cf.groups, "flammable") && fire_content != CONTENT_IGNORE) {
			env->setNode(pos, MapNode(fire_content), fast ? 2 : 0);
			return FmBlastOutcome::Transformed;
		}
		if (!cf.name.empty())
			++drop_counts[cf.name];
		return FmBlastOutcome::Removed;
	};

	const u64 end_ms =
			time_max > 0.0 ? porting::getTimeMs() + static_cast<u64>(time_max * 1000.0)
						   : 0;
	const auto timed_out = [&]() {
		return end_ms != 0 && porting::getTimeMs() >= end_ms;
	};
	int dr = 0, tnts = 1, ignited_tnts = 0, destroyed = 0, melted = 0;
	bool stopped_by_time = false, stopped_by_blocked = false, stopped_by_frontier = false;
	size_t last_active_rays = 0, last_blocked_rays = 0, last_frontier_rays = 0;
	double last_ray_strength = 0.0, total_strength = blast_strength;
	double remaining_strength = blast_strength;

	const auto ignite_terminal_tnt = [&](const v3pos_t &pos, MapNode node) {
		const content_t content = node.getContent();
		if (!tnt_contents.count(content) || content == tnt_burning_content ||
				tnt_burning_content == CONTENT_IGNORE || terminal_tnt_ignited.count(pos))
			return false;
		if (!ignore_protection && lua_is_node_protected(L, pos, owner))
			return false;
		if (!env->setNode(pos, MapNode(tnt_burning_content), 2))
			return false;
		terminal_tnt_ignited.emplace(pos);
		++ignited_tnts;
		return true;
	};

	const auto process_hit = [&](const v3pos_t &pos, FmBlastAngularHit &hit) {
		const content_t content = hit.node.getContent();
		const auto &cf = ndef->get(content);
		const bool empty_node = content == CONTENT_AIR || content == fire_content ||
								content == boom_content;
		const bool solid = !empty_node && (cf.walkable || cf.liquid_type != LIQUID_NONE);
		const double minimum = blast_min_strength;
		const double available = hit.energy;
		const double resistance = solid ? node_resistance(content) : 0.0;
		const double remaining = std::max(0.0, available - resistance);
		hit.outcome = solid ? FmBlastOutcome::Blocked : FmBlastOutcome::Transparent;
		if (hit.protected_node) {
			if (solid) {
				hit.energy = 0.0;
				++last_blocked_rays;
			}
			return;
		}
		if (remaining <= minimum) {
			hit.energy = 0.0;
			if (solid)
				++last_blocked_rays;
			return;
		}
		const bool weak_edge = available <= hit.distance_cost + resistance + 1.0;
		const int multiplier = itemgroup_get(cf.groups, "tnt_melt_level_multiplier");
		if (solid && liquid_real &&
				(multiplier > 0 ||
						(weak_edge && dr > melt_min_radius && melt_chance > 0 &&
								myrand_range(1, melt_chance) <= 1))) {
			MapNode changed_node = hit.node;
			const auto source_level = hit.node.getLevel(ndef);
			const int changed = changed_node.freeze_melt(ndef, melt_direction);
			if (changed) {
				const auto target_max = changed_node.getMaxLevel(ndef);
				if (multiplier > 1 && source_level > 0 && target_max > 0)
					changed_node.setLevel(
							ndef, std::clamp(static_cast<int>(source_level) * multiplier,
										  1, static_cast<int>(target_max)));
				if (env->swapNode(pos, changed_node)) {
					hit.outcome = FmBlastOutcome::Transformed;
					melted += changed;
				}
			}
		} else if (!empty_node && remaining >= 1.0) {
			hit.outcome =
					destroy_node(pos, hit.node, weak_edge, dr > fast_radius, available);
			if (hit.outcome == FmBlastOutcome::Removed ||
					hit.outcome == FmBlastOutcome::Transformed)
				++destroyed;
		}
		if (fm_blast_can_propagate(hit.outcome, solid))
			hit.energy = remaining;
		else {
			hit.energy = 0.0;
			if (solid)
				++last_blocked_rays;
		}
	};

	auto incoming = blast_strength > blast_min_strength
							? fm_blast_angular_seed(blast_strength)
							: std::vector<FmBlastFootprint>{};
	if (incoming.empty())
		remaining_strength = 0.0;
	while (!incoming.empty()) {
		if (timed_out()) {
			stopped_by_time = true;
			break;
		}
		++dr;
		fm_blast_angular_coalesce(incoming);
		auto layer = fm_blast_angular_project(incoming, dr);
		incoming.clear();
		if (dr <= blast_core_radius)
			fm_blast_angular_mix_core(layer, dr, blast_core_shell_fraction);
		fm_blast_angular_distance(layer, blast_distance_loss);
		last_blocked_rays = last_frontier_rays = last_active_rays = 0;
		size_t visited = 0;
		// Visit only projected hits. All processing updates the same energy record.
		for (auto &[rel, hit] : layer) {
			if ((visited++ & 255) == 0 && timed_out()) {
				stopped_by_time = true;
				break;
			}
			const v3pos_t pos = origin + rel;
			bool valid = false;
			hit.node = env->getMap().getNode(pos, &valid);
			if (!valid || hit.node.getContent() == CONTENT_IGNORE) {
				hit.energy = 0.0;
				++last_frontier_rays;
				continue;
			}
			hit.loaded = true;
			if (!hit.can_travel(blast_min_strength)) {
				ignite_terminal_tnt(pos, hit.node);
				hit.energy = 0.0;
				continue;
			}
			++last_active_rays;
			const auto content = hit.node.getContent();
			if (content != CONTENT_AIR && content != fire_content &&
					content != boom_content)
				hit.protected_node =
						!ignore_protection && lua_is_node_protected(L, pos, owner);
		}

		double shell_boost = 0.0;
		for (auto &[rel, hit] : layer) {
			if (!hit.loaded || hit.energy <= 0.0 ||
					!tnt_contents.contains(hit.node.getContent()))
				continue;
			const v3pos_t pos = origin + rel;
			if (hit.protected_node || hit.node.getContent() == tnt_burning_content) {
				hit.energy = 0.0;
				++last_blocked_rays;
				continue;
			}
			if (stopped_by_time || timed_out()) {
				stopped_by_time = true;
				if (ignite_terminal_tnt(pos, hit.node))
					chained_tnt.push_back(pos);
				hit.energy = 0.0;
				continue;
			}
			if (hit.energy < blast_tnt_absorb_strength) {
				ignite_terminal_tnt(pos, hit.node);
				hit.energy = 0.0;
				continue;
			}
			if (!remove_node(pos, 2)) {
				hit.energy = 0.0;
				++last_blocked_rays;
				continue;
			}
			hit.absorbed_tnt = true;
			hit.outcome = FmBlastOutcome::Removed;
			const double added = tnt_node_blast_strength(hit.node.getContent());
			const auto split = fm_blast_split(added, 1.0 - blast_tnt_ray_fraction);
			hit.energy += split.ray;
			shell_boost += split.shell;
			total_strength += added;
			++tnts;
		}
		if (shell_boost > 0.0) {
			size_t count = 0;
			for (const auto &[rel, hit] : layer)
				if (hit.loaded && !hit.protected_node && hit.energy > 0.0)
					++count;
			if (count > 0)
				for (auto &[rel, hit] : layer)
					if (hit.loaded && !hit.protected_node && hit.energy > 0.0)
						hit.energy += shell_boost / count;
		}
		if (!stopped_by_time) {
			visited = 0;
			for (auto &[rel, hit] : layer) {
				if ((visited++ & 255) == 0 && timed_out()) {
					stopped_by_time = true;
					break;
				}
				if (hit.loaded && !hit.absorbed_tnt && hit.energy > 0.0)
					process_hit(origin + rel, hit);
			}
		}
		remaining_strength = layer.energy();
		last_ray_strength =
				last_active_rays ? remaining_strength / last_active_rays : 0.0;
		if (stopped_by_time)
			break;
		layer.append_survivors(incoming);
		if (incoming.empty()) {
			stopped_by_blocked = last_blocked_rays > 0;
			stopped_by_frontier = !stopped_by_blocked && last_frontier_rays > 0;
		}
	}
	const char *stopped = stopped_by_time		? "time"
						  : stopped_by_blocked	? "blocked"
						  : stopped_by_frontier ? "frontier"
												: "strength";

	// A weak ray travelling through air must not enlarge all object/drop effects.
	const int effect_radius = static_cast<int>(std::min(
			static_cast<double>(dr), fm_blast_full_shell_radius(total_strength)));

	actionstream << tnts << " TNTs owned by " << owner << " detonated at " << origin
				 << " with radius=" << dr << " strength=" << total_strength
				 << " effect_radius=" << effect_radius
				 << " strength_left=" << remaining_strength
				 << " active_rays=" << last_active_rays
				 << " blocked_rays=" << last_blocked_rays
				 << " frontier_rays=" << last_frontier_rays
				 << " ray_strength=" << last_ray_strength << " stopped=" << stopped
				 << " ignited=" << ignited_tnts << " destroyed=" << destroyed
				 << " melted=" << melted << std::endl;

	// ModApiEnv is a friend of ScriptApiBase; keep protected access here.
	blast_objects(L, env, origin, effect_radius, [&](ServerActiveObject *object) {
		env->getScriptIface()->objectrefGetOrCreate(L, object);
	});

	lua_createtable(L, 0, 8);

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
	set_lua_number_field(L, "effect_radius", effect_radius);
	set_lua_number_field(L, "strength", total_strength);
	set_lua_number_field(L, "strength_left", remaining_strength);
	lua_pushstring(L, stopped);
	lua_setfield(L, -2, "stopped");

	return 1;
}
