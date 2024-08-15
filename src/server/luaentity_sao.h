/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2020 Minetest core developers & community

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

#include "unit_sao.h"

class LuaEntitySAO : public UnitSAO
{
public:
	LuaEntitySAO() = delete;
	// Used by the environment to load SAO
	LuaEntitySAO(ServerEnvironment *env, v3f pos, const std::string &data);
	// Used by the Lua API
	LuaEntitySAO(ServerEnvironment *env, v3f pos, const std::string &name,
			const std::string &state) :
			UnitSAO(env, pos),
			m_init_name(name), m_init_state(state)
	{
	}
	~LuaEntitySAO();

	ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_LUAENTITY; }
	ActiveObjectType getSendType() const { return ACTIVEOBJECT_TYPE_GENERIC; }
	virtual void addedToEnvironment(u32 dtime_s);
	void step(float dtime, bool send_recommended);
	std::string getClientInitializationData(u16 protocol_version);

	bool isStaticAllowed() const { return m_prop.static_save; }
	bool shouldUnload() const { return true; }
	void getStaticData(std::string *result) const;

	u32 punch(v3f dir, const ToolCapabilities *toolcap = nullptr,
			ServerActiveObject *puncher = nullptr,
			float time_from_last_punch = 1000000.0f,
			u16 initial_wear = 0);

	void rightClick(ServerActiveObject *clicker);

	void setPos(const v3f &pos);
	void moveTo(v3f pos, bool continuous);
	float getMinimumSavedMovement();

	std::string getDescription();

	void setHP(s32 hp, const PlayerHPChangeReason &reason);
	u16 getHP() const;

	/* LuaEntitySAO-specific */
	void setVelocity(v3f velocity);
	void addVelocity(v3f velocity) { m_velocity += velocity; }
	v3f getVelocity();
	void setAcceleration(v3f acceleration);
	v3f getAcceleration();

	void setTextureMod(const std::string &mod);
	std::string getTextureMod() const;
	void setSprite(v2s16 p, int num_frames, float framelength,
			bool select_horiz_by_yawpitch);
	std::string getName();
	bool getCollisionBox(aabb3f *toset) const;
	bool getSelectionBox(aabb3f *toset) const;
	bool collideWithObjects() const;

protected:
	void dispatchScriptDeactivate(bool removal);
	virtual void onMarkedForDeactivation() { dispatchScriptDeactivate(false); }
	virtual void onMarkedForRemoval() { dispatchScriptDeactivate(true); }

private:
	std::string getPropertyPacket();
	void sendPosition(bool do_interpolate, bool is_movement_end);
	std::string generateSetTextureModCommand() const;
	static std::string generateSetSpriteCommand(v2s16 p, u16 num_frames,
			f32 framelength, bool select_horiz_by_yawpitch);

	std::string m_init_name;
	std::string m_init_state;
	bool m_registered = false;

	v3f m_velocity;
	v3f m_acceleration;

	v3f m_last_sent_position;
	v3f m_last_sent_velocity;
	v3f m_last_sent_rotation;
	float m_last_sent_position_timer = 0.0f;
	float m_last_sent_move_precision = 0.0f;

	std::string m_texture_modifier;
	bool m_texture_modifier_sent = false;
};
