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
#include "map.h"
#include "mapnode.h"
#include "nodedef.h"
#include "server.h"
#include "scripting_game.h"
#include "util/serialize.h"
#include <sstream>

#include "log_types.h"

namespace epixel
{

FallingSAO::FallingSAO(ServerEnvironment *env, v3f pos,
		const std::string &name, const std::string &state, int fast_):
		LuaEntitySAO(env, pos, name, state)
{
	if(env == NULL) {
		ServerActiveObject::registerType(getType(), create);
		return;
	}

	m_prop.physical = true;
	m_prop.hp_max = 1;
	m_prop.collideWithObjects = false;
	m_prop.collisionbox = core::aabbox3d<f32>(-0.5, -0.5, -0.5, 0.5, 0.5, 0.5);
	m_prop.visual = "wielditem";
	m_prop.textures = {};
	m_prop.visual_size = v2f(0.667,0.667);
	fast = fast_;
}

FallingSAO::~FallingSAO()
{
}

ServerActiveObject* FallingSAO::create(ServerEnvironment *env, v3f pos,
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
			name = deSerializeString(is);
			state = deSerializeLongString(is);
		}
		else if(version == 1){
			name = deSerializeString(is);
			state = deSerializeLongString(is);
			hp = readS16(is);
			velocity = readV3F1000(is);
			yaw = readF1000(is);
		}
	}
	// create object
/*
	infostream << "FallingSAO::create(name=\"" << name << "\" state=\""
			<< state << "\")" << std::endl;
*/
	epixel::FallingSAO *sao = new epixel::FallingSAO(env, pos, name, state);
	sao->m_hp = hp;
	sao->m_velocity = velocity;
	sao->m_yaw = yaw;
	return sao;
}

void FallingSAO::addedToEnvironment(u32 dtime_s)
{
	ServerActiveObject::addedToEnvironment(dtime_s);
	m_env->getScriptIface()->
		luaentity_Add(m_id, m_init_name.c_str());
	m_registered = true;

	// And make it immortal
	std::map<std::string, int> armor_groups;
	armor_groups["immortal"] = 1;
	setArmorGroups(armor_groups);
}

void FallingSAO::step(float dtime, bool send_recommended)
{
	// If no texture, remove it
	if (m_prop.textures.size() == 0) {
		m_removed = true;
		return;
	}

	LuaEntitySAO::step(dtime, send_recommended);

	INodeDefManager* ndef = m_env->getGameDef()->getNodeDefManager();

	m_acceleration = v3f(0,-10*BS,0);
	// Under node, center
	v3f p_under(m_base_position.X, m_base_position.Y - 7, m_base_position.Z);
	v3s16 p = floatToInt(m_base_position, BS);
	//bool exists = false;
	MapNode n = m_env->getMap().getNode(p),
			n_under = m_env->getMap().getNode(floatToInt(p_under, BS));
	const ContentFeatures &f = ndef->get(n), f_under = ndef->get(n_under);

	if (!n || (f_under.walkable || (itemgroup_get(f_under.groups, "float") &&
			f_under.liquid_type == LIQUID_NONE))) {
		if (n && f_under.leveled && f_under.name.compare(f.name) == 0) {
			u8 addLevel = n.getLevel(ndef);
			if (addLevel == 0) {
				addLevel = n_under.getLevel(ndef);
			}

			if (n_under.addLevel(ndef, addLevel)) {
				m_removed = true;
				return;
			}
		}
		else if (n && f_under.buildable_to &&
				(itemgroup_get(f.groups,"float") == 0 ||
				 f_under.liquid_type == LIQUID_NONE)) {
			m_env->removeNode(floatToInt(p_under, BS), fast);
			return;
		}

		if (n.getContent() != CONTENT_AIR &&
				(n.getContent() == CONTENT_IGNORE || f.liquid_type == LIQUID_NONE)) {
			m_env->removeNode(p, fast);
			if (!f.buildable_to) {
				ItemStack stack;
				std::string n_name = ndef->get(m_node).name;
				stack.deSerialize(n_name);
				m_env->spawnItemActiveObject(n_name, m_base_position, stack);
			}
		}
		m_env->setNode(p,m_node, fast);
		m_removed = true;
		m_env->nodeUpdate(p, 2, fast);
		return;
	}
}

}
