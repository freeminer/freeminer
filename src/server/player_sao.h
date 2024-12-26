// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2020 Minetest core developers & community

#pragma once

#include <atomic>
#include <set>
#include "constants.h"
#include "irr_v3d.h"
#include "metadata.h"
#include "network/networkprotocol.h"
#include "unit_sao.h"
#include "util/numeric.h"

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

class RemotePlayer;

class PlayerSAO : public UnitSAO
{
public:
	PlayerSAO(ServerEnvironment *env_, RemotePlayer *player_, session_t peer_id_,
			bool is_singleplayer);

	~PlayerSAO();
	ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_PLAYER; }
	ActiveObjectType getSendType() const override { return ACTIVEOBJECT_TYPE_GENERIC; }
	std::string getDescription() override;

	/*
		Active object <-> environment interface
	*/

// fm:
	void addSpeed(v3f);
	std::atomic_uint m_ms_from_last_respawn {10000}; //more than ignore move time (1)
	v3opos_t m_last_stat_position {};
	double last_time_online = 0;
// ==

	void addedToEnvironment(u32 dtime_s) override;
	void removingFromEnvironment() override;
	bool isStaticAllowed() const override { return false; }
	bool shouldUnload() const override { return false; }
	std::string getClientInitializationData(u16 protocol_version) override;
	void getStaticData(std::string *result) const override;
	void step(float dtime, bool send_recommended) override;
	void setBasePosition(v3opos_t position);
	void setPos(const v3opos_t &pos) override;
	void addPos(const v3opos_t &added_pos) override;
	void moveTo(v3opos_t pos, bool continuous) override;
	void setPlayerYaw(const float yaw);
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
	void setCameraInverted(bool camera_inverted) { m_camera_inverted = camera_inverted; }
	bool getCameraInverted() const { return m_camera_inverted; }

	/*
		Interaction interface
	*/

	u32 punch(v3f dir, const ToolCapabilities *toolcap, ServerActiveObject *puncher,
			float time_from_last_punch, u16 initial_wear = 0) override;
	void rightClick(ServerActiveObject *clicker) override;
	void setHP(s32 hp, const PlayerHPChangeReason &reason) override
	{
		return setHP(hp, reason, false);
	}
	void setHP(s32 hp, const PlayerHPChangeReason &reason, bool from_client);
	void setHPRaw(u16 hp) { m_hp = hp; }
	u16 getBreath() const { return m_breath; }
	void setBreath(const u16 breath, bool send = true);
	void respawn();

	/*
		Inventory interface
	*/
	Inventory *getInventory() const override;
	InventoryLocation getInventoryLocation() const override;
	void setInventoryModified() override {}
	std::string getWieldList() const override { return "main"; }
	u16 getWieldIndex() const override;
	ItemStack getWieldedItem(ItemStack *selected, ItemStack *hand = nullptr) const override;
	bool setWieldedItem(const ItemStack &item) override;

	/*
		PlayerSAO-specific
	*/

	void disconnected();

	void setPlayer(RemotePlayer *player) { m_player = player; }
	RemotePlayer *getPlayer() { return m_player; }
	session_t getPeerID() const;

	// Cheat prevention

	v3opos_t getLastGoodPosition() const { return m_last_good_position; }
	float resetTimeFromLastPunch()
	{
		const auto lock = lock_unique_rec();
		float r = m_time_from_last_punch;
		m_time_from_last_punch = 0.0;
		return r;
	}
	void noCheatDigStart(const v3pos_t &p)
	{
		const auto lock = lock_unique_rec();
		m_nocheat_dig_pos = p;
		m_nocheat_dig_time = 0;
	}
	v3pos_t getNoCheatDigPos() { return m_nocheat_dig_pos; }
	float getNoCheatDigTime() { return m_nocheat_dig_time; }
	void noCheatDigEnd() { m_nocheat_dig_pos = v3pos_t(32767, 32767, 32767); }
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

	bool getCollisionBox(aabb3o *toset) const override;
	bool getSelectionBox(aabb3f *toset) const override;
	bool collideWithObjects() const override { return true; }

	void finalize(RemotePlayer *player, const std::set<std::string> &privs);

	v3opos_t getEyePosition() const { return getBasePosition() + v3fToOpos(getEyeOffset()); }
	v3f getEyeOffset() const;
	float getZoomFOV() const;

	inline SimpleMetadata &getMeta() { return m_meta; }

private:
	std::string getPropertyPacket();
	void unlinkPlayerSessionAndSave();
	std::string generateUpdatePhysicsOverrideCommand() const;

	RemotePlayer *m_player = nullptr;
	session_t m_peer_id_initial = 0; ///< only used to initialize RemotePlayer

	// Cheat prevention
	LagPool m_dig_pool;
	LagPool m_move_pool;
public:
	v3opos_t m_last_good_position;
	float m_time_from_last_teleport = 0.0f;
	float m_time_from_last_punch = 0.0f;
	v3pos_t m_nocheat_dig_pos = v3pos_t(32767, 32767, 32767);
	float m_nocheat_dig_time = 0.0f;
	float m_max_speed_override_time = 0.0f;
	v3f m_max_speed_override = v3f(0.0f, 0.0f, 0.0f);

	// Timers
	IntervalLimiter m_breathing_interval;
	IntervalLimiter m_drowning_interval;
	IntervalLimiter m_node_hurt_interval;

	std::atomic_bool m_position_not_sent {false};

	// Cached privileges for enforcement
	std::set<std::string> m_privs;
	bool m_is_singleplayer;

	std::atomic_uint16_t m_breath = PLAYER_MAX_BREATH_DEFAULT;
	std::atomic<f32> m_pitch {0.0f};
	std::atomic<f32> m_fov {0.0f};
	std::atomic_short m_wanted_range {0};

	bool m_camera_inverted = false; // this is not store in the player db

	SimpleMetadata m_meta;

public:
	struct {
		bool breathing : 1;
		bool drowning : 1;
		bool node_damage : 1;
	} m_flags = {true, true, true};

	std::atomic_bool m_physics_override_sent {false};
};

struct PlayerHPChangeReason
{
	enum Type : u8
	{
		SET_HP,
		SET_HP_MAX, // internal type to allow distinguishing hp reset and damage (for effects)
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
	v3pos_t node_pos;

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
		case PlayerHPChangeReason::SET_HP_MAX:
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

	PlayerHPChangeReason(Type type, std::string node, v3pos_t node_pos) : type(type), node(node), node_pos(node_pos) {}
};
