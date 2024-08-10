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
#include "environment.h"
#include "inventory.h"
#include "irr_v3d.h"
#include "map.h"
#include "mapnode.h"
#include "nodedef.h"
#include "scripting_server.h"
#include "server.h"
//#include "scripting_game.h"
#include "util/serialize.h"
#include <sstream>

#include "log_types.h"

namespace epixel
{

FallingSAO::FallingSAO(ServerEnvironment *env, v3opos_t pos,
		const std::string &name, const std::string &state, int fast_):
		LuaEntitySAO(env, pos, name, state)
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
	m_prop.visual = "wielditem";
	m_prop.textures.clear();
	m_prop.visual_size = v3f(0.667,0.667,0.667);
	fast = fast_;
}

FallingSAO::~FallingSAO()
{
}

ServerActiveObject* FallingSAO::create(ServerEnvironment *env, v3opos_t pos,
		const std::string &data)
{
	std::string name;
	std::string state;
	s16 hp = 1;
	v3f velocity;
	float yaw = 0;
	if(data != "") {
		std::istringstream is(data, std::ios::binary);
		// read version
		u8 version = readU8(is);
		// check if version is supported
		if(version == 0){
			name = deSerializeString16(is);
			state = deSerializeString32(is);
		}
		else if(version == 1){
			name = deSerializeString16(is);
			state = deSerializeString32(is);
			hp = readS16(is);
			velocity = readV3F1000(is);
			yaw = readF1000(is);
		}
	}
	// create object
	//infostream<<"FallingSAO::create(name='%s' state='%s')", name.c_str(), state.c_str();
	epixel::FallingSAO *sao = new epixel::FallingSAO(env, pos, name, state);
	sao->m_hp = hp;
	sao->setVelocity(velocity);
	sao->setRotation({0, yaw, 0});
	return sao;
}

void FallingSAO::addedToEnvironment(u32 dtime_s)
{
	ServerActiveObject::addedToEnvironment(dtime_s);
	m_env->getScriptIface()->
		luaentity_Add(m_id, m_init_name.c_str(), true);
	m_registered = true;

	// And make it immortal
	ItemGroupList armor_groups;
	armor_groups["immortal"] = 1;
	setArmorGroups(armor_groups);
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

	LuaEntitySAO::step(dtime, send_recommended);

	auto* ndef = m_env->getGameDef()->getNodeDefManager();

	setAcceleration(v3f(0,-10*BS,0));
	// Under node, center
	const auto m_base_position = getBasePosition();
	v3pos_t p = floatToInt(m_base_position, BS);
	v3pos_t p_under(p.X, p.Y - 1, p.Z);
/*
	bool cur_exists = false, under_exists = false;
*/
	MapNode n = m_env->getMap().getNode(p),
			n_under = m_env->getMap().getNode(p_under);
	const ContentFeatures &f = ndef->get(n), &f_under = ndef->get(n_under);

	bool cur_exists = n, under_exists = n_under;

	// Mapblock current or under is not loaded, stop there
	if (!n || !cur_exists || !under_exists) {
		return;
	}

	if ((f_under.walkable || (itemgroup_get(f_under.groups, "float") &&
			f_under.liquid_type == LIQUID_NONE))||
			(f_under.isLiquid() && f.isLiquid())
			) {
		const std::string & n_name = ndef->get(m_node).name;
		if (f_under.leveled && 
		(n_under.getContent() == m_node.getContent() || f_under.liquid_alternative_flowing_id == m_node.getContent() || f_under.liquid_alternative_source_id == m_node.getContent())
		) {
			u8 addLevel = m_node.getLevel(ndef);
			const auto compress = f.isLiquid();
			addLevel = n_under.addLevel(ndef, addLevel, compress);
			if (addLevel) {
				m_node.setLevel(ndef, addLevel, compress);
				m_env->setNode(p, m_node, fast);
			}
			m_env->setNode(p_under, n_under, fast);
			m_pending_removal = true;
			m_env->getServerMap().transforming_liquid_add(p_under);
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
		if (n.getContent() != CONTENT_AIR &&
				(f.liquid_type == LIQUID_NONE)) {
			m_env->removeNode(p);
			if (!f.buildable_to) {
				ItemStack stack;
				stack.deSerialize(n_name);
				m_env->spawnItemActiveObject(n_name, m_base_position, stack);
				m_pending_removal = true;
				return;
			}
		}
		m_env->setNode(p, m_node, fast);
		m_pending_removal = true;
		m_env->nodeUpdate(p, 1, fast);
		return;
	}
}

}
