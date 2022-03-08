/*
serverobject.cpp
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

#include "serveractiveobject.h"
#include <fstream>
#include "inventory.h"
#include "constants.h" // BS
<<<<<<< HEAD:src/serverobject.cpp
#include "environment.h"

Queue<ActiveObjectMessage> dummy_queue;

ServerActiveObject::ServerActiveObject(ServerEnvironment *env, v3f pos):
	ActiveObject(0),
	m_static_block(1337,1337,1337),
	m_messages_out(env ? env->m_active_object_messages : dummy_queue),
	m_uptime_last(0),
=======
#include "log.h"

ServerActiveObject::ServerActiveObject(ServerEnvironment *env, v3f pos):
	ActiveObject(0),
>>>>>>> 5.5.0:src/server/serveractiveobject.cpp
	m_env(env),
	m_base_position(pos)
{
	m_pending_deactivation = false;
	m_removed = false;
	m_static_exists = false;
	m_known_by_count = 0;
}

float ServerActiveObject::getMinimumSavedMovement()
{
	return 2.0*BS;
}

<<<<<<< HEAD:src/serverobject.cpp
ItemStack ServerActiveObject::getWieldedItem()
{
	auto lock = lock_shared_rec();
	const Inventory *inv = getInventory();
	if(inv)
	{
		const InventoryList *list = inv->getList(getWieldList());
		if(list && (getWieldIndex() < (s32)list->getSize())) 
			return list->getItem(getWieldIndex());
	}
=======
ItemStack ServerActiveObject::getWieldedItem(ItemStack *selected, ItemStack *hand) const
{
	*selected = ItemStack();
	if (hand)
		*hand = ItemStack();

>>>>>>> 5.5.0:src/server/serveractiveobject.cpp
	return ItemStack();
}

bool ServerActiveObject::setWieldedItem(const ItemStack &item)
{
<<<<<<< HEAD:src/serverobject.cpp
	auto lock = lock_unique_rec();
	if(Inventory *inv = getInventory()) {
		if (InventoryList *list = inv->getList(getWieldList())) {
			list->changeItem(getWieldIndex(), item);
			return true;
		}
	}
=======
>>>>>>> 5.5.0:src/server/serveractiveobject.cpp
	return false;
}

std::string ServerActiveObject::generateUpdateInfantCommand(u16 infant_id, u16 protocol_version)
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_SPAWN_INFANT);
	// parameters
	writeU16(os, infant_id);
	writeU8(os, getSendType());
	if (protocol_version < 38) {
		// Clients since 4aa9a66 so no longer need this data
		// Version 38 is the first bump after that commit.
		// See also: ClientEnvironment::addActiveObject
		os << serializeString32(getClientInitializationData(protocol_version));
	}
	return os.str();
}

void ServerActiveObject::dumpAOMessagesToQueue(std::queue<ActiveObjectMessage> &queue)
{
	while (!m_messages_out.empty()) {
		queue.push(std::move(m_messages_out.front()));
		m_messages_out.pop();
	}
}

void ServerActiveObject::markForRemoval()
{
	if (!m_pending_removal) {
		onMarkedForRemoval();
		m_pending_removal = true;
	}
}

void ServerActiveObject::markForDeactivation()
{
	if (!m_pending_deactivation) {
		onMarkedForDeactivation();
		m_pending_deactivation = true;
	}
}
