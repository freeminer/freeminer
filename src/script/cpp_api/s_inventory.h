/*
script/cpp_api/s_inventory.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef S_INVENTORY_H_
#define S_INVENTORY_H_

#include "cpp_api/s_base.h"

struct ItemStack;

class ScriptApiDetached
		: virtual public ScriptApiBase
{
public:
	/* Detached inventory callbacks */
	// Return number of accepted items to be moved
	int detached_inventory_AllowMove(
			const std::string &name,
			const std::string &from_list, int from_index,
			const std::string &to_list, int to_index,
			int count, ServerActiveObject *player);
	// Return number of accepted items to be put
	int detached_inventory_AllowPut(
			const std::string &name,
			const std::string &listname, int index, ItemStack &stack,
			ServerActiveObject *player);
	// Return number of accepted items to be taken
	int detached_inventory_AllowTake(
			const std::string &name,
			const std::string &listname, int index, ItemStack &stack,
			ServerActiveObject *player);
	// Report moved items
	void detached_inventory_OnMove(
			const std::string &name,
			const std::string &from_list, int from_index,
			const std::string &to_list, int to_index,
			int count, ServerActiveObject *player);
	// Report put items
	void detached_inventory_OnPut(
			const std::string &name,
			const std::string &listname, int index, ItemStack &stack,
			ServerActiveObject *player);
	// Report taken items
	void detached_inventory_OnTake(
			const std::string &name,
			const std::string &listname, int index, ItemStack &stack,
			ServerActiveObject *player);

	void on_detached_inventory_remove_item(
		const std::string &name,
		const std::string &inventory_list_name,
		const ItemStack &deleted_item);

	void on_detached_inventory_change_item(
		const std::string &name,
		const std::string &inventory_list_name,
		u32 query_slot, 
		const ItemStack &old_item,
		const ItemStack &new_item);

	void on_detached_inventory_add_item(
		const std::string &name,
		const std::string &inventory_list_name,
		u32 query_slot, 
		const ItemStack &added_item);

private:
	bool getDetachedInventoryCallback(
			const std::string &name, const char *callbackname);
};



#endif /* S_INVENTORY_H_ */
