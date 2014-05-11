/*
script/cpp_api/s_item.cpp
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

#include "cpp_api/s_item.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "lua_api/l_item.h"
#include "lua_api/l_inventory.h"
#include "server.h"
#include "log.h"
#include "util/pointedthing.h"
#include "inventory.h"
#include "inventorymanager.h"

bool ScriptApiItem::item_OnDrop(ItemStack &item,
		ServerActiveObject *dropper, v3f pos)
{
	SCRIPTAPI_PRECHECKHEADER

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_drop"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(dropper);
	pushFloatPos(L, pos);
	if (lua_pcall(L, 3, 1, m_errorhandler))
		scriptError();
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L,-1, getServer());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 1);  // Pop item
	return true;
}

bool ScriptApiItem::item_OnPlace(ItemStack &item,
		ServerActiveObject *placer, const PointedThing &pointed)
{
	SCRIPTAPI_PRECHECKHEADER

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_place"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(placer);
	pushPointedThing(pointed);
	if (lua_pcall(L, 3, 1, m_errorhandler))
		scriptError();
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L,-1, getServer());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 1);  // Pop item
	return true;
}

bool ScriptApiItem::item_OnUse(ItemStack &item,
		ServerActiveObject *user, const PointedThing &pointed)
{
	SCRIPTAPI_PRECHECKHEADER

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_use"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(user);
	pushPointedThing(pointed);
	if (lua_pcall(L, 3, 1, m_errorhandler))
		scriptError();
	if(!lua_isnil(L, -1)) {
		try {
			item = read_item(L,-1, getServer());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 1);  // Pop item
	return true;
}

bool ScriptApiItem::item_OnCraft(ItemStack &item, ServerActiveObject *user,
		const InventoryList *old_craft_grid, const InventoryLocation &craft_inv)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "on_craft");
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(user);
	
	// Push inventory list
	std::vector<ItemStack> items;
	for (u32 i = 0; i < old_craft_grid->getSize(); i++) {
		items.push_back(old_craft_grid->getItem(i));
	}
	push_items(L, items);

	InvRef::create(L, craft_inv);
	if (lua_pcall(L, 4, 1, m_errorhandler))
		scriptError();
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L,-1, getServer());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 1);  // Pop item
	return true;
}

bool ScriptApiItem::item_CraftPredict(ItemStack &item, ServerActiveObject *user,
		const InventoryList *old_craft_grid, const InventoryLocation &craft_inv)
{
	SCRIPTAPI_PRECHECKHEADER

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "craft_predict");
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(user);

	//Push inventory list
	std::vector<ItemStack> items;
	for (u32 i = 0; i < old_craft_grid->getSize(); i++) {
		items.push_back(old_craft_grid->getItem(i));
	}
	push_items(L, items);

	InvRef::create(L, craft_inv);
	if (lua_pcall(L, 4, 1, m_errorhandler))
		scriptError();
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L,-1, getServer());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 1);  // Pop item
	return true;
}

// Retrieves core.registered_items[name][callbackname]
// If that is nil or on error, return false and stack is unchanged
// If that is a function, returns true and pushes the
// function onto the stack
// If core.registered_items[name] doesn't exist, core.nodedef_default
// is tried instead so unknown items can still be manipulated to some degree
bool ScriptApiItem::getItemCallback(const char *name, const char *callbackname)
{
	lua_State* L = getStack();

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_items");
	lua_remove(L, -2); // Remove core
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, name);
	lua_remove(L, -2); // Remove registered_items
	// Should be a table
	if(lua_type(L, -1) != LUA_TTABLE)
	{
		// Report error and clean up
		errorstream << "Item \"" << name << "\" not defined" << std::endl;
		lua_pop(L, 1);

		// Try core.nodedef_default instead
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "nodedef_default");
		lua_remove(L, -2);
		luaL_checktype(L, -1, LUA_TTABLE);
	}
	lua_getfield(L, -1, callbackname);
	lua_remove(L, -2); // Remove item def
	// Should be a function or nil
	if (lua_type(L, -1) == LUA_TFUNCTION) {
		return true;
	} else if (!lua_isnil(L, -1)) {
		errorstream << "Item \"" << name << "\" callback \""
			<< callbackname << "\" is not a function" << std::endl;
	}
	lua_pop(L, 1);
	return false;
}

void ScriptApiItem::pushPointedThing(const PointedThing& pointed)
{
	lua_State* L = getStack();

	lua_newtable(L);
	if(pointed.type == POINTEDTHING_NODE)
	{
		lua_pushstring(L, "node");
		lua_setfield(L, -2, "type");
		push_v3s16(L, pointed.node_undersurface);
		lua_setfield(L, -2, "under");
		push_v3s16(L, pointed.node_abovesurface);
		lua_setfield(L, -2, "above");
	}
	else if(pointed.type == POINTEDTHING_OBJECT)
	{
		lua_pushstring(L, "object");
		lua_setfield(L, -2, "type");
		objectrefGet(pointed.object_id);
		lua_setfield(L, -2, "ref");
	}
	else
	{
		lua_pushstring(L, "nothing");
		lua_setfield(L, -2, "type");
	}
}

