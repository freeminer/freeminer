/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#include "clientopcodes.h"

#if MINETEST_PROTO
const static ToClientCommandHandler null_command_handler = {"TOCLIENT_NULL", TOCLIENT_STATE_ALL, &Client::handleCommand_Null};
#endif

const ToClientCommandHandler toClientCommandTable[TOCLIENT_NUM_MSG_TYPES] =
{
#if MINETEST_PROTO
	null_command_handler, // 0x00 (never use this)
	null_command_handler, // 0x01
	{ "TOCLIENT_HELLO",                   TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_Hello }, // 0x02
	{ "TOCLIENT_AUTH_ACCEPT",             TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_AuthAccept }, // 0x03
	null_command_handler, // 0x04
	null_command_handler, // 0x05
	null_command_handler, // 0x06
	null_command_handler, // 0x07
	null_command_handler, // 0x08
	null_command_handler, // 0x09
	{ "TOCLIENT_ACCESS_DENIED",           TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_AccessDenied }, // 0x0A
	null_command_handler, // 0x0B
	null_command_handler, // 0x0C
	null_command_handler, // 0x0D
	null_command_handler, // 0x0E
	null_command_handler, // 0x0F
	{ "TOCLIENT_INIT",                    TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_InitLegacy }, // 0x10
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_BLOCKDATA",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_BlockData }, // 0x20
	{ "TOCLIENT_ADDNODE",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AddNode }, // 0x21
	{ "TOCLIENT_REMOVENODE",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_RemoveNode }, // 0x22
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_INVENTORY",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Inventory }, // 0x27
	null_command_handler,
	{ "TOCLIENT_TIME_OF_DAY",              TOCLIENT_STATE_CONNECTED, &Client::handleCommand_TimeOfDay }, // 0x29
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_CHAT_MESSAGE",             TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ChatMessage }, // 0x30
	{ "TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ActiveObjectRemoveAdd }, // 0x31
	{ "TOCLIENT_ACTIVE_OBJECT_MESSAGES",   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ActiveObjectMessages }, // 0x32
	{ "TOCLIENT_HP",                       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HP }, // 0x33
	{ "TOCLIENT_MOVE_PLAYER",              TOCLIENT_STATE_CONNECTED, &Client::handleCommand_MovePlayer }, // 0x34
	{ "TOCLIENT_ACCESS_DENIED_LEGACY",     TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_AccessDenied }, // 0x35
	{ "TOCLIENT_PLAYERITEM",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_PlayerItem }, // 0x36
	{ "TOCLIENT_DEATHSCREEN",              TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DeathScreen }, // 0x37
	{ "TOCLIENT_MEDIA",                    TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Media }, // 0x38
	{ "TOCLIENT_TOOLDEF",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ToolDef }, // 0x39
	{ "TOCLIENT_NODEDEF",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NodeDef }, // 0x3a
	{ "TOCLIENT_CRAFTITEMDEF",             TOCLIENT_STATE_CONNECTED, &Client::handleCommand_CraftItemDef }, // 0x3b
	{ "TOCLIENT_ANNOUNCE_MEDIA",           TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AnnounceMedia }, // 0x3c
	{ "TOCLIENT_ITEMDEF",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ItemDef }, // 0x3d
	null_command_handler,
	{ "TOCLIENT_PLAY_SOUND",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_PlaySound }, // 0x3f
	{ "TOCLIENT_STOP_SOUND",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_StopSound }, // 0x40
	{ "TOCLIENT_PRIVILEGES",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Privileges }, // 0x41
	{ "TOCLIENT_INVENTORY_FORMSPEC",       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_InventoryFormSpec }, // 0x42
	{ "TOCLIENT_DETACHED_INVENTORY",       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DetachedInventory }, // 0x43
	{ "TOCLIENT_SHOW_FORMSPEC",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ShowFormSpec }, // 0x44
	{ "TOCLIENT_MOVEMENT",                 TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Movement }, // 0x45
	{ "TOCLIENT_SPAWN_PARTICLE",           TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticle }, // 0x46
	{ "TOCLIENT_ADD_PARTICLESPAWNER",      TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AddParticleSpawner }, // 0x47
	{ "TOCLIENT_DELETE_PARTICLESPAWNER_LEGACY",   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DeleteParticleSpawner }, // 0x48
	{ "TOCLIENT_HUDADD",                   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudAdd }, // 0x49
	{ "TOCLIENT_HUDRM",                    TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudRemove }, // 0x4a
	{ "TOCLIENT_HUDCHANGE",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudChange }, // 0x4b
	{ "TOCLIENT_HUD_SET_FLAGS",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetFlags }, // 0x4c
	{ "TOCLIENT_HUD_SET_PARAM",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetParam }, // 0x4d
	{ "TOCLIENT_BREATH",                   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Breath }, // 0x4e
	{ "TOCLIENT_SET_SKY",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetSky }, // 0x4f
	{ "TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_OverrideDayNightRatio }, // 0x50
	{ "TOCLIENT_LOCAL_PLAYER_ANIMATIONS",  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_LocalPlayerAnimations }, // 0x51
	{ "TOCLIENT_EYE_OFFSET",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_EyeOffset }, // 0x52
	{ "TOCLIENT_DELETE_PARTICLESPAWNER",   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DeleteParticleSpawner }, // 0x53
#endif
};

const static ServerCommandFactory null_command_factory = { "TOSERVER_NULL", 0, false };

const ServerCommandFactory serverCommandFactoryTable[TOSERVER_NUM_MSG_TYPES] =
{
	null_command_factory, // 0x00
#if MINETEST_PROTO
	null_command_factory, // 0x01
	null_command_factory, // 0x02
	null_command_factory, // 0x03
	null_command_factory, // 0x04
	null_command_factory, // 0x05
	null_command_factory, // 0x06
	null_command_factory, // 0x07
	null_command_factory, // 0x08
	null_command_factory, // 0x09
	null_command_factory, // 0x0a
	null_command_factory, // 0x0b
	null_command_factory, // 0x0c
	null_command_factory, // 0x0d
	null_command_factory, // 0x0e
	{ "TOSERVER_INIT",               1, false }, // 0x0F
	{ "TOSERVER_INIT_LEGACY",        1, false }, // 0x10
	{ "TOSERVER_INIT2",              1, true }, // 0x11
	null_command_factory, // 0x12
	null_command_factory, // 0x13
	null_command_factory, // 0x14
	null_command_factory, // 0x15
	null_command_factory, // 0x16
	null_command_factory, // 0x17
	null_command_factory, // 0x18
	null_command_factory, // 0x19
	null_command_factory, // 0x1a
	null_command_factory, // 0x1b
	null_command_factory, // 0x1c
	null_command_factory, // 0x1d
	null_command_factory, // 0x1e
	null_command_factory, // 0x1f
	null_command_factory, // 0x20
	null_command_factory, // 0x21
	null_command_factory, // 0x22
	{ "TOSERVER_PLAYERPOS",          0, false }, // 0x23
	{ "TOSERVER_GOTBLOCKS",          2, true }, // 0x24
	{ "TOSERVER_DELETEDBLOCKS",      2, true }, // 0x25
	null_command_factory, // 0x26
	{ "TOSERVER_CLICK_OBJECT",       0, false }, // 0x27
	{ "TOSERVER_GROUND_ACTION",      0, false }, // 0x28
	{ "TOSERVER_RELEASE",            0, false }, // 0x29
	null_command_factory, // 0x2a
	null_command_factory, // 0x2b
	null_command_factory, // 0x2c
	null_command_factory, // 0x2d
	null_command_factory, // 0x2e
	null_command_factory, // 0x2f
	{ "TOSERVER_SIGNTEXT",           0, false }, // 0x30
	{ "TOSERVER_INVENTORY_ACTION",   0, true }, // 0x31
	{ "TOSERVER_CHAT_MESSAGE",       0, true }, // 0x32
	{ "TOSERVER_SIGNNODETEXT",       0, false }, // 0x33
	{ "TOSERVER_CLICK_ACTIVEOBJECT", 0, false }, // 0x34
	{ "TOSERVER_DAMAGE",             0, true }, // 0x35
	{ "TOSERVER_PASSWORD_LEGACY",    0, true }, // 0x36
	{ "TOSERVER_PLAYERITEM",         0, true }, // 0x37
	{ "TOSERVER_RESPAWN",            0, true }, // 0x38
	{ "TOSERVER_INTERACT",           0, true }, // 0x39
	{ "TOSERVER_REMOVED_SOUNDS",     1, true }, // 0x3a
	{ "TOSERVER_NODEMETA_FIELDS",    0, true }, // 0x3b
	{ "TOSERVER_INVENTORY_FIELDS",   0, true }, // 0x3c
	{ "TOSERVER_PASSWORD",           0, true }, // 0x3d
	null_command_factory, // 0x3e
	null_command_factory, // 0x3f
	{ "TOSERVER_REQUEST_MEDIA",      1, true }, // 0x40
	{ "TOSERVER_RECEIVED_MEDIA",     1, true }, // 0x41
	{ "TOSERVER_BREATH",             0, true }, // 0x42
	{ "TOSERVER_CLIENT_READY",       0, true }, // 0x43
#endif
};
