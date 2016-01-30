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
#include "util/lock.h"
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
	}
	PlayerControl(
		bool a_up,
		bool a_down,
		bool a_left,
		bool a_right,
		bool a_jump,
		bool a_aux1,
		bool a_sneak,
		bool a_LMB,
		bool a_RMB,
		float a_pitch,
		float a_yaw
	)
	{
		up = a_up;
		down = a_down;
		left = a_left;
		right = a_right;
		jump = a_jump;
		aux1 = a_aux1;
		sneak = a_sneak;
		LMB = a_LMB;
		RMB = a_RMB;
		pitch = a_pitch;
		yaw = a_yaw;
	}
	bool up;
	bool down;
	bool left;
	bool right;
	bool jump;
	bool aux1;
	bool sneak;
	bool LMB;
	bool RMB;
	float pitch;
	float yaw;
};

class Map;
class IGameDef;
struct CollisionInfo;
class PlayerSAO;
struct HudElement;
class Environment;

// IMPORTANT:
// Do *not* perform an assignment or copy operation on a Player or
// RemotePlayer object!  This will copy the lock held for HUD synchronization
class Player
: public locker<>
{
public:

	Player(IGameDef *gamedef, const std::string & name);
	virtual ~Player() = 0;

	virtual void move(f32 dtime, Environment *env, f32 pos_max_d)
	{}
	virtual void move(f32 dtime, Environment *env, f32 pos_max_d,
			std::vector<CollisionInfo> *collision_info)
	{}

	v3f getSpeed()
	{
		auto lock = lock_shared();
		return m_speed;
	}

	void setSpeed(v3f speed)
	{
		auto lock = lock_unique();
		m_speed = speed;
	}

	void accelerateHorizontal(v3f target_speed, f32 max_increase, float slippery=0);
	void accelerateVertical(v3f target_speed, f32 max_increase);

	v3f getPosition()
	{
		auto lock = lock_shared();
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
		auto lock = lock_shared();
		return m_position + getEyeOffset();
	}

	virtual void setPosition(const v3f &position)
	{
		auto lock = lock_unique();
		m_position = position;
	}

	void setPitch(f32 pitch)
	{
		auto lock = lock_unique();
		m_pitch = pitch;
	}

	virtual void setYaw(f32 yaw)
	{
		auto lock = lock_unique();
		m_yaw = yaw;
	}

	f32 getPitch()
	{
		auto lock = lock_shared();
		return m_pitch;
	}

	f32 getYaw()
	{
		auto lock = lock_shared();
		return m_yaw;
	}

	u16 getBreath()
	{
		auto lock = lock_shared();
		return m_breath;
	}

	virtual void setBreath(u16 breath)
	{
		auto lock = lock_unique();
		m_breath = breath;
	}

	f32 getRadPitch()
	{
		return -1.0 * m_pitch * core::DEGTORAD;
	}

	f32 getRadYaw()
	{
		return (m_yaw + 90.) * core::DEGTORAD;
	}

	void updateName(const std::string &name)
	{
		m_name = name;
	}

	const std::string & getName() const
	{
		return m_name;
	}

	core::aabbox3d<f32> getCollisionbox()
	{
		return m_collisionbox;
	}

	u32 getFreeHudID() {
		size_t size = hud.size();
		for (size_t i = 0; i != size; i++) {
			if (!hud[i])
				return i;
		}
		return size;
	}

	void setHotbarItemcount(s32 hotbar_itemcount)
	{
		hud_hotbar_itemcount = hotbar_itemcount;
	}

	s32 getHotbarItemcount()
	{
		return hud_hotbar_itemcount;
	}

	void setHotbarImage(const std::string &name)
	{
		hud_hotbar_image = name;
	}

	std::string getHotbarImage()
	{
		return hud_hotbar_image;
	}

	void setHotbarSelectedImage(const std::string &name)
	{
		hud_hotbar_selected_image = name;
	}

	std::string getHotbarSelectedImage() {
		return hud_hotbar_selected_image;
	}

	void setSky(const video::SColor &bgcolor, const std::string &type,
		const std::vector<std::string> &params)
	{
		m_sky_bgcolor = bgcolor;
		m_sky_type = type;
		m_sky_params = params;
	}

	void getSky(video::SColor *bgcolor, std::string *type,
		std::vector<std::string> *params)
	{
		*bgcolor = m_sky_bgcolor;
		*type = m_sky_type;
		*params = m_sky_params;
	}

	void overrideDayNightRatio(bool do_override, float ratio)
	{
		m_day_night_ratio_do_override = do_override;
		m_day_night_ratio = ratio;
	}

	void getDayNightRatio(bool *do_override, float *ratio)
	{
		*do_override = m_day_night_ratio_do_override;
		*ratio = m_day_night_ratio;
	}

	void setLocalAnimations(v2s32 frames[4], float frame_speed)
	{
		for (int i = 0; i < 4; i++)
			local_animations[i] = frames[i];
		local_animation_speed = frame_speed;
	}

	void getLocalAnimations(v2s32 *frames, float *frame_speed)
	{
		for (int i = 0; i < 4; i++)
			frames[i] = local_animations[i];
		*frame_speed = local_animation_speed;
	}

	virtual bool isLocal() const
	{
		return false;
	}

	virtual PlayerSAO *getPlayerSAO()
	{
		return NULL;
	}

	virtual void setPlayerSAO(PlayerSAO *sao)
	{
		/*
		FATAL_ERROR("FIXME");
		*/
	}

	/*
		serialize() writes a bunch of text that can contain
		any characters except a '\0', and such an ending that
		deSerialize stops reading exactly at the right point.
	*/
	void serialize(std::ostream &os);
	void deSerialize(std::istream &is, std::string playername);

	s16 refs;

	// Use a function, if isDead can be defined by other conditions
	bool isDead() { return hp == 0; }

	bool touching_ground;
	// This oscillates so that the player jumps a bit above the surface
	bool in_liquid;
	// This is more stable and defines the maximum speed of the player
	bool in_liquid_stable;
	// Gets the viscosity of water to calculate friction
	float liquid_viscosity;
	bool is_climbing;
	bool swimming_vertical;
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

	float physics_override_speed;
	float physics_override_jump;
	float physics_override_gravity;
	bool physics_override_sneak;
	bool physics_override_sneak_glitch;

	v2s32 local_animations[4];
	float local_animation_speed;

	std::atomic_ushort hp;

	float hurt_tilt_timer;
	float hurt_tilt_strength;

	bool zoom;
	bool superspeed;
	bool free_move;

	u16 protocol_version;
	std::atomic_short peer_id;

	std::string inventory_formspec;

	PlayerControl control;
	std::mutex control_mutex;
	PlayerControl getPlayerControl()
	{
		std::lock_guard<std::mutex> lock(control_mutex);
		return control;
	}

	u32 keyPressed;


	HudElement* getHud(u32 id);
	u32         addHud(HudElement* hud);
	HudElement* removeHud(u32 id);
	void        clearHud();
	u32         maxHudId() {
		return hud.size();
	}

	u32 hud_flags;
	s32 hud_hotbar_itemcount;
	std::string hud_hotbar_image;
	std::string hud_hotbar_selected_image;
protected:
	IGameDef *m_gamedef;

public:
	std::string m_name;
	u16 m_breath;
	f32 m_pitch;
	f32 m_yaw;
	v3f m_speed;
	v3f m_position;
	core::aabbox3d<f32> m_collisionbox;

	std::vector<HudElement *> hud;

	std::string m_sky_type;
	video::SColor m_sky_bgcolor;
	std::vector<std::string> m_sky_params;

	bool m_day_night_ratio_do_override;
	float m_day_night_ratio;
private:
	// Protect some critical areas
	// hud for example can be modified by EmergeThread
	// and ServerThread
	Mutex m_mutex;
};


/*
	Player on the server
*/
class RemotePlayer : public Player
{
public:
	RemotePlayer(IGameDef *gamedef, const std::string & name);
	virtual ~RemotePlayer() {}

	PlayerSAO *getPlayerSAO()
	{ return m_sao; }
	void setPlayerSAO(PlayerSAO *sao)
	{ m_sao = sao; }
	void setPosition(const v3f &position);

private:
	PlayerSAO *m_sao;
};

Json::Value operator<<(Json::Value &json, Player &player);
Json::Value operator>>(Json::Value &json, Player &player);

#endif

