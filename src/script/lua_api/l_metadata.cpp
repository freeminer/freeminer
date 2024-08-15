/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

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

#include "lua_api/l_metadata.h"
#include "lua_api/l_internal.h"
#include "common/c_content.h"
#include "serverenvironment.h"
#include "map.h"
#include "server.h"
#include "util/basic_macros.h"

MetaDataRef *MetaDataRef::checkAnyMetadata(lua_State *L, int narg)
{
	void *ud = lua_touserdata(L, narg);

	bool ok = ud && luaL_getmetafield(L, narg, "metadata_class");
	if (ok) {
		ok = lua_isstring(L, -1);
		lua_pop(L, 1);
	}

	if (!ok)
		luaL_typerror(L, narg, "MetaDataRef");

	return *(MetaDataRef **)ud; // unbox pointer
}

int MetaDataRef::gc_object(lua_State *L)
{
	MetaDataRef *o = *(MetaDataRef **)lua_touserdata(L, 1);
	delete o;
	return 0;
}

// Exported functions

// contains(self, name)
int MetaDataRef::l_contains(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);

	IMetadata *meta = ref->getmeta(false);
	if (meta == NULL)
		return 0;

	lua_pushboolean(L, meta->contains(name));
	return 1;
}

// get(self, name)
int MetaDataRef::l_get(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);

	IMetadata *meta = ref->getmeta(false);
	if (meta == NULL)
		return 0;

	std::string str;
	if (meta->getStringToRef(name, str)) {
		lua_pushlstring(L, str.c_str(), str.size());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// get_string(self, name)
int MetaDataRef::l_get_string(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);

	IMetadata *meta = ref->getmeta(false);
	if (meta == NULL) {
		lua_pushlstring(L, "", 0);
		return 1;
	}

	std::string str_;
	const std::string &str = meta->getString(name, &str_);
	lua_pushlstring(L, str.c_str(), str.size());
	return 1;
}

// set_string(self, name, var)
int MetaDataRef::l_set_string(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);
	std::string_view str;
	if (!lua_isnoneornil(L, 3)) {
		str = readParam<std::string_view>(L, 3);
	} else {
		log_deprecated(L, "Value passed to set_string is nil. This behaviour is"
			" undocumented and will result in an error in the future.", 1, true);
	}

	IMetadata *meta = ref->getmeta(!str.empty());
	if (meta != NULL && meta->setString(name, str))
		ref->reportMetadataChange(&name);
	return 0;
}

// get_int(self, name)
int MetaDataRef::l_get_int(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);

	IMetadata *meta = ref->getmeta(false);
	if (meta == NULL) {
		lua_pushnumber(L, 0);
		return 1;
	}

	std::string str_;
	const std::string &str = meta->getString(name, &str_);
	lua_pushnumber(L, stoi(str));
	return 1;
}

// set_int(self, name, var)
int MetaDataRef::l_set_int(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);
	int a = luaL_checkint(L, 3);
	std::string str = itos(a);

	IMetadata *meta = ref->getmeta(true);
	if (meta != NULL && meta->setString(name, str))
		ref->reportMetadataChange(&name);
	return 0;
}

// get_float(self, name)
int MetaDataRef::l_get_float(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);

	IMetadata *meta = ref->getmeta(false);
	if (meta == NULL) {
		lua_pushnumber(L, 0);
		return 1;
	}

	std::string str_;
	const std::string &str = meta->getString(name, &str_);
	// Convert with Lua, as is done in set_float.
	lua_pushlstring(L, str.data(), str.size());
	lua_pushnumber(L, lua_tonumber(L, -1));
	return 1;
}

// set_float(self, name, var)
int MetaDataRef::l_set_float(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	std::string name = luaL_checkstring(L, 2);
	luaL_checknumber(L, 3);
	// Convert number to string with Lua as it gives good precision.
	std::string str = readParam<std::string>(L, 3);

	IMetadata *meta = ref->getmeta(true);
	if (meta != NULL && meta->setString(name, str))
		ref->reportMetadataChange(&name);
	return 0;
}

// get_keys(self)
int MetaDataRef::l_get_keys(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);

	IMetadata *meta = ref->getmeta(false);
	if (meta == NULL) {
		lua_newtable(L);
		return 1;
	}

	std::vector<std::string> keys_;
	const std::vector<std::string> &keys = meta->getKeys(&keys_);

	int i = 0;
	lua_createtable(L, keys.size(), 0);
	for (const std::string &key : keys) {
		lua_pushlstring(L, key.c_str(), key.size());
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

// to_table(self)
int MetaDataRef::l_to_table(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);

	IMetadata *meta = ref->getmeta(true);
	if (meta == NULL) {
		lua_pushnil(L);
		return 1;
	}
	lua_newtable(L);

	ref->handleToTable(L, meta);

	return 1;
}

// from_table(self, table)
int MetaDataRef::l_from_table(lua_State *L)
{
	MAP_LOCK_REQUIRED;

	MetaDataRef *ref = checkAnyMetadata(L, 1);
	int base = 2;

	ref->clearMeta();

	if (!lua_istable(L, base)) {
		// No metadata
		lua_pushboolean(L, true);
		return 1;
	}

	// Create new metadata
	IMetadata *meta = ref->getmeta(true);
	if (meta == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}

	bool was_successful = ref->handleFromTable(L, base, meta);
	ref->reportMetadataChange();
	lua_pushboolean(L, was_successful);
	return 1;
}

void MetaDataRef::handleToTable(lua_State *L, IMetadata *meta)
{
	lua_newtable(L);
	{
		StringMap fields_;
		const StringMap &fields = meta->getStrings(&fields_);
		for (const auto &field : fields) {
			const std::string &name = field.first;
			const std::string &value = field.second;
			lua_pushlstring(L, name.c_str(), name.size());
			lua_pushlstring(L, value.c_str(), value.size());
			lua_settable(L, -3);
		}
	}
	lua_setfield(L, -2, "fields");
}

bool MetaDataRef::handleFromTable(lua_State *L, int table, IMetadata *meta)
{
	// Set fields
	lua_getfield(L, table, "fields");
	if (lua_istable(L, -1)) {
		int fieldstable = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, fieldstable) != 0) {
			// key at index -2 and value at index -1
			std::string name = readParam<std::string>(L, -2);
			auto value = readParam<std::string_view>(L, -1);
			meta->setString(name, value);
			lua_pop(L, 1); // Remove value, keep key for next iteration
		}
		lua_pop(L, 1);
	}

	return true;
}

// equals(self, other)
int MetaDataRef::l_equals(lua_State *L)
{
	MetaDataRef *ref1 = checkAnyMetadata(L, 1);
	IMetadata *data1 = ref1->getmeta(false);
	MetaDataRef *ref2 = checkAnyMetadata(L, 2);
	IMetadata *data2 = ref2->getmeta(false);
	if (data1 == NULL || data2 == NULL)
		lua_pushboolean(L, data1 == data2);
	else
		lua_pushboolean(L, *data1 == *data2);
	return 1;
}

void MetaDataRef::registerMetadataClass(lua_State *L, const char *name,
		const luaL_Reg *methods)
{
	const luaL_Reg metamethods[] = {
		{"__eq", l_equals},
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass(L, name, methods, metamethods);

	// Set metadata_class in the metatable for MetaDataRef::checkAnyMetadata.
	luaL_getmetatable(L, name);
	lua_pushstring(L, name);
	lua_setfield(L, -2, "metadata_class");
	lua_pop(L, 1);
}
