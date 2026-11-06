/*
 * Epixel
 * Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include "fallingsao.h"
#include "falling_placement.h"
#include "falling_map_events.h"
#include "collision.h"
#include "environment.h"
#include "irr_v3d.h"
#include "map.h"
#include "mapblock.h"
#include "mapnode.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "server.h"
//#include "scripting_game.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include <cmath>
#include <limits>
#include <sstream>

namespace epixel
{

namespace
{

constexpr float FALLINGSAO_WIND_ACCEL = 0.75f;
constexpr float FALLINGSAO_MAX_WIND_ACCEL = 6.0f * BS;

v3f getFallingSAOAcceleration(
		Map &map, const v3pos_t &pos, const v3f &velocity, const v3f &gravity)
{
	auto wind = map.getWind(pos, true);

	const v3f target_velocity(wind.X * BS, 0.0f, wind.Z * BS);
	v3f wind_accel =
			(target_velocity - v3f(velocity.X, 0.0f, velocity.Z)) * FALLINGSAO_WIND_ACCEL;
	const float accel_length = wind_accel.getLength();
	if (accel_length > FALLINGSAO_MAX_WIND_ACCEL && accel_length > 0.0001f)
		wind_accel *= FALLINGSAO_MAX_WIND_ACCEL / accel_length;

	return wind_accel + gravity;
}
}

FallingSAO::FallingSAO(ServerEnvironment *env, v3opos_t pos, const std::string &name,
		const std::string &state, int fast_) : LuaEntitySAO(env, pos, name, state)
{
	/*
	if(env == NULL) {
		ServerActiveObject::registerType(getType(), create);
		return;
	}
*/

	m_prop.physical = true;
	m_prop.hp_max = 1;
	m_prop.collideWithObjects = false;
	m_prop.collisionbox = core::aabbox3d<f32>(-0.5, -0.5, -0.5, 0.5, 0.5, 0.5);
	m_prop.visual = OBJECTVISUAL_WIELDITEM;
	m_prop.textures.clear();
	m_prop.visual_size = v3f(0.667, 0.667, 0.667);
	fast = fast_;
}

FallingSAO::~FallingSAO()
{
}

void FallingSAO::attachNode(const MapNode n)
{
	m_node = n;
	m_prop.visual = OBJECTVISUAL_NODE;
	m_prop.node = n;
	m_prop.visual_size = v3f(1, 1, 1);
	m_prop.is_visible = true;
	m_prop.textures = {m_env->getGameDef()->ndef()->get(n).name};
	notifyObjectPropertiesModified();
}

bool FallingSAO::captureNodeState(const v3pos_t &pos)
{
	const v3bpos_t block_pos = getNodeBlockPos(pos);
	auto *block = m_env->getMap().getBlockNoCreateNoEx(block_pos, false, true);
	if (!block)
		return false;
	const v3pos_t relative_pos = pos - block_pos * MAP_BLOCKSIZE;
	if (auto *meta = block->m_node_metadata.get(relative_pos); meta && !meta->empty()) {
		std::ostringstream os(std::ios::binary);
		meta->serialize(os, 2, true);
		m_metadata = os.str();
	}
	m_timer = block->getNodeTimer(relative_pos);
	m_timer.position = pos;
	return true;
}

void FallingSAO::getStaticData(std::string *result) const
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, 3);
	os << serializeString16(m_env->getGameDef()->ndef()->get(m_node).name);
	writeU8(os, m_node.param1);
	writeU8(os, m_node.param2);
	os << serializeString32(m_metadata);
	writeV3F1000(os, clampToF1000(const_cast<FallingSAO *>(this)->getVelocity()));
	writeU8(os, fast);
	m_timer.serialize(os);
	*result = os.str();
}

ServerActiveObject *FallingSAO::create(
		ServerEnvironment *env, v3opos_t pos, const std::string &data)
{
	std::istringstream is(data, std::ios::binary);
	const u8 version = data.empty() ? 0 : readU8(is);
	if (version != 2 && version != 3) {
		warningstream << "Cannot restore legacy falling object without node data"
					  << std::endl;
		return nullptr;
	}
	const std::string name = deSerializeString16(is);
	const u8 param1 = readU8(is);
	const u8 param2 = readU8(is);
	const std::string metadata = deSerializeString32(is);
	const v3f velocity = readV3F1000(is);
	const int fast = readU8(is);
	NodeTimer timer;
	if (version >= 3)
		timer.deSerialize(is);
	content_t content;
	if (!env->getGameDef()->ndef()->getId(name, content)) {
		warningstream << "Cannot restore falling node with unknown name: " << name
					  << std::endl;
		return nullptr;
	}
	auto *sao = new FallingSAO(env, pos, "__builtin:falling_node", "", fast);
	sao->attachNode(MapNode(content, param1, param2));
	sao->m_metadata = metadata;
	sao->m_timer = timer;
	sao->setVelocity(velocity);
	return sao;
}

void FallingSAO::addedToEnvironment(u32 dtime_s)
{
	ServerActiveObject::addedToEnvironment(dtime_s);
	m_registered = false;

	// And make it immortal
	ItemGroupList armor_groups;
	armor_groups["immortal"] = 1;
	setArmorGroups(armor_groups);
}

bool FallingSAO::placeNode(const v3pos_t &pos, const MapNode &node)
{
	if (!m_env->setNode(pos, node, fast))
		return false;
	if (fast)
		notifyFallingNodeChange(m_env->getMap(), pos, MEET_ADDNODE);
	return true;
}

bool FallingSAO::escapeFromNode(const v3opos_t &pos, const v3pos_t &node_pos)
{
	auto &map = m_env->getMap();
	const MapNode node = map.getNode(node_pos);
	auto *ndef = m_env->getGameDef()->getNodeDefManager();
	if (!node || !ndef->get(node).walkable)
		return false;

	// Use the node's actual collision boxes, not just its walkable flag:
	// a falling node can legitimately occupy the empty part of a nodebox.
	std::vector<aabb3f> node_boxes;
	node.getCollisionBoxes(ndef, &node_boxes, node.getNeighbors(node_pos, &map));
	aabb3f object_box = m_prop.collisionbox;
	object_box.MinEdge *= BS;
	object_box.MaxEdge *= BS;
	aabb3o overlap_box = ToOpos(object_box);
	overlap_box.MinEdge += pos + v3opos_t(0.1f * BS);
	overlap_box.MaxEdge += pos - v3opos_t(0.1f * BS);
	const v3opos_t node_origin = intToFloat(node_pos, BS);
	bool embedded = false;
	for (const auto &boxf : node_boxes) {
		auto box = ToOpos(boxf);
		box.MinEdge += node_origin;
		box.MaxEdge += node_origin;
		if (box.intersectsWithBox(overlap_box)) {
			embedded = true;
			break;
		}
	}
	if (!embedded)
		return false;

	// Match dropped-item recovery by moving upward, but only to a loaded
	// position where the entire falling object fits. Never replace a node.
	constexpr pos_t max_escape_nodes = 16;
	for (pos_t offset = 1; offset <= max_escape_nodes; ++offset) {
		if (node_pos.Y > std::numeric_limits<pos_t>::max() - offset)
			break;
		const v3pos_t candidate_node = node_pos + v3pos_t(0, offset, 0);
		if (!map.getNode(candidate_node))
			break;
		v3opos_t candidate = pos;
		candidate.Y = intToFloat(candidate_node, BS).Y;
		if (collision_check_intersection(
					m_env, m_env->getGameDef(), object_box, candidate, this, false))
			continue;
		setPos(candidate);
		v3f velocity = getVelocity();
		velocity.Y = 0.0f;
		setVelocity(velocity);
		return true;
	}
	return false;
}

void FallingSAO::step(float dtime, bool send_recommended)
{
	// Object pending removal, skip
	if (m_pending_removal || !m_env) {
		return;
	}

	// If no texture, remove it
	if (m_prop.textures.empty()) {
		m_pending_removal = true;
		return;
	}

	const v3opos_t current_pos = getBasePosition();
	const v3pos_t node_pos = floatToInt(current_pos, BS);
	m_stuck_check_timer -= dtime;
	if (m_stuck_check_timer <= 0.0f) {
		m_stuck_check_timer = 0.5f;
		if (escapeFromNode(current_pos, node_pos))
			return;
	}
	setAcceleration(getFallingSAOAcceleration(m_env->getMap(), node_pos, getVelocity(),
			m_env->getFallingGravity(getNodeBlockPos(node_pos))));
	CollisionMoveResult movement;
	stepWithCollision(dtime, send_recommended, &movement);

	auto *ndef = m_env->getGameDef()->getNodeDefManager();
	const ContentFeatures &falling = ndef->get(m_node);
	// Ordinary nodes in free fall cannot land until movement reports a collision.
	if (!movement.touching_ground && !falling.isLiquid() &&
			itemgroup_get(falling.groups, "float") == 0)
		return;

	// Under node, center
	const auto m_base_position = getBasePosition();
	v3pos_t p = floatToInt(m_base_position, BS);
	v3pos_t p_under(p.X, p.Y - 1, p.Z);
	bool landed = false;
	bool has_target_node = false;
	MapNode target_node;
	if (movement.touching_ground) {
		for (const auto &collision : movement.collisions) {
			if (collision.type == COLLISION_NODE && collision.axis == COLLISION_AXIS_Y &&
					collision.old_speed.Y <= 0) {
				p_under = collision.node_p;
				landed = true;
				break;
			}
		}
	}
	if (landed) {
		auto &map = m_env->getMap();
		auto usable = [&](const v3pos_t &support_pos) {
			// Do not teleport across long protruding node boxes.
			if (std::abs(static_cast<long double>(support_pos.X) - p.X) > 1 ||
					std::abs(static_cast<long double>(support_pos.Z) - p.Z) > 1)
				return false;
			const MapNode support = map.getNode(support_pos);
			const MapNode target = map.getNode(support_pos + v3pos_t(0, 1, 0));
			if (!support || !target)
				return false;
			const auto &support_def = ndef->get(support);
			const auto &target_def = ndef->get(target);
			return support_def.walkable && !support_def.buildable_to &&
				   (target.getContent() == CONTENT_AIR || target_def.buildable_to);
		};

		const v3pos_t target = p_under + v3pos_t(0, 1, 0);
		target_node = map.getNode(target);
		has_target_node = true;
		const auto &target_def = ndef->get(target_node);
		const bool blocked = !target_node || (target_node.getContent() != CONTENT_AIR &&
													 !target_def.buildable_to);
		if (p.X != p_under.X || p.Z != p_under.Z || blocked) {
			v3pos_t support;
			if (!findFallingSupport(p_under, usable, support)) {
				// No safe placement is available. Move away and retry on a later step.
				v3f velocity = getVelocity();
				static constexpr int directions[][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
				const auto &direction = directions[m_blocked_attempts++ % 4];
				velocity.X = direction[0] * 3.0f * BS;
				velocity.Z = direction[1] * 3.0f * BS;
				setVelocity(velocity);
				return;
			}
			if (support != p_under)
				has_target_node = false;
			p_under = support;
		}
		p = p_under + v3pos_t(0, 1, 0);
	}
	/*
	bool cur_exists = false, under_exists = false;
*/
	MapNode n = has_target_node ? target_node : m_env->getMap().getNode(p);
	MapNode n_under = m_env->getMap().getNode(p_under);
	const ContentFeatures &f = ndef->get(n), &f_under = ndef->get(n_under);

	bool cur_exists = n, under_exists = n_under;

	// Mapblock current or under is not loaded, stop there
	if (!n || !cur_exists || !under_exists) {
		return;
	}

	if ((landed || (itemgroup_get(falling.groups, "float") && f_under.isLiquid() &&
						   !(falling.liquid_type == LIQUID_SOURCE &&
								   f_under.liquid_type == LIQUID_FLOWING))) ||
			(f_under.isLiquid() && f.isLiquid() && falling.isLiquid() &&
					!(itemgroup_get(falling.groups, "float") &&
							falling.liquid_type == LIQUID_SOURCE &&
							f_under.liquid_type == LIQUID_FLOWING))) {
		// A liquid node is compressible only when it has pressure. Flowing
		// sand is a level-1 liquid without pressure, so each block must occupy
		// its own node rather than disappearing into a compressed pile.
		const bool compress =
				falling.isLiquid() && itemgroup_get(falling.groups, "pressure") > 0;
		if (m_metadata.empty() && m_timer.timeout <= 0 &&
				(f_under.leveled || f_under.param_type_2 == CPT2_LEVELED) &&
				n_under.getLevel(ndef) < n_under.getMaxLevel(ndef, compress) &&
				(n_under.getContent() == m_node.getContent() ||
						f_under.liquid_alternative_flowing_id == m_node.getContent() ||
						f_under.liquid_alternative_source_id == m_node.getContent())) {
			const auto *support_meta = m_env->getMap().getNodeMetadata(p_under);
			std::unique_ptr<NodeMetadata> saved_support;
			if (support_meta && !support_meta->empty()) {
				std::ostringstream os(std::ios::binary);
				support_meta->serialize(os, 2, true);
				std::istringstream is(os.str(), std::ios::binary);
				saved_support =
						std::make_unique<NodeMetadata>(m_env->getGameDef()->idef());
				saved_support->deSerialize(is, 2);
			}
			const s16 remaining = n_under.addLevel(ndef, m_node.getLevel(ndef), compress);
			if (remaining < 0 || !placeNode(p_under, n_under))
				return;
			if (saved_support &&
					m_env->getMap().setNodeMetadata(p_under, saved_support.get()))
				saved_support.release();
			m_env->getServerMap().transforming_liquid_add(p_under);
			if (remaining > 0) {
				// Keep the remainder as an entity until it can be placed safely.
				m_node.setLevel(ndef, remaining, compress);
				attachNode(m_node);
				return;
			}
			m_pending_removal = true;
			return;
		}
		/*
		else if (f_under.buildable_to &&
				(itemgroup_get(f.groups,"float") == 0 ||
				 f_under.liquid_type == LIQUID_NONE)) {
			m_env->removeNode(p_under, fast);
			//return;
		}
*/
		if (n.getContent() != CONTENT_AIR && !f.buildable_to)
			return;
		if (!placeNode(p, m_node))
			return;
		m_env->getMap().removeNodeMetadata(p);
		m_env->getMap().removeNodeTimer(p);
		if (m_timer.timeout > 0) {
			m_timer.position = p;
			m_env->getMap().setNodeTimer(m_timer);
		}
		if (!m_metadata.empty()) {
			auto meta = std::make_unique<NodeMetadata>(m_env->getGameDef()->idef());
			std::istringstream is(m_metadata, std::ios::binary);
			meta->deSerialize(is, 2);
			if (m_env->getMap().setNodeMetadata(p, meta.get()))
				meta.release();
		}
		m_pending_removal = true;
		if (falling.isLiquid())
			m_env->getServerMap().transforming_liquid_add(p);
		m_env->nodeUpdate(p, 1, fast);
		return;
	}
}

}
