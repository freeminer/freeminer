// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "inventory.h"
#include "nodemetadata.h"
#include <limits>

// Preserve node state omitted by the standard metadata table representation.
static void fm_push_node_metadata_state(lua_State *L, NodeMetadata *meta)
{
	lua_newtable(L);
	std::vector<std::string> keys;
	int index = 0;
	for (const auto &key : meta->getKeys(&keys)) {
		if (meta->isPrivate(key)) {
			lua_pushlstring(L, key.data(), key.size());
			lua_rawseti(L, -2, ++index);
		}
	}
	lua_setfield(L, -2, "private_fields");

	lua_newtable(L);
	for (const auto *list : meta->getInventory()->getLists()) {
		lua_pushinteger(L, list->getWidth());
		lua_setfield(L, -2, list->getName().c_str());
	}
	lua_setfield(L, -2, "inventory_widths");
}

static void fm_read_node_metadata_state(lua_State *L, int table, NodeMetadata *meta)
{
	lua_getfield(L, table, "private_fields");
	if (lua_istable(L, -1)) {
		for (size_t i = 1; i <= lua_objlen(L, -1); ++i) {
			lua_rawgeti(L, -1, i);
			const char *key = luaL_checkstring(L, -1);
			if (meta->contains(key))
				meta->markPrivate(key, true);
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, table, "inventory_widths");
	if (lua_istable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			const char *key = luaL_checkstring(L, -2);
			const lua_Integer width = luaL_checkinteger(L, -1);
			if (width < 0 ||
					static_cast<uint64_t>(width) > std::numeric_limits<u32>::max())
				luaL_error(L, "Invalid inventory width");
			if (auto *list = meta->getInventory()->getList(key))
				list->setWidth(static_cast<u32>(width));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}
