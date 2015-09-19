/*
content_sao.h
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

#ifndef CONTENT_SAO_HEADER
#define CONTENT_SAO_HEADER

#include "serverobject.h"
#include "itemgroup.h"
#include "player.h"
#include "object_properties.h"

/*
	LuaEntitySAO needs some internals exposed.
*/

class LuaEntitySAO : public ServerActiveObject
{
public:
	LuaEntitySAO(
	             const std::string &name, const std::string &state,
	             s16 hp = -1,
	             v3f velocity = v3f(0,0,0),
	             float yaw = 0.0);
	~LuaEntitySAO();
	virtual ActiveObjectType getSendType() const
	{ return ACTIVEOBJECT_TYPE_GENERIC; }
	virtual void addedToEnvironment(u32 dtime_s);
	bool isAttached();
	void step(float dtime, bool send_recommended);
	std::string getClientInitializationData(u16 protocol_version);
	std::string getStaticData();
	int punch(v3f dir,
			const ToolCapabilities *toolcap=NULL,
			ServerActiveObject *puncher=NULL,
			float time_from_last_punch=1000000);
	void rightClick(ServerActiveObject *clicker);
	void setPos(v3f pos);
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
	std::set<int> getAttachmentChildIds();
	ObjectProperties* accessObjectProperties();
	void notifyObjectPropertiesModified();
	/* LuaEntitySAO-specific */
	void setVelocity(v3f velocity);
	v3f getVelocity();
	void setAcceleration(v3f acceleration);
	v3f getAcceleration();
	void setYaw(float yaw);
	float getYaw();
	void setTextureMod(const std::string &mod);
	void setSprite(v2s16 p, int num_frames, float framelength,
			bool select_horiz_by_yawpitch);
	std::string getName();
	bool getCollisionBox(aabb3f *toset);
	bool collideWithObjects();
private:
	std::string getPropertyPacket();
	void sendPosition(bool do_interpolate, bool is_movement_end);

	std::string m_init_name;
	std::string m_init_state;
	bool m_registered;
	struct ObjectProperties m_prop;
	
	std::atomic_ushort m_hp;
	v3f m_velocity;
	v3f m_acceleration;
	float m_yaw;
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

	std::map<std::string, core::vector2d<v3f> > m_bone_position;
	bool m_bone_position_sent;

	int m_attachment_parent_id;
	std::set<int> m_attachment_child_ids;
	std::string m_attachment_bone;
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attachment_sent;
};

/*
	PlayerSAO needs some internals exposed.
*/

class LagPool
{
	float m_pool;
	float m_max;
public:
	LagPool(): m_pool(15), m_max(15)
	{}
	void setMax(float new_max)
	{
		m_max = new_max;
		if(m_pool > new_max)
			m_pool = new_max;
	}
	void add(float dtime)
	{
		m_pool -= dtime;
		if(m_pool < 0)
			m_pool = 0;
	}
	bool grab(float dtime)
	{
		if(dtime <= 0)
			return true;
		if(m_pool + dtime > m_max)
			return false;
		m_pool += dtime;
		return true;
	}
};

class PlayerSAO : public ServerActiveObject
{
public:
	PlayerSAO(ServerEnvironment *env_, Player *player_, u16 peer_id_,
			const std::set<std::string> &privs, bool is_singleplayer);
	~PlayerSAO();
	virtual ActiveObjectType getSendType() const
	{ return ACTIVEOBJECT_TYPE_GENERIC; }
	std::string getDescription();

	/*
		Active object <-> environment interface
	*/

	void addedToEnvironment(u32 dtime_s);
	void removingFromEnvironment();
	bool isStaticAllowed() const;
	std::string getClientInitializationData(u16 protocol_version);
	std::string getStaticData();
	bool isAttached();
	void step(float dtime, bool send_recommended);
	void setBasePosition(const v3f &position);
	void setPos(v3f pos);
	void moveTo(v3f pos, bool continuous);
	void setYaw(float);
	void setPitch(float);

	/*
		Interaction interface
	*/

	int punch(v3f dir,
		const ToolCapabilities *toolcap,
		ServerActiveObject *puncher,
		float time_from_last_punch);
	void rightClick(ServerActiveObject *clicker);
	s16 getHP() const;
	void setHP(s16 hp);
	s16 readDamage();
	u16 getBreath() const;
	void setBreath(u16 breath);
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
	std::set<int> getAttachmentChildIds();
	ObjectProperties* accessObjectProperties();
	void notifyObjectPropertiesModified();
	void setNametagColor(video::SColor color);
	video::SColor getNametagColor();

	/*
		Inventory interface
	*/

	Inventory* getInventory();
	const Inventory* getInventory() const;
	InventoryLocation getInventoryLocation() const;
	std::string getWieldList() const;
	int getWieldIndex() const;
	void setWieldIndex(int i);

	/*
		PlayerSAO-specific
	*/

	void disconnected();

	Player* getPlayer()
	{
		return m_player;
	}
	u16 getPeerID() const
	{
		return m_peer_id;
	}

	// Cheat prevention

	v3f getLastGoodPosition() const
	{
		return m_last_good_position;
	}
	float resetTimeFromLastPunch()
	{
		auto lock = lock_unique();
		float r = m_time_from_last_punch;
		m_time_from_last_punch = 0.0;
		return r;
	}
	void noCheatDigStart(v3s16 p)
	{
		auto lock = lock_unique();
		m_nocheat_dig_pos = p;
		m_nocheat_dig_time = 0;
	}
	v3s16 getNoCheatDigPos()
	{
		return m_nocheat_dig_pos;
	}
	float getNoCheatDigTime()
	{
		return m_nocheat_dig_time;
	}
	void noCheatDigEnd()
	{
		m_nocheat_dig_pos = v3s16(32767, 32767, 32767);
	}
	LagPool& getDigPool()
	{
		return m_dig_pool;
	}
	// Returns true if cheated
	bool checkMovementCheat();

	// Other

	void updatePrivileges(const std::set<std::string> &privs,
			bool is_singleplayer)
	{
		m_privs = privs;
		m_is_singleplayer = is_singleplayer;
	}

	bool getCollisionBox(aabb3f *toset);
	bool collideWithObjects();

private:
	std::string getPropertyPacket();
	
	Player *m_player;
	u16 m_peer_id;
	Inventory *m_inventory;
	s16 m_damage;

	// Cheat prevention
	LagPool m_dig_pool;
	LagPool m_move_pool;
public:
	v3f m_last_good_position;
	std::atomic_uint m_ms_from_last_respawn;
private:
	float m_time_from_last_punch;
	v3s16 m_nocheat_dig_pos;
	float m_nocheat_dig_time;

	int m_wield_index;
	std::atomic_bool m_position_not_sent;
	ItemGroupList m_armor_groups;
	bool m_armor_groups_sent;

	std::atomic_bool m_properties_sent;
	struct ObjectProperties m_prop;
	// Cached privileges for enforcement
	std::set<std::string> m_privs;
	bool m_is_singleplayer;

	v2f m_animation_range;
	float m_animation_speed;
	float m_animation_blend;
	bool m_animation_loop;
	bool m_animation_sent;

	std::map<std::string, core::vector2d<v3f> > m_bone_position; // Stores position and rotation for each bone name
	bool m_bone_position_sent;

	int m_attachment_parent_id;
	std::set<int> m_attachment_child_ids;
	std::string m_attachment_bone;
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attachment_sent;

	video::SColor m_nametag_color;
	bool m_nametag_sent;

public:
	float m_physics_override_speed;
	float m_physics_override_jump;
	float m_physics_override_gravity;
	bool m_physics_override_sneak;
	bool m_physics_override_sneak_glitch;
	std::atomic_bool m_physics_override_sent;
};

#endif

