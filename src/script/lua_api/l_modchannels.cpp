/*
Minetest
Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#include <cassert>
#include <log.h>
#include "lua_api/l_modchannels.h"
#include "l_internal.h"
#include "modchannels.h"

int ModApiChannels::l_mod_channel_join(lua_State *L)
{
	if (!lua_isstring(L, 1))
		return 0;

	std::string channel = luaL_checkstring(L, 1);
	if (channel.empty())
		return 0;

	getGameDef(L)->joinModChannel(channel);
	assert(getGameDef(L)->getModChannel(channel) != nullptr);
	ModChannelRef::create(L, channel);

	int object = lua_gettop(L);
	lua_pushvalue(L, object);
	return 1;
}

void ModApiChannels::Initialize(lua_State *L, int top)
{
	API_FCT(mod_channel_join);
}

/*
 * ModChannelRef
 */

ModChannelRef::ModChannelRef(const std::string &modchannel) :
		m_modchannel_name(modchannel)
{
}

int ModChannelRef::l_leave(lua_State *L)
{
	ModChannelRef *ref = checkObject<ModChannelRef>(L, 1);
	getGameDef(L)->leaveModChannel(ref->m_modchannel_name);
	return 0;
}

int ModChannelRef::l_send_all(lua_State *L)
{
	ModChannelRef *ref = checkObject<ModChannelRef>(L, 1);
	ModChannel *channel = getobject(L, ref);
	if (!channel || !channel->canWrite())
		return 0;

	// @TODO serialize message
	std::string message = luaL_checkstring(L, 2);

	getGameDef(L)->sendModChannelMessage(channel->getName(), message);
	return 0;
}

int ModChannelRef::l_is_writeable(lua_State *L)
{
	ModChannelRef *ref = checkObject<ModChannelRef>(L, 1);
	ModChannel *channel = getobject(L, ref);
	if (!channel)
		return 0;

	lua_pushboolean(L, channel->canWrite());
	return 1;
}
void ModChannelRef::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass(L, className, methods, metamethods);
}

void ModChannelRef::create(lua_State *L, const std::string &channel)
{
	ModChannelRef *o = new ModChannelRef(channel);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

int ModChannelRef::gc_object(lua_State *L)
{
	ModChannelRef *o = *(ModChannelRef **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}

ModChannel *ModChannelRef::getobject(lua_State *L, ModChannelRef *ref)
{
	return getGameDef(L)->getModChannel(ref->m_modchannel_name);
}

const char ModChannelRef::className[] = "ModChannelRef";
const luaL_Reg ModChannelRef::methods[] = {
	luamethod(ModChannelRef, leave),
	luamethod(ModChannelRef, is_writeable),
	luamethod(ModChannelRef, send_all),
	{0, 0},
};
