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

#include "serverobject.h"
#include <fstream>
#include "inventory.h"
#include "constants.h" // BS
#include "environment.h"

ServerActiveObject::ServerActiveObject(ServerEnvironment *env, v3f pos):
	ActiveObject(0),
	m_static_block(1337,1337,1337),
	m_messages_out(env->m_active_object_messages),
	m_uptime_last(0),
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

ItemStack ServerActiveObject::getWieldedItem() const
{
	const Inventory *inv = getInventory();
	if(inv)
	{
		const InventoryList *list = inv->getList(getWieldList());
		if(list && (getWieldIndex() < (s32)list->getSize())) 
			return list->getItem(getWieldIndex());
	}
	return ItemStack();
}

bool ServerActiveObject::setWieldedItem(const ItemStack &item)
{
	if(Inventory *inv = getInventory()) {
		if (InventoryList *list = inv->getList(getWieldList())) {
			list->changeItem(getWieldIndex(), item);
			return true;
		}
	}
	return false;
}

template<>
bool ServerRegistry::check(ActiveObjectType type)
{
	// These are 0.3 entity types, return without error.
	return ACTIVEOBJECT_TYPE_ITEM <= type && type <= ACTIVEOBJECT_TYPE_MOBV2;
}


ServerRegistry serverRegistry;
