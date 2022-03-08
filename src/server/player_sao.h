/*
content_sao.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
<<<<<<< HEAD:src/content_sao.h
*/
=======
Copyright (C) 2013-2020 Minetest core developers & community
>>>>>>> 5.5.0:src/server/player_sao.h

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

#pragma once

#include "constants.h"
<<<<<<< HEAD:src/content_sao.h

class UnitSAO: public ServerActiveObject
{
public:
	UnitSAO(ServerEnvironment *env, v3f pos):
			ServerActiveObject(env, pos),
			m_hp(-1), m_yaw(0) {}
	virtual ~UnitSAO() {}

	virtual void setYaw(const float yaw) { m_yaw = yaw; }
	float getYaw() const { return m_yaw; };
	f32 getRadYaw() const { return m_yaw * core::DEGTORAD; }
	// Deprecated
	f32 getRadYawDep() const { return (m_yaw + 90.) * core::DEGTORAD; }

	s16 getHP() const { return m_hp; }
	// Use a function, if isDead can be defined by other conditions
	bool isDead() const { return m_hp == 0; }
protected:
	std::atomic_ushort m_hp;
	float m_yaw;
};

/*
	LuaEntitySAO needs some internals exposed.
*/

class LuaEntitySAO : public UnitSAO
{
public:
	LuaEntitySAO(ServerEnvironment *env, v3f pos,
	             const std::string &name, const std::string &state);
	~LuaEntitySAO();
	ActiveObjectType getType() const
	{ return ACTIVEOBJECT_TYPE_LUAENTITY; }
	ActiveObjectType getSendType() const
	{ return ACTIVEOBJECT_TYPE_GENERIC; }
	virtual void addedToEnvironment(u32 dtime_s);
	static ServerActiveObject* create(ServerEnvironment *env, v3f pos,
			const std::string &data);
	bool isAttached();
	void step(float dtime, bool send_recommended);
	std::string getClientInitializationData(u16 protocol_version);
	std::string getStaticData();
	int punch(v3f dir,
			const ToolCapabilities *toolcap=NULL,
			ServerActiveObject *puncher=NULL,
			float time_from_last_punch=1000000);
	void rightClick(ServerActiveObject *clicker);
	void setPos(const v3f &pos);
	void moveTo(v3f pos, bool continuous);
	float getMinimumSavedMovement();
	std::string getDescription();
	void setHP(s16 hp);
	s16 getHP() const;
	void setArmorGroups(const ItemGroupList &armor_groups);
	ItemGroupList getArmorGroups();
	void setAnimation(v2f frame_range, float frame_speed, float frame_blend, bool frame_loop);
	void getAnimation(v2f *frame_range, float *frame_speed, float *frame_blend, bool *frame_loop);
	void setBonePosition(const std::string &bone, v3f position, v3f rotation);
	void getBonePosition(const std::string &bone, v3f *position, v3f *rotation);
	void setAttachment(int parent_id, const std::string &bone, v3f position, v3f rotation);
	void getAttachment(int *parent_id, std::string *bone, v3f *position, v3f *rotation);
	void addAttachmentChild(int child_id);
	void removeAttachmentChild(int child_id);
	UNORDERED_SET<int> getAttachmentChildIds();
	ObjectProperties* accessObjectProperties();
	void notifyObjectPropertiesModified();
	/* LuaEntitySAO-specific */
	void setVelocity(v3f velocity);
	v3f getVelocity();
	void setAcceleration(v3f acceleration);
	v3f getAcceleration();

	void setTextureMod(const std::string &mod);
	void setSprite(v2s16 p, int num_frames, float framelength,
			bool select_horiz_by_yawpitch);
	std::string getName();
	bool getCollisionBox(aabb3f *toset);
	bool collideWithObjects();
protected:
	std::string getPropertyPacket();
	void sendPosition(bool do_interpolate, bool is_movement_end);

	std::string m_init_name;
	std::string m_init_state;
	bool m_registered;

public:
	struct ObjectProperties m_prop;

	//std::atomic_ushort m_hp;
	v3f m_velocity;
	v3f m_acceleration;

private:
	ItemGroupList m_armor_groups;

	std::atomic_bool m_properties_sent;
	float m_last_sent_yaw;
	v3f m_last_sent_position;
	v3f m_last_sent_velocity;
	float m_last_sent_position_timer;
	float m_last_sent_move_precision;
	bool m_armor_groups_sent;

	v2f m_animation_range;
	float m_animation_speed;
	float m_animation_blend;
	bool m_animation_loop;
	bool m_animation_sent;

	UNORDERED_MAP<std::string, core::vector2d<v3f> > m_bone_position;
	bool m_bone_position_sent;

	int m_attachment_parent_id;
	UNORDERED_SET<int> m_attachment_child_ids;
	std::string m_attachment_bone;
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attachment_sent;
};
=======
#include "network/networkprotocol.h"
#include "unit_sao.h"
#include "util/numeric.h"
>>>>>>> 5.5.0:src/server/player_sao.h

/*
	PlayerSAO needs some internals exposed.
*/

class LagPool
{
	float m_pool = 15.0f;
	float m_max = 15.0f;

public:
	LagPool() = default;

	void setMax(float new_max)
	{
		m_max = new_max;
		if (m_pool > new_max)
			m_pool = new_max;
	}

	void add(float dtime)
	{
		m_pool -= dtime;
		if (m_pool < 0)
			m_pool = 0;
	}

	void empty() { m_pool = m_max; }

	bool grab(float dtime)
	{
		if (dtime <= 0)
			return true;
		if (m_pool + dtime > m_max)
			return false;
		m_pool += dtime;
		return true;
	}
};

typedef UNORDERED_MAP<std::string, std::string> PlayerAttributes;
class RemotePlayer;

class PlayerSAO : public UnitSAO
{
public:
	PlayerSAO(ServerEnvironment *env_, RemotePlayer *player_, session_t peer_id_,
			bool is_singleplayer);

	ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_PLAYER; }
	ActiveObjectType getSendType() const { return ACTIVEOBJECT_TYPE_GENERIC; }
	std::string getDescription();

	/*
		Active object <-> environment interface
	*/

	void addedToEnvironment(u32 dtime_s);
	void removingFromEnvironment();
	bool isStaticAllowed() const { return false; }
	bool shouldUnload() const { return false; }
	std::string getClientInitializationData(u16 protocol_version);
	void getStaticData(std::string *result) const;
	void step(float dtime, bool send_recommended);
	void setBasePosition(const v3f &position);
	void setPos(const v3f &pos);
	void moveTo(v3f pos, bool continuous);
<<<<<<< HEAD:src/content_sao.h
	void addSpeed(v3f);
	void setYaw(const float yaw);
=======
	void setPlayerYaw(const float yaw);
>>>>>>> 5.5.0:src/server/player_sao.h
	// Data should not be sent at player initialization
	void setPlayerYawAndSend(const float yaw);
	void setLookPitch(const float pitch);
	// Data should not be sent at player initialization
	void setLookPitchAndSend(const float pitch);
	f32 getLookPitch() const { return m_pitch; }
	f32 getRadLookPitch() const { return m_pitch * core::DEGTORAD; }
	// Deprecated
	f32 getRadLookPitchDep() const { return -1.0 * m_pitch * core::DEGTORAD; }
	void setFov(const float pitch);
	f32 getFov() const { return m_fov; }
	void setWantedRange(const s16 range);
	s16 getWantedRange() const { return m_wanted_range; }

	/*
		Interaction interface
	*/

	u32 punch(v3f dir, const ToolCapabilities *toolcap, ServerActiveObject *puncher,
			float time_from_last_punch, u16 initial_wear = 0);
	void rightClick(ServerActiveObject *clicker);
	void setHP(s32 hp, const PlayerHPChangeReason &reason) override
	{
		return setHP(hp, reason, false);
	}
	void setHP(s32 hp, const PlayerHPChangeReason &reason, bool from_client);
	void setHPRaw(u16 hp) { m_hp = hp; }
	u16 getBreath() const { return m_breath; }
	void setBreath(const u16 breath, bool send = true);

	/*
		Inventory interface
	*/
	Inventory *getInventory() const;
	InventoryLocation getInventoryLocation() const;
	void setInventoryModified() {}
	std::string getWieldList() const { return "main"; }
	u16 getWieldIndex() const;
	ItemStack getWieldedItem(ItemStack *selected, ItemStack *hand = nullptr) const;
	bool setWieldedItem(const ItemStack &item);

	/*
		Modding interface
	*/
	inline void setExtendedAttribute(const std::string &attr, const std::string &value)
	{
		m_extra_attributes[attr] = value;
		m_extended_attributes_modified = true;
	}

	inline bool getExtendedAttribute(const std::string &attr, std::string *value)
	{
		if (m_extra_attributes.find(attr) == m_extra_attributes.end())
			return false;

		*value = m_extra_attributes[attr];
		return true;
	}

	inline const PlayerAttributes &getExtendedAttributes()
	{
		return m_extra_attributes;
	}

	inline bool extendedAttributesModified() const
	{
		return m_extended_attributes_modified;
	}

	inline void setExtendedAttributeModified(bool v)
	{
		m_extended_attributes_modified = v;
	}

	/*
		PlayerSAO-specific
	*/

	void disconnected();

	RemotePlayer *getPlayer() { return m_player; }
	session_t getPeerID() const { return m_peer_id; }

	// Cheat prevention

	v3f getLastGoodPosition() const { return m_last_good_position; }
	float resetTimeFromLastPunch()
	{
		auto lock = lock_unique_rec();
		float r = m_time_from_last_punch;
		m_time_from_last_punch = 0.0;
		return r;
	}
	void noCheatDigStart(const v3s16 &p)
	{
		auto lock = lock_unique_rec();
		m_nocheat_dig_pos = p;
		m_nocheat_dig_time = 0;
	}
	v3s16 getNoCheatDigPos() { return m_nocheat_dig_pos; }
	float getNoCheatDigTime() { return m_nocheat_dig_time; }
	void noCheatDigEnd() { m_nocheat_dig_pos = v3s16(32767, 32767, 32767); }
	LagPool &getDigPool() { return m_dig_pool; }
	void setMaxSpeedOverride(const v3f &vel);
	// Returns true if cheated
	bool checkMovementCheat();

	// Other

	void updatePrivileges(const std::set<std::string> &privs, bool is_singleplayer)
	{
		m_privs = privs;
		m_is_singleplayer = is_singleplayer;
	}

	bool getCollisionBox(aabb3f *toset) const;
	bool getSelectionBox(aabb3f *toset) const;
	bool collideWithObjects() const { return true; }

	void finalize(RemotePlayer *player, const std::set<std::string> &privs);

	v3f getEyePosition() const { return m_base_position + getEyeOffset(); }
	v3f getEyeOffset() const;
	float getZoomFOV() const;

	inline Metadata &getMeta() { return m_meta; }

private:
	std::string getPropertyPacket();
	void unlinkPlayerSessionAndSave();
	std::string generateUpdatePhysicsOverrideCommand() const;

	RemotePlayer *m_player = nullptr;
	session_t m_peer_id = 0;

	// Cheat prevention
	LagPool m_dig_pool;
	LagPool m_move_pool;
public:
	v3f m_last_good_position;
<<<<<<< HEAD:src/content_sao.h
	std::atomic_uint m_ms_from_last_respawn;
private:
	float m_time_from_last_punch;
	v3s16 m_nocheat_dig_pos;
	float m_nocheat_dig_time;

	int m_wield_index;
	std::atomic_bool m_position_not_sent;
	ItemGroupList m_armor_groups;
	std::atomic_bool m_armor_groups_sent;

	std::atomic_bool m_properties_sent;
	struct ObjectProperties m_prop;
=======
	float m_time_from_last_teleport = 0.0f;
	float m_time_from_last_punch = 0.0f;
	v3s16 m_nocheat_dig_pos = v3s16(32767, 32767, 32767);
	float m_nocheat_dig_time = 0.0f;
	float m_max_speed_override_time = 0.0f;
	v3f m_max_speed_override = v3f(0.0f, 0.0f, 0.0f);

	// Timers
	IntervalLimiter m_breathing_interval;
	IntervalLimiter m_drowning_interval;
	IntervalLimiter m_node_hurt_interval;

	bool m_position_not_sent = false;

>>>>>>> 5.5.0:src/server/player_sao.h
	// Cached privileges for enforcement
	std::set<std::string> m_privs;
	bool m_is_singleplayer;

<<<<<<< HEAD:src/content_sao.h
	v2f m_animation_range;
	float m_animation_speed;
	float m_animation_blend;
	bool m_animation_loop;
	std::atomic_bool m_animation_sent;
=======
	u16 m_breath = PLAYER_MAX_BREATH_DEFAULT;
	f32 m_pitch = 0.0f;
	f32 m_fov = 0.0f;
	s16 m_wanted_range = 0.0f;
>>>>>>> 5.5.0:src/server/player_sao.h

	Metadata m_meta;

<<<<<<< HEAD:src/content_sao.h
	int m_attachment_parent_id;
	UNORDERED_SET<int> m_attachment_child_ids;
	std::string m_attachment_bone;
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attachment_sent;
	u16 m_breath;
	f32 m_pitch;
	f32 m_fov;
	s16 m_wanted_range;

	PlayerAttributes m_extra_attributes;
	bool m_extended_attributes_modified;
public:
	float m_physics_override_speed;
	float m_physics_override_jump;
	float m_physics_override_gravity;
	bool m_physics_override_sneak;
	bool m_physics_override_sneak_glitch;
	std::atomic_bool m_physics_override_sent;
=======
public:
	float m_physics_override_speed = 1.0f;
	float m_physics_override_jump = 1.0f;
	float m_physics_override_gravity = 1.0f;
	bool m_physics_override_sneak = true;
	bool m_physics_override_sneak_glitch = false;
	bool m_physics_override_new_move = true;
	bool m_physics_override_sent = false;
>>>>>>> 5.5.0:src/server/player_sao.h
};

struct PlayerHPChangeReason
{
	enum Type : u8
	{
		SET_HP,
		PLAYER_PUNCH,
		FALL,
		NODE_DAMAGE,
		DROWNING,
		RESPAWN
	};

	Type type = SET_HP;
	bool from_mod = false;
	int lua_reference = -1;

	// For PLAYER_PUNCH
	ServerActiveObject *object = nullptr;
	// For NODE_DAMAGE
	std::string node;

	inline bool hasLuaReference() const { return lua_reference >= 0; }

	bool setTypeFromString(const std::string &typestr)
	{
		if (typestr == "set_hp")
			type = SET_HP;
		else if (typestr == "punch")
			type = PLAYER_PUNCH;
		else if (typestr == "fall")
			type = FALL;
		else if (typestr == "node_damage")
			type = NODE_DAMAGE;
		else if (typestr == "drown")
			type = DROWNING;
		else if (typestr == "respawn")
			type = RESPAWN;
		else
			return false;

		return true;
	}

	std::string getTypeAsString() const
	{
		switch (type) {
		case PlayerHPChangeReason::SET_HP:
			return "set_hp";
		case PlayerHPChangeReason::PLAYER_PUNCH:
			return "punch";
		case PlayerHPChangeReason::FALL:
			return "fall";
		case PlayerHPChangeReason::NODE_DAMAGE:
			return "node_damage";
		case PlayerHPChangeReason::DROWNING:
			return "drown";
		case PlayerHPChangeReason::RESPAWN:
			return "respawn";
		default:
			return "?";
		}
	}

	PlayerHPChangeReason(Type type) : type(type) {}

	PlayerHPChangeReason(Type type, ServerActiveObject *object) :
			type(type), object(object)
	{
	}

	PlayerHPChangeReason(Type type, std::string node) : type(type), node(node) {}
};
