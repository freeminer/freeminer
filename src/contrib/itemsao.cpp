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

#include "itemsao.h"
#include "environment.h"
#include "irr_v3d.h"
#include "map.h"
#include "nodedef.h"
#include "scripting_server.h"
#include "server.h"
//#include "scripting_game.h"
#include "sound.h"
#include "util/serialize.h"
#include <sstream>

namespace epixel
{

ItemSAO::ItemSAO(ServerEnvironment *env, v3opos_t pos,
		const std::string &name, const std::string &state):
		LuaEntitySAO(env, pos, name, state),
		m_timer_before_loot(1.0f), m_life_timer(600.0f), m_check_current_node_timer(1.8f)
{
/*
	if(env == NULL) {
		ServerActiveObject::registerType(getType(), create);
		return;
	}
*/
	m_prop.physical = true;
	m_prop.hp_max = 1;
	m_prop.mesh = "empty.obj";
	m_prop.collideWithObjects = false;
	m_prop.collisionbox = core::aabbox3d<f32>(-0.3, -0.3, -0.3, 0.3, 0.3, 0.3);
	m_prop.visual = OBJECTVISUAL_WIELDITEM;
	m_prop.visual_size = v3f(0.4,0.4, 0.4);
	m_prop.spritediv = v2s16(1,1);
	m_prop.initial_sprite_basepos = v2s16(0,0);
	m_prop.is_visible = false;
}

ItemSAO::~ItemSAO()
{
}

ServerActiveObject* ItemSAO::create(ServerEnvironment *env, v3opos_t pos,
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
	infostream << "ItemSAO::create(name=\"" << name << "\" state=\""
			<< state << "\")" << std::endl;
	epixel::ItemSAO *sao = new epixel::ItemSAO(env, pos, name, state);
	sao->m_hp = hp;
	sao->setVelocity(velocity);
	sao->setRotation({0, yaw, 0});
	return sao;
}

void ItemSAO::addedToEnvironment(u32 dtime_s)
{
	ServerActiveObject::addedToEnvironment(dtime_s);
	m_env->getScriptIface()->
		luaentity_Add(m_id, m_init_name.c_str(), true);
	m_registered = true;

	// Add an axis to make entity do a little jump
	setVelocity(v3f(0, 2 * BS, 0));
	setAcceleration(v3f(0, -10 * BS, 0));

	// And make it immortal
	ItemGroupList armor_groups;
	armor_groups["immortal"] = 1;
	setArmorGroups(armor_groups);
}

void ItemSAO::step(float dtime, bool send_recommended)
{
	LuaEntitySAO::step(dtime, send_recommended);

	m_timer_before_loot -= dtime;
	// When loot timer expire, stop object move
	const auto velocity = getVelocity();
	if (m_timer_before_loot <= 0.0f && velocity != v3f(0,0,0)) {
		setVelocity(v3f(0,velocity.Y,0));
	}

	m_life_timer -= dtime;
	// Remove SAO is lifetime expire
	if (m_life_timer <= 0.0f) {
		m_pending_removal = true;
	}

	m_check_current_node_timer -= dtime;
	// Check on which node is the SAO
	if (m_check_current_node_timer <= 0.0f) {
		const auto m_base_position = getBasePosition();
		v3pos_t p(m_base_position.X / BS, m_base_position.Y / BS, m_base_position.Z / BS);
		MapNode node = m_env->getMap().getNode(p);
		auto* ndef = ((Server*)m_env->getGameDef())->getNodeDefManager();
		std::string nodeName = ndef->get(node).name;

		// If node is lava, burn it
		if (nodeName.compare("default:lava_flowing") == 0 ||
				nodeName.compare("default:lava_source") == 0) {

			ServerPlayingSound params;
			params.spec.name = "builtin_item_lava";
			params.object = getId();
			params.type = SoundLocation::Object;
			params.max_hear_distance = 15.0f * BS;

			((Server*)m_env->getGameDef())->playSound(params);
			m_pending_removal = true;
		}
		// Check every 4 cycles
		m_check_current_node_timer = 1.2f;
	}
}

}
