/*
player.h
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

#ifndef PLAYER_HEADER
#define PLAYER_HEADER

#include "irrlichttypes_bloated.h"
#include "inventory.h"
#include "constants.h" // BS
#include "threading/mutex.h"
#include <list>
#include "threading/lock.h"
#include "json/json.h"

#define PLAYERNAME_SIZE 20

#define PLAYERNAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
#define PLAYERNAME_ALLOWED_CHARS_USER_EXPL "'a' to 'z', 'A' to 'Z', '0' to '9', '-', '_'"

struct PlayerControl
{
	PlayerControl()
	{
		up = false;
		down = false;
		left = false;
		right = false;
		jump = false;
		aux1 = false;
		sneak = false;
		LMB = false;
		RMB = false;
		pitch = 0;
		yaw = 0;
		sidew_move_joystick_axis = .0f;
		forw_move_joystick_axis = .0f;
	}

	PlayerControl(
		bool a_up,
		bool a_down,
		bool a_left,
		bool a_right,
		bool a_jump,
		bool a_aux1,
		bool a_sneak,
		bool a_zoom,
		bool a_LMB,
		bool a_RMB,
		float a_pitch,
		float a_yaw,
		float a_sidew_move_joystick_axis,
		float a_forw_move_joystick_axis
	)
	{
		up = a_up;
		down = a_down;
		left = a_left;
		right = a_right;
		jump = a_jump;
		aux1 = a_aux1;
		sneak = a_sneak;
		zoom = a_zoom;
		LMB = a_LMB;
		RMB = a_RMB;
		pitch = a_pitch;
		yaw = a_yaw;
		sidew_move_joystick_axis = a_sidew_move_joystick_axis;
		forw_move_joystick_axis = a_forw_move_joystick_axis;
	}
	bool up;
	bool down;
	bool left;
	bool right;
	bool jump;
	bool aux1;
	bool sneak;
	bool zoom;
	bool LMB;
	bool RMB;
	float pitch;
	float yaw;
	float sidew_move_joystick_axis;
	float forw_move_joystick_axis;
};

class Map;
struct CollisionInfo;
struct HudElement;
class Environment;

// IMPORTANT:
// Do *not* perform an assignment or copy operation on a Player or
// RemotePlayer object!  This will copy the lock held for HUD synchronization
class Player
: public locker<>
{
public:

	Player(const std::string & name, IItemDefManager *idef);
	virtual ~Player() = 0;

	virtual void move(f32 dtime, Environment *env, f32 pos_max_d)
	{}
	virtual void move(f32 dtime, Environment *env, f32 pos_max_d,
			std::vector<CollisionInfo> *collision_info)
	{}

	v3f getSpeed()
	{
		auto lock = lock_shared_rec();
		return m_speed;
	}

	void setSpeed(v3f speed)
	{
		auto lock = lock_unique_rec();
		m_speed = speed;
	}

	void addSpeed(v3f speed);

	v3f getPosition()
	{
		auto lock = lock_shared_rec();
		return m_position;
	}

	v3s16 getLightPosition() const;

 	v3f getEyeOffset()
	{
		float eye_height = camera_barely_in_ceiling ? 1.5f : 1.625f;
		return v3f(0, BS * eye_height, 0);
	}

	v3f getEyePosition()
	{
		auto lock = lock_shared_rec();
		return m_position + getEyeOffset();
	}

	virtual void setPosition(const v3f &position)
	{
		auto lock = lock_unique_rec();
		m_position = position;
	}

	virtual void setPitch(f32 pitch)
	{
		auto lock = lock_unique_rec();
		m_pitch = pitch;
	}

	virtual void setYaw(f32 yaw)
	{
		auto lock = lock_unique_rec();
		m_yaw = yaw;
	}

	f32 getPitch() { auto lock = lock_shared_rec(); return m_pitch; }
	f32 getYaw() { auto lock = lock_shared_rec(); return m_yaw; }
	u16 getBreath() { auto lock = lock_shared_rec(); return m_breath; }

	virtual void setBreath(u16 breath) { m_breath = breath; }

	f32 getRadPitch() const { return m_pitch * core::DEGTORAD; }
	f32 getRadYaw() const { return m_yaw * core::DEGTORAD; }
	const std::string &getName() const { return m_name; }
	aabb3f getCollisionbox() const { return m_collisionbox; }

	u32 getFreeHudID()
	{
		size_t size = hud.size();
		for (size_t i = 0; i != size; i++) {
			if (!hud[i])
				return i;
		}
		return size;
	}

	bool camera_barely_in_ceiling;
	v3f eye_offset_first;
	v3f eye_offset_third;

	Inventory inventory;

	f32 movement_acceleration_default;
	f32 movement_acceleration_air;
	f32 movement_acceleration_fast;
	f32 movement_speed_walk;
	f32 movement_speed_crouch;
	f32 movement_speed_fast;
	f32 movement_speed_climb;
	f32 movement_speed_jump;
	f32 movement_liquid_fluidity;
	f32 movement_liquid_fluidity_smooth;
	f32 movement_liquid_sink;
	f32 movement_gravity;
	f32 movement_fall_aerodynamics;

	v2s32 local_animations[4];
	float local_animation_speed;

	std::atomic_ushort hp;

	std::atomic_short peer_id;

	std::string inventory_formspec;

	PlayerControl control;
	Mutex control_mutex;
	const PlayerControl& getPlayerControl() {
		std::lock_guard<Mutex> lock(control_mutex);
		return control;
	}

	u32 keyPressed;

	HudElement* getHud(u32 id);
	u32         addHud(HudElement* hud);
	HudElement* removeHud(u32 id);
	void        clearHud();

	u32 hud_flags;
	s32 hud_hotbar_itemcount;

	std::string hotbar_image;
	int hotbar_image_items;
	std::string hotbar_selected_image;

public:
	std::string m_name;
protected:
	u16 m_breath;
	f32 m_pitch;
	f32 m_yaw;
	v3f m_speed;
public:
	v3f m_position;
protected:
	aabb3f m_collisionbox;

	std::vector<HudElement *> hud;
private:
	// Protect some critical areas
	// hud for example can be modified by EmergeThread
	// and ServerThread
	Mutex m_mutex;
};


Json::Value operator<<(Json::Value &json, Player &player);
Json::Value operator>>(Json::Value &json, Player &player);

#endif

