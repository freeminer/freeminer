#pragma once

#include <vector>
#include <utility>
#include <string>
#include "../irrlichttypes.h"
#include "../msgpack_fix.h"
#include "../config.h"

#define CLIENT_PROTOCOL_VERSION_FM 2
#define SERVER_PROTOCOL_VERSION_FM 0

enum
{
	// u8 deployed version
	TOCLIENT_INIT_DEPLOYED,
	// u64 map seed
	TOCLIENT_INIT_SEED,
	// float recommended send interval (server step)
	TOCLIENT_INIT_STEP,
	// v3f player's position
	TOCLIENT_INIT_POS,
	// json map params
	TOCLIENT_INIT_MAP_PARAMS,
	TOCLIENT_INIT_PROTOCOL_VERSION_FM,
	TOCLIENT_INIT_WEATHER
};

enum
{
	TOCLIENT_BLOCKDATA_POS,
	TOCLIENT_BLOCKDATA_DATA,
	TOCLIENT_BLOCKDATA_HEAT,
	TOCLIENT_BLOCKDATA_HUMIDITY,
	TOCLIENT_BLOCKDATA_STEP,
	TOCLIENT_BLOCKDATA_CONTENT_ONLY,
	TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM1,
	TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM2
};

enum
{
	TOCLIENT_ADDNODE_POS,
	TOCLIENT_ADDNODE_NODE,
	TOCLIENT_ADDNODE_REMOVE_METADATA
};

enum
{
	TOCLIENT_REMOVENODE_POS
};

enum
{
	// string, serialized inventory
	TOCLIENT_INVENTORY_DATA
};

enum
{
	// u16 time (0-23999)
	TOCLIENT_TIME_OF_DAY_TIME,
	// f32 time_speed
	TOCLIENT_TIME_OF_DAY_TIME_SPEED
};

enum
{
	// string
	TOCLIENT_CHAT_MESSAGE_DATA
};

enum
{
	// list of ids
	TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_REMOVE,
	// list of [id, type, initialization_data]
	TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD_ADD
};

struct ActiveObjectAddData
{
	ActiveObjectAddData(u16 id_, u8 type_, std::string data_) :
			id(id_), type(type_), data(data_)
	{
	}
	ActiveObjectAddData() : id(0), type(0), data("") {}
	u16 id;
	u8 type;
	std::string data;
	MSGPACK_DEFINE(id, type, data);
};

enum
{
	// list of pair<id, message> where id is u16 and message is string
	TOCLIENT_ACTIVE_OBJECT_MESSAGES_MESSAGES
};

typedef std::vector<std::pair<unsigned int, std::string>> ActiveObjectMessages;

enum
{
	TOCLIENT_HP_HP
};

enum
{
	// v3f player position
	TOCLIENT_MOVE_PLAYER_POS,
	// f32 pitch
	TOCLIENT_MOVE_PLAYER_PITCH,
	// f32 yaw
	TOCLIENT_MOVE_PLAYER_YAW
	// TOCLIENT_MOVE_PLAYER_SPEED

};

enum
{
	// string
	TOCLIENT_ACCESS_DENIED_CUSTOM_STRING,
	// u16 command
	TOCLIENT_ACCESS_DENIED_REASON,
	TOCLIENT_ACCESS_DENIED_RECONNECT
};

#define TOCLIENT_PUNCH_PLAYER 0x11
enum
{
	// v3f player speed add
	TOCLIENT_PUNCH_PLAYER_SPEED,
};

enum
{
	// bool set camera point target
	TOCLIENT_DEATHSCREEN_SET_CAMERA,
	// v3f camera point target (to point the death cause or whatever)
	TOCLIENT_DEATHSCREEN_CAMERA_POINT
};

enum
{
	// vector<pair<name, data>>
	TOCLIENT_MEDIA_MEDIA
};

typedef std::vector<std::pair<std::string, std::string>> MediaData;

enum
{
	TOCLIENT_NODEDEF_DEFINITIONS,
	TOCLIENT_NODEDEF_DEFINITIONS_ZIP
};

enum
{
	// list of [string name, string sha1_digest]
	TOCLIENT_ANNOUNCE_MEDIA_LIST,
	// string, url of remote media server
	TOCLIENT_ANNOUNCE_MEDIA_REMOTE_SERVER
};

enum
{
	TOCLIENT_ITEMDEF_DEFINITIONS,
	TOCLIENT_ITEMDEF_DEFINITIONS_ZIP
};

typedef std::vector<std::pair<std::string, std::string>> MediaAnnounceList;

enum
{
	// s32
	TOCLIENT_PLAY_SOUND_ID,
	// string
	TOCLIENT_PLAY_SOUND_NAME,
	// f32
	TOCLIENT_PLAY_SOUND_GAIN,
	// u8
	TOCLIENT_PLAY_SOUND_TYPE,
	// v3f
	TOCLIENT_PLAY_SOUND_POS,
	// u16
	TOCLIENT_PLAY_SOUND_OBJECT_ID,
	// bool
	TOCLIENT_PLAY_SOUND_LOOP
};

enum
{
	// s32
	TOCLIENT_STOP_SOUND_ID
};

enum
{
	// list of strings
	TOCLIENT_PRIVILEGES_PRIVILEGES
};

enum
{
	// string
	TOCLIENT_INVENTORY_FORMSPEC_DATA
};

enum
{
	TOCLIENT_DETACHED_INVENTORY_NAME,
	TOCLIENT_DETACHED_INVENTORY_DATA
};

enum
{
	// string formspec
	TOCLIENT_SHOW_FORMSPEC_DATA,
	// string formname
	TOCLIENT_SHOW_FORMSPEC_NAME
};

// all values are floats here
enum
{
	TOCLIENT_MOVEMENT_ACCELERATION_DEFAULT,
	TOCLIENT_MOVEMENT_ACCELERATION_AIR,
	TOCLIENT_MOVEMENT_ACCELERATION_FAST,
	TOCLIENT_MOVEMENT_SPEED_WALK,
	TOCLIENT_MOVEMENT_SPEED_CROUCH,
	TOCLIENT_MOVEMENT_SPEED_FAST,
	TOCLIENT_MOVEMENT_SPEED_CLIMB,
	TOCLIENT_MOVEMENT_SPEED_JUMP,
	TOCLIENT_MOVEMENT_LIQUID_FLUIDITY,
	TOCLIENT_MOVEMENT_LIQUID_FLUIDITY_SMOOTH,
	TOCLIENT_MOVEMENT_LIQUID_SINK,
	TOCLIENT_MOVEMENT_GRAVITY,
	TOCLIENT_MOVEMENT_FALL_AERODYNAMICS
};

enum
{
	TOCLIENT_SPAWN_PARTICLE_POS,
	TOCLIENT_SPAWN_PARTICLE_VELOCITY,
	TOCLIENT_SPAWN_PARTICLE_ACCELERATION,
	TOCLIENT_SPAWN_PARTICLE_EXPIRATIONTIME,
	TOCLIENT_SPAWN_PARTICLE_SIZE,
	TOCLIENT_SPAWN_PARTICLE_COLLISIONDETECTION,
	TOCLIENT_SPAWN_PARTICLE_VERTICAL,
	TOCLIENT_SPAWN_PARTICLE_TEXTURE,
	TOCLIENT_SPAWN_PARTICLE_COLLISION_REMOVAL,
};

enum
{
	TOCLIENT_ADD_PARTICLESPAWNER_AMOUNT,
	TOCLIENT_ADD_PARTICLESPAWNER_SPAWNTIME,
	TOCLIENT_ADD_PARTICLESPAWNER_MINPOS,
	TOCLIENT_ADD_PARTICLESPAWNER_MAXPOS,
	TOCLIENT_ADD_PARTICLESPAWNER_MINVEL,
	TOCLIENT_ADD_PARTICLESPAWNER_MAXVEL,
	TOCLIENT_ADD_PARTICLESPAWNER_MINACC,
	TOCLIENT_ADD_PARTICLESPAWNER_MAXACC,
	TOCLIENT_ADD_PARTICLESPAWNER_MINEXPTIME,
	TOCLIENT_ADD_PARTICLESPAWNER_MAXEXPTIME,
	TOCLIENT_ADD_PARTICLESPAWNER_MINSIZE,
	TOCLIENT_ADD_PARTICLESPAWNER_MAXSIZE,
	TOCLIENT_ADD_PARTICLESPAWNER_COLLISIONDETECTION,
	TOCLIENT_ADD_PARTICLESPAWNER_VERTICAL,
	TOCLIENT_ADD_PARTICLESPAWNER_TEXTURE,
	TOCLIENT_ADD_PARTICLESPAWNER_ID,
	TOCLIENT_ADD_PARTICLESPAWNER_COLLISION_REMOVAL,
	TOCLIENT_ADD_PARTICLESPAWNER_ATTACHED_ID,
};

enum
{
	TOCLIENT_DELETE_PARTICLESPAWNER_ID
};

enum
{
	TOCLIENT_HUDADD_ID,
	TOCLIENT_HUDADD_TYPE,
	TOCLIENT_HUDADD_POS,
	TOCLIENT_HUDADD_NAME,
	TOCLIENT_HUDADD_SCALE,
	TOCLIENT_HUDADD_TEXT,
	TOCLIENT_HUDADD_NUMBER,
	TOCLIENT_HUDADD_ITEM,
	TOCLIENT_HUDADD_DIR,
	TOCLIENT_HUDADD_ALIGN,
	TOCLIENT_HUDADD_OFFSET,
	TOCLIENT_HUDADD_WORLD_POS,
	TOCLIENT_HUDADD_SIZE,
};

enum
{
	TOCLIENT_HUDRM_ID
};

enum
{
	TOCLIENT_HUDCHANGE_ID,
	TOCLIENT_HUDCHANGE_STAT,
	TOCLIENT_HUDCHANGE_V2F,
	TOCLIENT_HUDCHANGE_V3F,
	TOCLIENT_HUDCHANGE_STRING,
	TOCLIENT_HUDCHANGE_U32,
	TOCLIENT_HUDCHANGE_V2S32
};

enum
{
	TOCLIENT_HUD_SET_FLAGS_FLAGS,
	TOCLIENT_HUD_SET_FLAGS_MASK
};

enum
{
	TOCLIENT_HUD_SET_PARAM_ID,
	TOCLIENT_HUD_SET_PARAM_VALUE
};

enum
{
	// u16 breath
	TOCLIENT_BREATH_BREATH
};

enum
{
	TOCLIENT_SET_SKY_COLOR,
	TOCLIENT_SET_SKY_TYPE,
	TOCLIENT_SET_SKY_PARAMS
};

enum
{
	TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_DO,
	TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO_VALUE
};

enum
{
	TOCLIENT_LOCAL_PLAYER_ANIMATIONS_IDLE,
	TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALK,
	TOCLIENT_LOCAL_PLAYER_ANIMATIONS_DIG,
	TOCLIENT_LOCAL_PLAYER_ANIMATIONS_WALKDIG,
	TOCLIENT_LOCAL_PLAYER_ANIMATIONS_FRAME_SPEED
};

enum
{
	TOCLIENT_EYE_OFFSET_FIRST,
	TOCLIENT_EYE_OFFSET_THIRD
};

enum
{
	TOSERVER_INIT_FMT,
	TOSERVER_INIT_COMPRESSION,
	TOSERVER_INIT_PROTOCOL_VERSION_MIN,
	TOSERVER_INIT_PROTOCOL_VERSION_MAX,
	TOSERVER_INIT_NAME
};

enum
{
	// u8 SER_FMT_VER_HIGHEST_READ
	TOSERVER_INIT_LEGACY_FMT,
	TOSERVER_INIT_LEGACY_NAME,
	TOSERVER_INIT_LEGACY_PASSWORD,
	TOSERVER_INIT_LEGACY_PROTOCOL_VERSION_MIN,
	TOSERVER_INIT_LEGACY_PROTOCOL_VERSION_MAX,
	TOSERVER_INIT_LEGACY_PROTOCOL_VERSION_FM
};

enum
{
	// v3f
	TOSERVER_PLAYERPOS_POSITION,
	// v3f
	TOSERVER_PLAYERPOS_SPEED,
	// f32
	TOSERVER_PLAYERPOS_PITCH,
	// f32
	TOSERVER_PLAYERPOS_YAW,
	// u32
	TOSERVER_PLAYERPOS_KEY_PRESSED
};

enum
{
	TOSERVER_DELETEDBLOCKS_DATA
};

enum
{
	TOSERVER_INVENTORY_ACTION_DATA
};

enum
{
	TOSERVER_CHAT_MESSAGE_DATA
};

enum
{
	TOSERVER_DAMAGE_VALUE
};

enum
{
	TOSERVER_CHANGE_PASSWORD_OLD,
	TOSERVER_CHANGE_PASSWORD_NEW
};

enum
{
	TOSERVER_PLAYERITEM_VALUE
};

enum
{
	TOSERVER_REMOVED_SOUNDS_IDS
};

enum
{
	TOSERVER_NODEMETA_FIELDS_POS,
	TOSERVER_NODEMETA_FIELDS_FORMNAME,
	TOSERVER_NODEMETA_FIELDS_DATA
};

enum
{
	TOSERVER_INVENTORY_FIELDS_FORMNAME,
	TOSERVER_INVENTORY_FIELDS_DATA
};

enum
{
	TOSERVER_REQUEST_MEDIA_FILES
};

enum
{
	TOSERVER_BREATH_VALUE
};

enum
{
	/*
		actions:
		0: start digging (from undersurface) or use
		1: stop digging (all parameters ignored)
		2: digging completed
		3: place block or item (to abovesurface)
		4: use item
	*/
	TOSERVER_INTERACT_ACTION,
	TOSERVER_INTERACT_ITEM,
	TOSERVER_INTERACT_POINTED_THING
};

enum
{
	TOSERVER_CLIENT_READY_VERSION_MAJOR,
	TOSERVER_CLIENT_READY_VERSION_MINOR,
	TOSERVER_CLIENT_READY_VERSION_PATCH,
	TOSERVER_CLIENT_READY_VERSION_STRING,
	TOSERVER_CLIENT_READY_VERSION_TWEAK
};

// freeminer only packet
#define TOSERVER_DRAWCONTROL 0x44
enum
{
	TOSERVER_DRAWCONTROL_WANTED_RANGE,
	TOSERVER_DRAWCONTROL_RANGE_ALL,
	TOSERVER_DRAWCONTROL_FARMESH,
	TOSERVER_DRAWCONTROL_FOV,
	TOSERVER_DRAWCONTROL_BLOCK_OVERFLOW // not used
};