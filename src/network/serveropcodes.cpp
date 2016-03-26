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

#include "serveropcodes.h"
#include "../config.h"

const static ToServerCommandHandler null_command_handler = { "TOSERVER_NULL", TOSERVER_STATE_ALL, &Server::handleCommand_Null };

const ToServerCommandHandler toServerCommandTable[TOSERVER_NUM_MSG_TYPES] =
{
	null_command_handler, // 0x00 (never use this)
	null_command_handler, // 0x01
	{ "TOSERVER_INIT",                     TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_Init }, // 0x02
	null_command_handler, // 0x03
	null_command_handler, // 0x04
	null_command_handler, // 0x05
	null_command_handler, // 0x06
	null_command_handler, // 0x07
	null_command_handler, // 0x08
	null_command_handler, // 0x09
	null_command_handler, // 0x0a
	null_command_handler, // 0x0b
	null_command_handler, // 0x0c
	null_command_handler, // 0x0d
	null_command_handler, // 0x0e
	null_command_handler, // 0x0f
	{ "TOSERVER_INIT_LEGACY",              TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_Init_Legacy }, // 0x10
	{ "TOSERVER_INIT2",                    TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_Init2 }, // 0x11
	null_command_handler, // 0x12
	null_command_handler, // 0x13
	null_command_handler, // 0x14
	null_command_handler, // 0x15
	null_command_handler, // 0x16
	null_command_handler, // 0x17
	null_command_handler, // 0x18
	null_command_handler, // 0x19
	null_command_handler, // 0x1a
	null_command_handler, // 0x1b
	null_command_handler, // 0x1c
	null_command_handler, // 0x1d
	null_command_handler, // 0x1e
	null_command_handler, // 0x1f
	null_command_handler, // 0x20
	null_command_handler, // 0x21
	null_command_handler, // 0x22
	{ "TOSERVER_PLAYERPOS",                TOSERVER_STATE_INGAME, &Server::handleCommand_PlayerPos }, // 0x23
	{ "TOSERVER_GOTBLOCKS",                TOSERVER_STATE_STARTUP, &Server::handleCommand_GotBlocks }, // 0x24
	{ "TOSERVER_DELETEDBLOCKS",            TOSERVER_STATE_INGAME, &Server::handleCommand_DeletedBlocks }, // 0x25
	null_command_handler, // 0x26
	{ "TOSERVER_CLICK_OBJECT",             TOSERVER_STATE_INGAME, &Server::handleCommand_Deprecated }, // 0x27
	{ "TOSERVER_GROUND_ACTION",            TOSERVER_STATE_INGAME, &Server::handleCommand_Deprecated }, // 0x28
	{ "TOSERVER_RELEASE",                  TOSERVER_STATE_INGAME, &Server::handleCommand_Deprecated }, // 0x29
	null_command_handler, // 0x2a
	null_command_handler, // 0x2b
	null_command_handler, // 0x2c
	null_command_handler, // 0x2d
	null_command_handler, // 0x2e
	null_command_handler, // 0x2f
	{ "TOSERVER_SIGNTEXT",                 TOSERVER_STATE_INGAME, &Server::handleCommand_Deprecated }, // 0x30
	{ "TOSERVER_INVENTORY_ACTION",         TOSERVER_STATE_INGAME, &Server::handleCommand_InventoryAction }, // 0x31
	{ "TOSERVER_CHAT_MESSAGE",             TOSERVER_STATE_INGAME, &Server::handleCommand_ChatMessage }, // 0x32
	{ "TOSERVER_SIGNNODETEXT",             TOSERVER_STATE_INGAME, &Server::handleCommand_Deprecated }, // 0x33
	{ "TOSERVER_CLICK_ACTIVEOBJECT",       TOSERVER_STATE_INGAME, &Server::handleCommand_Deprecated }, // 0x34
	{ "TOSERVER_DAMAGE",                   TOSERVER_STATE_INGAME, &Server::handleCommand_Damage }, // 0x35
	{ "TOSERVER_PASSWORD_LEGACY",          TOSERVER_STATE_INGAME, &Server::handleCommand_Password }, // 0x36
	{ "TOSERVER_PLAYERITEM",               TOSERVER_STATE_INGAME, &Server::handleCommand_PlayerItem }, // 0x37
	{ "TOSERVER_RESPAWN",                  TOSERVER_STATE_INGAME, &Server::handleCommand_Respawn }, // 0x38
	{ "TOSERVER_INTERACT",                 TOSERVER_STATE_INGAME, &Server::handleCommand_Interact }, // 0x39
	{ "TOSERVER_REMOVED_SOUNDS",           TOSERVER_STATE_INGAME, &Server::handleCommand_RemovedSounds }, // 0x3a
	{ "TOSERVER_NODEMETA_FIELDS",          TOSERVER_STATE_INGAME, &Server::handleCommand_NodeMetaFields }, // 0x3b
	{ "TOSERVER_INVENTORY_FIELDS",         TOSERVER_STATE_INGAME, &Server::handleCommand_InventoryFields }, // 0x3c
	null_command_handler, // 0x3d
	null_command_handler, // 0x3e
	null_command_handler, // 0x3f
	{ "TOSERVER_REQUEST_MEDIA",            TOSERVER_STATE_STARTUP, &Server::handleCommand_RequestMedia }, // 0x40
	{ "TOSERVER_RECEIVED_MEDIA",           TOSERVER_STATE_STARTUP, &Server::handleCommand_ReceivedMedia }, // 0x41
	{ "TOSERVER_BREATH",                   TOSERVER_STATE_INGAME, &Server::handleCommand_Breath }, // 0x42
	{ "TOSERVER_CLIENT_READY",             TOSERVER_STATE_STARTUP, &Server::handleCommand_ClientReady }, // 0x43

	{ "TOSERVER_DRAWCONTROL",              TOSERVER_STATE_STARTUP, &Server::handleCommand_Drawcontrol }, // 0x44

	null_command_handler, // 0x45
	null_command_handler, // 0x46
	null_command_handler, // 0x47
	null_command_handler, // 0x48
	null_command_handler, // 0x49
	null_command_handler, // 0x4a
	null_command_handler, // 0x4b
	null_command_handler, // 0x4c
	null_command_handler, // 0x4d
	null_command_handler, // 0x4e
	null_command_handler, // 0x4f
	{ "TOSERVER_FIRST_SRP",          TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_FirstSrp }, // 0x50
	{ "TOSERVER_SRP_BYTES_A",        TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_SrpBytesA }, // 0x51
	{ "TOSERVER_SRP_BYTES_M",        TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_SrpBytesM }, // 0x52
};

const static ClientCommandFactory null_command_factory = { "TOCLIENT_NULL", 0, false };

const ClientCommandFactory clientCommandFactoryTable[TOCLIENT_NUM_MSG_TYPES] =
{
	null_command_factory, // 0x00
	null_command_factory, // 0x01
	{ "TOCLIENT_HELLO",             0, true }, // 0x02
	{ "TOCLIENT_AUTH_ACCEPT",       0, true }, // 0x03
	{ "TOCLIENT_ACCEPT_SUDO_MODE",  0, true }, // 0x04
	{ "TOCLIENT_DENY_SUDO_MODE",    0, true }, // 0x05
	null_command_factory, // 0x06
	null_command_factory, // 0x07
	null_command_factory, // 0x08
	null_command_factory, // 0x09
	{ "TOCLIENT_ACCESS_DENIED",     0, true }, // 0x0A
	null_command_factory, // 0x0B
	null_command_factory, // 0x0C
	null_command_factory, // 0x0D
	null_command_factory, // 0x0E
	null_command_factory, // 0x0F
	{ "TOCLIENT_INIT",              0, true }, // 0x10
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	{ "TOCLIENT_BLOCKDATA",                2, true }, // 0x20
	{ "TOCLIENT_ADDNODE",                  0, true }, // 0x21
	{ "TOCLIENT_REMOVENODE",               0, true }, // 0x22
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	{ "TOCLIENT_INVENTORY",                0, true }, // 0x27
	null_command_factory,
	{ "TOCLIENT_TIME_OF_DAY",              0, true }, // 0x29
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	{ "TOCLIENT_CHAT_MESSAGE",             0, true }, // 0x30
	{ "TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD", 0, true }, // 0x31
	{ "TOCLIENT_ACTIVE_OBJECT_MESSAGES",   0, true }, // 0x32 Special packet, sent by 0 (rel) and 1 (unrel) channel
	{ "TOCLIENT_HP",                       0, true }, // 0x33
	{ "TOCLIENT_MOVE_PLAYER",              0, true }, // 0x34
	{ "TOCLIENT_ACCESS_DENIED_LEGACY",     0, true }, // 0x35
	{ "TOCLIENT_PLAYERITEM",               0, false }, // 0x36 obsolete
	{ "TOCLIENT_DEATHSCREEN",              0, true }, // 0x37
	{ "TOCLIENT_MEDIA",                    2, true }, // 0x38
	{ "TOCLIENT_TOOLDEF",                  0, false }, // 0x39 obsolete
	{ "TOCLIENT_NODEDEF",                  0, true }, // 0x3a
	{ "TOCLIENT_CRAFTITEMDEF",             0, false }, // 0x3b obsolete
	{ "TOCLIENT_ANNOUNCE_MEDIA",           0, true }, // 0x3c
	{ "TOCLIENT_ITEMDEF",                  0, true }, // 0x3d
	null_command_factory,
	{ "TOCLIENT_PLAY_SOUND",               0, true }, // 0x3f
	{ "TOCLIENT_STOP_SOUND",               0, true }, // 0x40
	{ "TOCLIENT_PRIVILEGES",               0, true }, // 0x41
	{ "TOCLIENT_INVENTORY_FORMSPEC",       0, true }, // 0x42
	{ "TOCLIENT_DETACHED_INVENTORY",       0, true }, // 0x43
	{ "TOCLIENT_SHOW_FORMSPEC",            0, true }, // 0x44
	{ "TOCLIENT_MOVEMENT",                 0, true }, // 0x45
	{ "TOCLIENT_SPAWN_PARTICLE",           0, true }, // 0x46
	{ "TOCLIENT_ADD_PARTICLESPAWNER",      0, true }, // 0x47
	{ "TOCLIENT_DELETE_PARTICLESPAWNER_LEGACY",   0, true }, // 0x48
	{ "TOCLIENT_HUDADD",                   1, true }, // 0x49
	{ "TOCLIENT_HUDRM",                    1, true }, // 0x4a
	{ "TOCLIENT_HUDCHANGE",                0, true }, // 0x4b
	{ "TOCLIENT_HUD_SET_FLAGS",            0, true }, // 0x4c
	{ "TOCLIENT_HUD_SET_PARAM",            0, true }, // 0x4d
	{ "TOCLIENT_BREATH",                   0, true }, // 0x4e
	{ "TOCLIENT_SET_SKY",                  0, true }, // 0x4f
	{ "TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO", 0, true }, // 0x50
	{ "TOCLIENT_LOCAL_PLAYER_ANIMATIONS",  0, true }, // 0x51
	{ "TOCLIENT_EYE_OFFSET",               0, true }, // 0x52
	{ "TOCLIENT_DELETE_PARTICLESPAWNER",   0, true }, // 0x53
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	null_command_factory,
	{ "TOSERVER_SRP_BYTES_S_B",            0, true }, // 0x60
};
