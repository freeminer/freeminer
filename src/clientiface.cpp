/*
clientiface.cpp
Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "clientiface.h"
#include "player.h"
#include "settings.h"
#include "mapblock.h"
#include "connection.h"
#include "environment.h"
#include "map.h"
#include "emerge.h"
#include "serverobject.h"              // TODO this is used for cleanup of only

#include "util/numeric.h"
#include "util/mathconstants.h"

#include "main.h"                      // for g_settings

void RemoteClient::GetNextBlocks(
		ServerEnvironment *env,
		EmergeManager * emerge,
		float dtime,
		double m_uptime,
		std::vector<PrioritySortedBlockTransfer> &dest)
{
	DSTACK(__FUNCTION_NAME);

	// Increment timers
	m_nothing_to_send_pause_timer -= dtime;
	m_nearest_unsent_reset_timer += dtime;

	if(m_nothing_to_send_pause_timer >= 0)
		return;

	Player *player = env->getPlayer(peer_id);
	// This can happen sometimes; clients and players are not in perfect sync.
	if(player == NULL)
		return;

	// Won't send anything if already sending
	if(m_blocks_sending.size() >= g_settings->getU16
			("max_simultaneous_block_sends_per_client"))
	{
		//infostream<<"Not sending any blocks, Queue full."<<std::endl;
		return;
	}

	v3f playerpos = player->getPosition();
	v3f playerspeed = player->getSpeed();
	v3f playerspeeddir(0,0,0);
	if(playerspeed.getLength() > 1.0*BS)
		playerspeeddir = playerspeed / playerspeed.getLength();
	// Predict to next block
	v3f playerpos_predicted = playerpos + playerspeeddir*MAP_BLOCKSIZE*BS;

	v3s16 center_nodepos = floatToInt(playerpos_predicted, BS);

	v3s16 center = getNodeBlockPos(center_nodepos);

	// Camera position and direction
	v3f camera_pos = player->getEyePosition();
	v3f camera_dir = v3f(0,0,1);
	camera_dir.rotateYZBy(player->getPitch());
	camera_dir.rotateXZBy(player->getYaw());

	/*infostream<<"camera_dir=("<<camera_dir.X<<","<<camera_dir.Y<<","
			<<camera_dir.Z<<")"<<std::endl;*/

	/*
		Get the starting value of the block finder radius.
	*/

	if(m_last_center != center)
	{
		m_nearest_unsent_d = 0;
		m_last_center = center;
	}

	/*infostream<<"m_nearest_unsent_reset_timer="
			<<m_nearest_unsent_reset_timer<<std::endl;*/

	// Reset periodically to workaround for some bugs or stuff
	if(m_nearest_unsent_reset_timer > 120.0)
	{
		m_nearest_unsent_reset_timer = 0;
		m_nearest_unsent_d = 0;
		m_nearest_unsent_nearest = 0;
		//infostream<<"Resetting m_nearest_unsent_d for "<<peer_id<<std::endl;
	}

	//s16 last_nearest_unsent_d = m_nearest_unsent_d;
	s16 d_start = m_nearest_unsent_d;

	//infostream<<"d_start="<<d_start<<std::endl;

	u16 max_simul_sends_setting = g_settings->getU16
			("max_simultaneous_block_sends_per_client");
	u16 max_simul_sends_usually = max_simul_sends_setting;

	/*
		Check the time from last addNode/removeNode.

		Decrease send rate if player is building stuff.
	*/
	m_time_from_building += dtime;
	if(m_time_from_building < g_settings->getFloat(
				"full_block_send_enable_min_time_from_building"))
	{
		max_simul_sends_usually
			= LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS;
	}

	/*
		Number of blocks sending + number of blocks selected for sending
	*/
	u32 num_blocks_selected = m_blocks_sending.size();

	/*
		next time d will be continued from the d from which the nearest
		unsent block was found this time.

		This is because not necessarily any of the blocks found this
		time are actually sent.
	*/
	s32 new_nearest_unsent_d = -1;

	s16 full_d_max = g_settings->getS16("max_block_send_distance");
	if (wanted_range) {
		s16 wanted_blocks = wanted_range / MAP_BLOCKSIZE + 1;
		if (wanted_blocks < full_d_max)
			full_d_max = wanted_blocks;
	}

	s16 d_max = full_d_max;
	s16 d_max_gen = g_settings->getS16("max_block_generate_distance");

	// Don't loop very much at a time
	s16 max_d_increment_at_time = 5;
	if(d_max > d_start + max_d_increment_at_time)
		d_max = d_start + max_d_increment_at_time;
	/*if(d_max_gen > d_start+2)
		d_max_gen = d_start+2;*/

	//infostream<<"Starting from "<<d_start<<std::endl;

	s32 nearest_emerged_d = -1;
	s32 nearest_emergefull_d = -1;
	s32 nearest_sent_d = -1;
	bool queue_is_full = false;

	f32 speed_in_blocks = (playerspeed/(MAP_BLOCKSIZE*BS)).getLength();

	s16 d;
	for(d = d_start; d <= d_max; d++)
	{
		/*errorstream<<"checking d="<<d<<" for "
				<<server->getPlayerName(peer_id)<<std::endl;*/
		//infostream<<"RemoteClient::SendBlocks(): d="<<d<<" d_start="<<d_start<<" d_max="<<d_max<<" d_max_gen="<<d_max_gen<<std::endl;

		/*
			If m_nearest_unsent_d was changed by the EmergeThread
			(it can change it to 0 through SetBlockNotSent),
			update our d to it.
			Else update m_nearest_unsent_d
		*/
		/*if(m_nearest_unsent_d != last_nearest_unsent_d)
		{
			d = m_nearest_unsent_d;
			last_nearest_unsent_d = m_nearest_unsent_d;
		}*/

		std::list<v3s16> list;
		bool can_skip = true;
		// Fast fall/move optimize. speed_in_blocks now limited to 6.4
		if (speed_in_blocks>0.8 && d <= 2) {
			can_skip = false;
			if (d == 0) {
				for(s16 addn = 0; addn < (speed_in_blocks+1)*2; ++addn)
					list.push_back(floatToInt(playerspeeddir*addn, 1));
			} else if (d == 1) {
				for(s16 addn = 0; addn < (speed_in_blocks+1)*1.5; ++addn) {
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( 0,  0,  1)); // back
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( -1, 0,  0)); // left
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( 1,  0,  0)); // right
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( 0,  0, -1)); // front
				}
			} else if (d == 2) {
				for(s16 addn = 0; addn < (speed_in_blocks+1)*1.5; ++addn) {
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( -1, 0,  1)); // back left
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( 1,  0,  1)); // left right
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( -1, 0, -1)); // right left
					list.push_back(floatToInt(playerspeeddir*addn, 1) + v3s16( 1,  0, -1)); // front right
				}
			}
		} else {
		/*
			Get the border/face dot coordinates of a "d-radiused"
			box
		*/
			getFacePositions(list, d);
		}

		std::list<v3s16>::iterator li;
		for(li=list.begin(); li!=list.end(); ++li)
		{
			v3s16 p = *li + center;

			/*
				Send throttling
				- Don't allow too many simultaneous transfers
				- EXCEPT when the blocks are very close

				Also, don't send blocks that are already flying.
			*/

			// Start with the usual maximum
			u16 max_simul_dynamic = max_simul_sends_usually;

			// If block is very close, allow full maximum
			if(d <= BLOCK_SEND_DISABLE_LIMITS_MAX_D)
				max_simul_dynamic = max_simul_sends_setting;

			// Don't select too many blocks for sending
			if(num_blocks_selected >= max_simul_dynamic)
			{
				queue_is_full = true;
				goto queue_full_break;
			}

			// Don't send blocks that are currently being transferred
			if(m_blocks_sending.find(p) != m_blocks_sending.end())
				continue;

			/*
				Do not go over-limit
			*/
			if(p.X < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.X > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Y < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Y > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Z < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Z > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE)
				continue;

			// If this is true, inexistent block will be made from scratch
			bool generate = d <= d_max_gen;

			{
				/*// Limit the generating area vertically to 2/3
				if(abs(p.Y - center.Y) > d_max_gen - d_max_gen / 3)
					generate = false;*/

				// Limit the send area vertically to 1/2
				if(can_skip && abs(p.Y - center.Y) > full_d_max / 2)
					generate = false;
			}


			//infostream<<"d="<<d<<std::endl;
			/*
				Don't generate or send if not in sight
				FIXME This only works if the client uses a small enough
				FOV setting. The default of 72 degrees is fine.
			*/

			float camera_fov = (80.0*M_PI/180) * 4./3.;
			if(can_skip && isBlockInSight(p, camera_pos, camera_dir, camera_fov, 10000*BS) == false)
			{
				continue;
			}

			/*
				Don't send already sent blocks
			*/
			{
				if(m_blocks_sent.find(p) != m_blocks_sent.end() && m_blocks_sent[p] > 0 && m_blocks_sent[p] + (d <= 2 ? 1 : d*d) > m_uptime) {
					continue;
				}
			}

			/*
				Check if map has this block
			*/
			MapBlock *block = env->getMap().getBlockNoCreateNoEx(p);

			bool surely_not_found_on_disk = false;
			bool block_is_invalid = false;
			if(block != NULL)
			{

				if (m_blocks_sent[p] > 0 && m_blocks_sent[p] >= block->m_changed_timestamp) {
					continue;
				}

				// Reset usage timer, this block will be of use in the future.
				block->resetUsageTimer();

				// Block is dummy if data doesn't exist.
				// It means it has been not found from disk and not generated
				if(block->isDummy())
				{
					surely_not_found_on_disk = true;
				}

				// Block is valid if lighting is up-to-date and data exists
				if(block->isValid() == false)
				{
					block_is_invalid = true;
				}

				if(block->isGenerated() == false)
					block_is_invalid = true;

				/*
					If block is not close, don't send it unless it is near
					ground level.

					Block is near ground level if night-time mesh
					differs from day-time mesh.
				*/
/*
				if(d >= 4)
				{
					if(block->getDayNightDiff() == false)
						continue;
				}
*/
			}

			/*
				If block has been marked to not exist on disk (dummy)
				and generating new ones is not wanted, skip block.
			*/
			if(generate == false && surely_not_found_on_disk == true)
			{
				// get next one.
				continue;
			}

			/*
				Add inexistent block to emerge queue.
			*/
			if(block == NULL || surely_not_found_on_disk || block_is_invalid)
			{
				if (emerge->enqueueBlockEmerge(peer_id, p, generate)) {
					if (nearest_emerged_d == -1)
						nearest_emerged_d = d;
				} else {
					if (nearest_emergefull_d == -1)
						nearest_emergefull_d = d;
					goto queue_full_break;
				}

				// get next one.
				continue;
			}

			if(nearest_sent_d == -1)
				nearest_sent_d = d;

			/*
				Add block to send queue
			*/

			PrioritySortedBlockTransfer q((float)d, p, peer_id);

			dest.push_back(q);

			num_blocks_selected += 1;
		}
	}
queue_full_break:

	//infostream<<"Stopped at "<<d<<" d_start="<<d_start<< " d_max="<<d_max<<" nearest_emerged_d="<<nearest_emerged_d<<" nearest_emergefull_d="<<nearest_emergefull_d<< " new_nearest_unsent_d="<<new_nearest_unsent_d<< " sel="<<num_blocks_selected<<std::endl;
	if(!num_blocks_selected && d_start == d) {
		//new_nearest_unsent_d = 0;
		m_nothing_to_send_pause_timer = 1.0;
	}
		

	// If nothing was found for sending and nothing was queued for
	// emerging, continue next time browsing from here
	if(nearest_emerged_d != -1){
		new_nearest_unsent_d = nearest_emerged_d;
	} else if(nearest_emergefull_d != -1){
		new_nearest_unsent_d = nearest_emergefull_d;
	} else {
		if(d > full_d_max){
			new_nearest_unsent_d = 0;
			m_nothing_to_send_pause_timer = 2.0;
		} else {
			if(nearest_sent_d != -1)
				new_nearest_unsent_d = nearest_sent_d;
			else
				new_nearest_unsent_d = d;
		}
	}

	if(new_nearest_unsent_d != -1)
		m_nearest_unsent_d = new_nearest_unsent_d;
}

void RemoteClient::GotBlock(v3s16 p, double time)
{
	if(m_blocks_sending.find(p) != m_blocks_sending.end())
		m_blocks_sending.erase(p);
	else
	{
		m_excess_gotblocks++;
	}
	m_blocks_sent[p] = time;
}

void RemoteClient::SentBlock(v3s16 p)
{
	if(m_blocks_sending.find(p) == m_blocks_sending.end())
		m_blocks_sending[p] = 0.0;
	else
		infostream<<"RemoteClient::SentBlock(): Sent block"
				" already in m_blocks_sending"<<std::endl;
}

void RemoteClient::SetBlockNotSent(v3s16 p)
{
	++m_nearest_unsent_nearest;

	if(m_blocks_sending.find(p) != m_blocks_sending.end())
		m_blocks_sending.erase(p);
}

void RemoteClient::SetBlocksNotSent(std::map<v3s16, MapBlock*> &blocks)
{
	for(std::map<v3s16, MapBlock*>::iterator
			i = blocks.begin();
			i != blocks.end(); ++i)
	{
		v3s16 p = i->first;
		SetBlockNotSent(p);
	}
}

void RemoteClient::notifyEvent(ClientStateEvent event)
{
	switch (m_state)
	{
	case Invalid:
		//assert("State update for client in invalid state" != 0);
		break;

	case Created:
		switch(event)
		{
		case Init:
			m_state = InitSent;
			break;

		case Disconnect:
			m_state = Disconnecting;
			break;

		case SetDenied:
			m_state = Denied;
			break;

		/* GotInit2 SetDefinitionsSent SetMediaSent */
		default:
			break;
			//assert("Invalid client state transition!" == 0);
		}
		break;

	case Denied:
		/* don't do anything if in denied state */
		break;

	case InitSent:
		switch(event)
		{
		case GotInit2:
			confirmSerializationVersion();
			m_state = InitDone;
			break;

		case Disconnect:
			m_state = Disconnecting;
			break;

		case SetDenied:
			m_state = Denied;
			break;

		/* Init SetDefinitionsSent SetMediaSent */
		default:
			break;
			//assert("Invalid client state transition!" == 0);
		}
		break;

	case InitDone:
		switch(event)
		{
		case SetDefinitionsSent:
			m_state = DefinitionsSent;
			break;

		case Disconnect:
			m_state = Disconnecting;
			break;

		case SetDenied:
			m_state = Denied;
			break;

		/* Init GotInit2 SetMediaSent */
		default:
			break;
			//assert("Invalid client state transition!" == 0);
		}
		break;

	case DefinitionsSent:
		switch(event)
		{
		case SetMediaSent:
			m_state = Active;
			break;

		case Disconnect:
			m_state = Disconnecting;
			break;

		case SetDenied:
			m_state = Denied;
			break;

		/* Init GotInit2 SetDefinitionsSent */
		default:
			break;
			//assert("Invalid client state transition!" == 0);
		}
		break;

	case Active:
		switch(event)
		{
		case SetDenied:
			m_state = Denied;
			break;

		case Disconnect:
			m_state = Disconnecting;
			break;

		/* Init GotInit2 SetDefinitionsSent SetMediaSent SetDenied */
		default:
			//assert("Invalid client state transition!" == 0);
			break;
		}
		break;

	case Disconnecting:
		/* we are already disconnecting */
		break;
	}
}

ClientInterface::ClientInterface(con::Connection* con)
:
	m_con(con),
	m_env(NULL),
	m_print_info_timer(0.0)
{

}
ClientInterface::~ClientInterface()
{
	/*
		Delete clients
	*/
	{
		JMutexAutoLock clientslock(m_clients_mutex);

		for(std::map<u16, RemoteClient*>::iterator
			i = m_clients.begin();
			i != m_clients.end(); ++i)
		{

			// Delete client
			delete i->second;
		}
	}
}

std::list<u16> ClientInterface::getClientIDs(ClientState min_state)
{
	std::list<u16> reply;
	JMutexAutoLock clientslock(m_clients_mutex);

	for(std::map<u16, RemoteClient*>::iterator
		i = m_clients.begin();
		i != m_clients.end(); ++i)
	{
		if (i->second->getState() >= min_state)
			reply.push_back(i->second->peer_id);
	}

	return reply;
}

std::vector<std::string> ClientInterface::getPlayerNames()
{
	return m_clients_names;
}


void ClientInterface::step(float dtime)
{
	m_print_info_timer += dtime;
	if(m_print_info_timer >= 30.0)
	{
		m_print_info_timer = 0.0;
		UpdatePlayerList();
	}
}

void ClientInterface::UpdatePlayerList()
{
	if (m_env != NULL)
		{
		std::list<u16> clients = getClientIDs();
		m_clients_names.clear();


		if(clients.size() != 0)
			infostream<<"Players:"<<std::endl;
		for(std::list<u16>::iterator
			i = clients.begin();
			i != clients.end(); ++i)
		{
			Player *player = m_env->getPlayer(*i);
			if(player==NULL)
				continue;
			infostream<<"* "<<player->getName()<<"\t";

			{
				JMutexAutoLock clientslock(m_clients_mutex);
				RemoteClient* client = lockedGetClientNoEx(*i);
				if(client != NULL)
					client->PrintInfo(infostream);
			}
			m_clients_names.push_back(player->getName());
		}
	}
}

void ClientInterface::send(u16 peer_id,u8 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	m_con->Send(peer_id, channelnum, data, reliable);
}

void ClientInterface::sendToAll(u16 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	JMutexAutoLock clientslock(m_clients_mutex);
	for(std::map<u16, RemoteClient*>::iterator
		i = m_clients.begin();
		i != m_clients.end(); ++i)
	{
		RemoteClient *client = i->second;

		if (client->net_proto_version != 0)
		{
			m_con->Send(client->peer_id, channelnum, data, reliable);
		}
	}
}

RemoteClient* ClientInterface::getClientNoEx(u16 peer_id, ClientState state_min)
{
	JMutexAutoLock clientslock(m_clients_mutex);
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n == m_clients.end())
		return NULL;

	if (n->second->getState() >= state_min)
		return n->second;
	else
		return NULL;
}

RemoteClient* ClientInterface::lockedGetClientNoEx(u16 peer_id, ClientState state_min)
{
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n == m_clients.end())
		return NULL;

	if (n->second->getState() >= state_min)
		return n->second;
	else
		return NULL;
}

ClientState ClientInterface::getClientState(u16 peer_id)
{
	JMutexAutoLock clientslock(m_clients_mutex);
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n == m_clients.end())
		return Invalid;

	return n->second->getState();
}

void ClientInterface::setPlayerName(u16 peer_id,std::string name)
{
	JMutexAutoLock clientslock(m_clients_mutex);
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n != m_clients.end())
		n->second->setName(name);
}

void ClientInterface::DeleteClient(u16 peer_id)
{
	JMutexAutoLock conlock(m_clients_mutex);

	// Error check
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if(n == m_clients.end())
		return;

	/*
		Mark objects to be not known by the client
	*/
	//TODO this should be done by client destructor!!!
	RemoteClient *client = n->second;
	// Handle objects
	for(std::set<u16>::iterator
			i = client->m_known_objects.begin();
			i != client->m_known_objects.end(); ++i)
	{
		// Get object
		u16 id = *i;
		ServerActiveObject* obj = m_env->getActiveObject(id);

		if(obj && obj->m_known_by_count > 0)
			obj->m_known_by_count--;
	}

	// Delete client
	delete m_clients[peer_id];
	m_clients.erase(peer_id);
}

void ClientInterface::CreateClient(u16 peer_id)
{
	JMutexAutoLock conlock(m_clients_mutex);

	// Error check
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);
	// The client shouldn't already exist
	if(n != m_clients.end()) return;

	// Create client
	RemoteClient *client = new RemoteClient(m_env);
	client->peer_id = peer_id;
	m_clients[client->peer_id] = client;
}

void ClientInterface::event(u16 peer_id, ClientStateEvent event)
{
	{
		JMutexAutoLock clientlock(m_clients_mutex);

		// Error check
		std::map<u16, RemoteClient*>::iterator n;
		n = m_clients.find(peer_id);

		// No client to deliver event
		if (n == m_clients.end())
			return;
		n->second->notifyEvent(event);
	}

	if ((event == SetMediaSent) || (event == Disconnect) || (event == SetDenied))
	{
		UpdatePlayerList();
	}
}

u16 ClientInterface::getProtocolVersion(u16 peer_id)
{
	JMutexAutoLock conlock(m_clients_mutex);

	// Error check
	std::map<u16, RemoteClient*>::iterator n;
	n = m_clients.find(peer_id);

	// No client to deliver event
	if (n == m_clients.end())
		return 0;

	return n->second->net_proto_version;
}
