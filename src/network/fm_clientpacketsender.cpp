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

void Client::request_media(const std::vector<std::string> &file_requests)
{
	MSGPACK_PACKET_INIT(TOSERVER_REQUEST_MEDIA, 1);
	PACK(TOSERVER_REQUEST_MEDIA_FILES, file_requests);

	// Send as reliable
	Send(1, buffer, true);
	infostream<<"Client: Sending media request list to server ("
			<<file_requests.size()<<" files)"<<std::endl;
}

void Client::received_media()
{
	// notify server we received everything
	MSGPACK_PACKET_INIT(TOSERVER_RECEIVED_MEDIA, 0);
	// Send as reliable
	Send(1, buffer, true);
	infostream<<"Client: Notifying server that we received all media"
			<<std::endl;
}


void Client::interact(u8 action, const PointedThing& pointed)
{
	if(m_state != LC_Ready) {
		infostream << "Client::interact() "
				"cancelled (not connected)"
				<< std::endl;
		return;
	}

	/*
		[0] u16 command
		[2] u8 action
		[3] u16 item
		[5] u32 length of the next item
		[9] serialized PointedThing
		actions:
		0: start digging (from undersurface) or use
		1: stop digging (all parameters ignored)
		2: digging completed
		3: place block or item (to abovesurface)
		4: use item
	*/
	MSGPACK_PACKET_INIT(TOSERVER_INTERACT, 3);
	PACK(TOSERVER_INTERACT_ACTION, action);
	PACK(TOSERVER_INTERACT_ITEM, getPlayerItem());
	PACK(TOSERVER_INTERACT_POINTED_THING, pointed);

	// Send as reliable
	Send(0, buffer, true);
}


void Client::sendLegacyInit(const std::string &playerName, const std::string &playerPassword)
{
	// Send TOSERVER_INIT
	// [0] u16 TOSERVER_INIT
	// [2] u8 SER_FMT_VER_HIGHEST_READ
	// [3] u8[20] player_name
	// [23] u8[28] password (new in some version)
	// [51] u16 minimum supported network protocol version (added sometime)
	// [53] u16 maximum supported network protocol version (added later than the previous one)
	MSGPACK_PACKET_INIT(TOSERVER_INIT_LEGACY, 5);
	PACK(TOSERVER_INIT_FMT, SER_FMT_VER_HIGHEST_READ);
	PACK(TOSERVER_INIT_NAME, playerName);
	PACK(TOSERVER_INIT_PASSWORD, playerPassword);
	PACK(TOSERVER_INIT_PROTOCOL_VERSION_MIN, CLIENT_PROTOCOL_VERSION_MIN);
	PACK(TOSERVER_INIT_PROTOCOL_VERSION_MAX, CLIENT_PROTOCOL_VERSION_MAX);

	// Send as unreliable
	Send(1, buffer, false);
}

void Client::sendDeletedBlocks(std::vector<v3s16> &blocks)
{

	MSGPACK_PACKET_INIT(TOSERVER_DELETEDBLOCKS, 1);
	PACK(TOSERVER_DELETEDBLOCKS_DATA, blocks);

	m_con.Send(PEER_ID_SERVER, 2, buffer, true);
}

/*
void Client::sendGotBlocks(v3s16 block)
{
	NetworkPacket pkt(TOSERVER_GOTBLOCKS, 1 + 6);
	pkt << (u8) 1 << block;
	Send(&pkt);
}
*/

void Client::sendRemovedSounds(std::vector<s32> &soundList)
{
	MSGPACK_PACKET_INIT(TOSERVER_REMOVED_SOUNDS, 1);
	PACK(TOSERVER_REMOVED_SOUNDS_IDS, soundList);
	// Send as reliable
	Send(1, buffer, true);
}

void Client::sendNodemetaFields(v3s16 p, const std::string &formname,
		const std::map<std::string, std::string> &fields)
{
	MSGPACK_PACKET_INIT(TOSERVER_NODEMETA_FIELDS, 3);
	PACK(TOSERVER_NODEMETA_FIELDS_POS, p);
	PACK(TOSERVER_NODEMETA_FIELDS_FORMNAME, formname);
	PACK(TOSERVER_NODEMETA_FIELDS_DATA, fields);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendInventoryFields(const std::string &formname,
		const std::map<std::string, std::string> &fields)
{
	MSGPACK_PACKET_INIT(TOSERVER_INVENTORY_FIELDS, 2);
	PACK(TOSERVER_INVENTORY_FIELDS_FORMNAME, formname);
	PACK(TOSERVER_INVENTORY_FIELDS_DATA, fields);
	Send(0, buffer, true);
}

void Client::sendInventoryAction(InventoryAction *a)
{
	MSGPACK_PACKET_INIT(TOSERVER_INVENTORY_ACTION, 1);

	std::ostringstream os(std::ios_base::binary);
	a->serialize(os);
	std::string s = os.str();

	PACK(TOSERVER_INVENTORY_ACTION_DATA, s);

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendChatMessage(const std::string &message)
{
	MSGPACK_PACKET_INIT(TOSERVER_CHAT_MESSAGE, 1);
	PACK(TOSERVER_CHAT_MESSAGE_DATA, message);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendChangePassword(const std::string &oldpassword,
								const std::string &newpassword)
{
	Player *player = m_env.getLocalPlayer();
	if(player == NULL)
		return;

	std::string playername = player->getName();
	std::string oldpwd = translatePassword(playername, oldpassword);
	std::string newpwd = translatePassword(playername, newpassword);

	MSGPACK_PACKET_INIT(TOSERVER_CHANGE_PASSWORD, 2);
	PACK(TOSERVER_CHANGE_PASSWORD_OLD, oldpwd);
	PACK(TOSERVER_CHANGE_PASSWORD_NEW, newpwd);

	// Send as reliable
	Send(0, buffer, true);
}


void Client::sendDamage(u8 damage)
{
	DSTACK(__FUNCTION_NAME);
	MSGPACK_PACKET_INIT(TOSERVER_DAMAGE, 1);
	PACK(TOSERVER_DAMAGE_VALUE, damage);

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendBreath(u16 breath)
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOSERVER_BREATH, 1);
	PACK(TOSERVER_BREATH_VALUE, breath);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendRespawn()
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOSERVER_RESPAWN, 0);
	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendReady()
{
	DSTACK(__FUNCTION_NAME);

	MSGPACK_PACKET_INIT(TOSERVER_CLIENT_READY, 3);
	PACK(TOSERVER_CLIENT_READY_VERSION_MAJOR, VERSION_MAJOR);
	PACK(TOSERVER_CLIENT_READY_VERSION_MINOR, VERSION_MINOR);
	// PACK(TOSERVER_CLIENT_READY_VERSION_PATCH, VERSION_PATCH_ORIG); TODO
	PACK(TOSERVER_CLIENT_READY_VERSION_STRING, std::string(g_version_hash));

	// Send as reliable
	Send(0, buffer, true);
}

void Client::sendPlayerPos()
{
	LocalPlayer *myplayer = m_env.getLocalPlayer();
	if(myplayer == NULL)
		return;

	// Save bandwidth by only updating position when something changed
	if(myplayer->last_position == myplayer->getPosition() &&
			myplayer->last_speed == myplayer->getSpeed() &&
			myplayer->last_pitch == myplayer->getPitch() &&
			myplayer->last_yaw == myplayer->getYaw() &&
			myplayer->last_keyPressed == myplayer->keyPressed)
		return;

	myplayer->last_position = myplayer->getPosition();
	myplayer->last_speed = myplayer->getSpeed();
	myplayer->last_pitch = myplayer->getPitch();
	myplayer->last_yaw = myplayer->getYaw();
	myplayer->last_keyPressed = myplayer->keyPressed;

	u16 our_peer_id;
	{
		//JMutexAutoLock lock(m_con_mutex); //bulk comment-out
		our_peer_id = m_con.GetPeerID();
	}

	// Set peer id if not set already
	if(myplayer->peer_id == PEER_ID_INEXISTENT)
		myplayer->peer_id = our_peer_id;
	// Check that an existing peer_id is the same as the connection's
	if (myplayer->peer_id != our_peer_id)
		return;

	MSGPACK_PACKET_INIT(TOSERVER_PLAYERPOS, 5);
	PACK(TOSERVER_PLAYERPOS_POSITION, myplayer->getPosition());
	PACK(TOSERVER_PLAYERPOS_SPEED, myplayer->getSpeed());
	PACK(TOSERVER_PLAYERPOS_PITCH, myplayer->getPitch());
	PACK(TOSERVER_PLAYERPOS_YAW, myplayer->getYaw());
	PACK(TOSERVER_PLAYERPOS_KEY_PRESSED, myplayer->keyPressed);
	// Send as unreliable
	Send(0, buffer, false);
}

void Client::sendPlayerItem(u16 item)
{
	Player *myplayer = m_env.getLocalPlayer();
	if(myplayer == NULL)
		return;

	u16 our_peer_id = m_con.GetPeerID();

	// Set peer id if not set already
	if(myplayer->peer_id == PEER_ID_INEXISTENT)
		myplayer->peer_id = our_peer_id;
	// Check that an existing peer_id is the same as the connection's
	assert(myplayer->peer_id == our_peer_id);

	MSGPACK_PACKET_INIT(TOSERVER_PLAYERITEM, 1);
	PACK(TOSERVER_PLAYERITEM_VALUE, item);

	// Send as reliable
	Send(0, buffer, true);
}


void Client::sendDrawControl() {
	MSGPACK_PACKET_INIT(TOSERVER_DRAWCONTROL, 5);
	const auto & draw_control = m_env.getClientMap().getControl();
	PACK(TOSERVER_DRAWCONTROL_WANTED_RANGE, (u32)draw_control.wanted_range);
	PACK(TOSERVER_DRAWCONTROL_RANGE_ALL, (u32)draw_control.range_all);
	PACK(TOSERVER_DRAWCONTROL_FARMESH, (u8)draw_control.farmesh);
	PACK(TOSERVER_DRAWCONTROL_FOV, draw_control.fov);
	PACK(TOSERVER_DRAWCONTROL_BLOCK_OVERFLOW, draw_control.block_overflow);

	Send(0, buffer, false);
}
