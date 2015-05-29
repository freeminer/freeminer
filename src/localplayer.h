/*
localplayer.h
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

#ifndef LOCALPLAYER_HEADER
#define LOCALPLAYER_HEADER

#include "player.h"
#include "environment.h"
#include <list>

class Client;
class Environment;
class ClientEnvironment;
struct MapNode;
class GenericCAO;
class ClientActiveObject;
class IGameDef;

enum LocalPlayerAnimations {NO_ANIM, WALK_ANIM, DIG_ANIM, WD_ANIM};  // no local animation, walking, digging, both

class LocalPlayer : public Player
{
public:
	LocalPlayer(Client *gamedef, const char *name);
	virtual ~LocalPlayer();

	ClientActiveObject *parent;

	u16 hp;
	bool got_teleported;
	bool isAttached;
	bool touching_ground;
	// This oscillates so that the player jumps a bit above the surface
	bool in_liquid;
	// This is more stable and defines the maximum speed of the player
	bool in_liquid_stable;
	// Gets the viscosity of water to calculate friction
	float liquid_viscosity;
	bool is_climbing;
	bool swimming_vertical;

	float physics_override_speed;
	float physics_override_jump;
	float physics_override_gravity;
	bool physics_override_sneak;
	bool physics_override_sneak_glitch;

	v3f overridePosition;

	void move(f32 dtime, Environment *env, f32 pos_max_d);
	void move(f32 dtime, Environment *env, f32 pos_max_d,
			std::vector<CollisionInfo> *collision_info);
	bool canPlaceNode(const v3s16& p, const MapNode& node);

	void applyControl(float dtime, ClientEnvironment *env);

	v3s16 getStandingNodePos();

	// Used to check if anything changed and prevent sending packets if not
	v3f last_position;
	v3f last_speed;
	float last_pitch;
	float last_yaw;
	unsigned int last_keyPressed;
	u8 last_camera_fov;
	u8 last_wanted_range;

	float camera_impact;

	int last_animation;
	float last_animation_speed;

	/*
	std::string hotbar_image;
	int hotbar_image_items;
	std::string hotbar_selected_image;
	*/

	video::SColor light_color;

	float hurt_tilt_timer;
	float hurt_tilt_strength;

	GenericCAO* getCAO() const {
		return m_cao;
	}

	void setCAO(GenericCAO* toset) {
		assert( m_cao == NULL ); // Pre-condition
		m_cao = toset;
	}

	u32 maxHudId() const { return hud.size(); }

//freeminer:
	bool zoom = false;
	bool superspeed = false;
	bool free_move = false;

	//void addSpeed(v3f speed);
//=========

	u16 getBreath() { auto lock = lock_shared_rec(); return m_breath; }
	void setBreath(u16 breath) { auto lock = lock_unique_rec(); m_breath = breath; }

	v3s16 getLightPosition() const
	{
		return floatToInt(m_position + v3f(0,BS+BS/2,0), BS);
	}

	void setYaw(f32 yaw)
	{
		auto lock = lock_unique_rec();
		m_yaw = yaw;
	}

	f32 getYaw() { auto lock = lock_shared_rec(); return m_yaw; }

	void setPitch(f32 pitch)
	{
		auto lock = lock_unique_rec();
		m_pitch = pitch;
	}

	f32 getPitch() { auto lock = lock_shared_rec(); return m_pitch; }

	void setPosition(const v3f &position)
	{
		auto lock = lock_unique_rec();
		m_position = position;
	}

	v3f getPosition() { auto lock = lock_shared_rec(); return m_position; }
	v3f getEyePosition() { auto lock = lock_shared_rec(); return m_position + getEyeOffset(); }
	v3f getEyeOffset() const
	{
		float eye_height = camera_barely_in_ceiling ? 1.5f : 1.625f;
		return v3f(0, BS * eye_height, 0);
	}

	void setCollisionbox(const aabb3f &box) { m_collisionbox = box; }

private:
	void accelerateHorizontal(const v3f &target_speed, const f32 max_increase, float slippery = 0);
	void accelerateVertical(const v3f &target_speed, const f32 max_increase);

	v3f m_position;
	// This is used for determining the sneaking range
	v3s16 m_sneak_node;
	// Whether the player is allowed to sneak
public:
	bool m_sneak_node_exists;
	// Whether recalculation of the sneak node is needed
private:
	bool m_need_to_get_new_sneak_node;
	// Stores the max player uplift by m_sneak_node and is updated
	// when m_need_to_get_new_sneak_node == true
	f32 m_sneak_node_bb_ymax;
	// Node below player, used to determine whether it has been removed,
	// and its old type
	v3s16 m_old_node_below;
	std::string m_old_node_below_type;
	bool m_can_jump;
	u16 m_breath;
	f32 m_yaw;
	f32 m_pitch;
	bool camera_barely_in_ceiling;
	aabb3f m_collisionbox;

	GenericCAO* m_cao;
	Client *m_gamedef;
};

#endif

