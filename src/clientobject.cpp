/*
clientobject.cpp
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

#include "clientobject.h"
#include "debug.h"
#include "porting.h"
#include "constants.h"

/*
	ClientActiveObject
*/

ClientActiveObject::ClientActiveObject(u16 id, IGameDef *gamedef,
		ClientEnvironment *env):
	ActiveObject(id),
	m_gamedef(gamedef),
	m_env(env)
{
}

ClientActiveObject::~ClientActiveObject()
{
	removeFromScene(true);
}

ClientActiveObject* ClientActiveObject::create(ActiveObjectType type,
		IGameDef *gamedef, ClientEnvironment *env)
{
	// Find factory function
	std::map<u16, Factory>::iterator n;
	n = m_types.find(type);
	if(n == m_types.end()) {
		// If factory is not found, just return.
		dstream<<"WARNING: ClientActiveObject: No factory for type="
				<<(int)type<<std::endl;
		return NULL;
	}

	Factory f = n->second;
	ClientActiveObject *object = (*f)(gamedef, env);
	return object;
}

void ClientActiveObject::registerType(u16 type, Factory f)
{
	std::map<u16, Factory>::iterator n;
	n = m_types.find(type);
	if(n != m_types.end())
		return;
	m_types[type] = f;
}


