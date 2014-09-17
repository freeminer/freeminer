/*
script/cpp_api/s_base.h
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

#ifndef S_BASE_H_
#define S_BASE_H_

#include <iostream>
#include <string>

extern "C" {
#include <lua.h>
}

#include "irrlichttypes.h"
#include "jthread/jmutex.h"
#include "jthread/jmutexautolock.h"
#include "common/c_types.h"
#include "common/c_internal.h"

class Server;
class Environment;
class GUIEngine;
class ServerActiveObject;

class ScriptApiBase {
public:

	ScriptApiBase();
	virtual ~ScriptApiBase();

	bool loadMod(const std::string &scriptpath, const std::string &modname);
	bool loadScript(const std::string &scriptpath);

	/* object */
	void addObjectReference(ServerActiveObject *cobj);
	void removeObjectReference(ServerActiveObject *cobj);

protected:
	friend class LuaABM;
	friend class InvRef;
	friend class ObjectRef;
	friend class NodeMetaRef;
	friend class ModApiBase;
	friend class ModApiEnvMod;
	friend class LuaVoxelManip;

	lua_State* getStack()
		{ return m_luastack; }

	void realityCheck();
	void scriptError();
	void stackDump(std::ostream &o);

	Server* getServer() { return m_server; }
	void setServer(Server* server) { m_server = server; }

	Environment* getEnv() { return m_environment; }
	void setEnv(Environment* env) { m_environment = env; }

	GUIEngine* getGuiEngine() { return m_guiengine; }
	void setGuiEngine(GUIEngine* guiengine) { m_guiengine = guiengine; }

	void objectrefGetOrCreate(ServerActiveObject *cobj);
	void objectrefGet(u16 id);

	std::recursive_mutex m_luastackmutex;
	// Stack index of Lua error handler
	int             m_errorhandler;

private:
	lua_State*      m_luastack;

	Server*         m_server;
	Environment*    m_environment;
	GUIEngine*      m_guiengine;
};

#endif /* S_BASE_H_ */
