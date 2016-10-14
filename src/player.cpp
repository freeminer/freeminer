/*
player.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "player.h"

#include "threading/mutex_auto_lock.h"
#include "util/numeric.h"
#include "hud.h"
#include "constants.h"
#include "gamedef.h"
#include "settings.h"
//#include "content_sao.h"
//#include "filesys.h"
#include "log_types.h"
#include "porting.h"  // strlcpy


Player::Player(const std::string & name, IItemDefManager *idef):
	camera_barely_in_ceiling(false),
	inventory(idef),
	hp(PLAYER_MAX_HP),
	peer_id(PEER_ID_INEXISTENT),
	keyPressed(0),
// protected
	m_breath(PLAYER_MAX_BREATH),
	m_pitch(0),
	m_yaw(0),
	m_speed(0,0,0),
	m_position(0,0,0),
	m_collisionbox(-BS*0.30,0.0,-BS*0.30,BS*0.30,BS*1.75,BS*0.30)
{
	hp = PLAYER_MAX_HP;

	peer_id = PEER_ID_INEXISTENT;
	m_name = name;
	hotbar_image_items = 0;

	inventory.clear();
	inventory.addList("main", PLAYER_INVENTORY_SIZE);
	InventoryList *craft = inventory.addList("craft", 9);
	craft->setWidth(3);
	inventory.addList("craftpreview", 1);
	inventory.addList("craftresult", 1);
	inventory.setModified(false);

	// Can be redefined via Lua
	inventory_formspec = "size[8,7.5]"
		//"image[1,0.6;1,2;player.png]"
		"list[current_player;main;0,3.5;8,4;]"
		"list[current_player;craft;3,0;3,3;]"
		"listring[]"
		"list[current_player;craftpreview;7,1;1,1;]";

	// Initialize movement settings at default values, so movement can work
	// if the server fails to send them
	movement_acceleration_default   = 3    * BS;
	movement_acceleration_air       = 2    * BS;
	movement_acceleration_fast      = 10   * BS;
	movement_speed_walk             = 4    * BS;
	movement_speed_crouch           = 1.35 * BS;
	movement_speed_fast             = 20   * BS;
	movement_speed_climb            = 2    * BS;
	movement_speed_jump             = 6.5  * BS;
	movement_liquid_fluidity        = 1    * BS;
	movement_liquid_fluidity_smooth = 0.5  * BS;
	movement_liquid_sink            = 10   * BS;
	movement_gravity                = 9.81 * BS;
	movement_fall_aerodynamics      = 110;
	local_animation_speed           = 0.0;

	hud_flags =
		HUD_FLAG_HOTBAR_VISIBLE    | HUD_FLAG_HEALTHBAR_VISIBLE |
		HUD_FLAG_CROSSHAIR_VISIBLE | HUD_FLAG_WIELDITEM_VISIBLE |
		HUD_FLAG_BREATHBAR_VISIBLE | HUD_FLAG_MINIMAP_VISIBLE;

	hud_hotbar_itemcount = HUD_HOTBAR_ITEMCOUNT_DEFAULT;
}

Player::~Player()
{
	clearHud();
}

v3s16 Player::getLightPosition() const
{
	return floatToInt(m_position + v3f(0,BS+BS/2,0), BS);
}

u32 Player::addHud(HudElement *toadd)
{
	MutexAutoLock lock(m_mutex);

	u32 id = getFreeHudID();

	if (id < hud.size())
		hud[id] = toadd;
	else
		hud.push_back(toadd);

	return id;
}

HudElement* Player::getHud(u32 id)
{
	MutexAutoLock lock(m_mutex);

	if (id < hud.size())
		return hud[id];

	return NULL;
}

HudElement* Player::removeHud(u32 id)
{
	MutexAutoLock lock(m_mutex);

	HudElement* retval = NULL;
	if (id < hud.size()) {
		retval = hud[id];
		hud[id] = NULL;
	}
	return retval;
}

void Player::clearHud()
{
	MutexAutoLock lock(m_mutex);

	while(!hud.empty()) {
		delete hud.back();
		hud.pop_back();
	}
}

//freeminer part:
void Player::addSpeed(v3f speed) {
		auto lock = lock_unique_rec();
		m_speed += speed;
}

Json::Value operator<<(Json::Value &json, v3f &v) {
	json["X"] = v.X;
	json["Y"] = v.Y;
	json["Z"] = v.Z;
	return json;
}

Json::Value operator>>(Json::Value &json, v3f &v) {
	v.X = json["X"].asFloat();
	v.Y = json["Y"].asFloat();
	v.Z = json["Z"].asFloat();
	return json;
}

Json::Value operator<<(Json::Value &json, Player &player) {
	std::ostringstream ss(std::ios_base::binary);
	//todo
	player.inventory.serialize(ss);
	json["inventory_old"] = ss.str();

	json["name"] = player.m_name;
	json["pitch"] = player.getPitch();
	json["yaw"] = player.getYaw();
	auto pos = player.getPosition();
	json["position"] << pos;
	json["hp"] = player.hp.load();
	json["breath"] = player.getBreath();
	return json;
}

Json::Value operator>>(Json::Value &json, Player &player) {
	player.m_name = json["name"].asCString();
	player.setPitch(json["pitch"].asFloat());
	player.setYaw(json["yaw"].asFloat());
	v3f position;
	json["position"]>>position;
	player.setPosition(position);
	player.hp = json["hp"].asInt();
	player.setBreath(json["breath"].asInt());

	//todo
	std::istringstream ss(json["inventory_old"].asString());
	auto & inventory = player.inventory;
	inventory.deSerialize(ss);

	if(inventory.getList("craftpreview") == NULL)
	{
		// Convert players without craftpreview
		inventory.addList("craftpreview", 1);

		bool craftresult_is_preview = true;
		//if(args.exists("craftresult_is_preview"))
		//	craftresult_is_preview = args.getBool("craftresult_is_preview");
		if(craftresult_is_preview)
		{
			// Clear craftresult
			inventory.getList("craftresult")->changeItem(0, ItemStack());
		}
	}

	return json;
}
// end of freeminer
