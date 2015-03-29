/*
activeobject.h
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

#ifndef ACTIVEOBJECT_HEADER
#define ACTIVEOBJECT_HEADER

#include "irr_aabb3d.h"
#include <string>

enum ActiveObjectType {
	ACTIVEOBJECT_TYPE_INVALID = 0,
	ACTIVEOBJECT_TYPE_TEST = 1,
// Deprecated stuff
	ACTIVEOBJECT_TYPE_ITEM = 2,
	ACTIVEOBJECT_TYPE_RAT = 3,
	ACTIVEOBJECT_TYPE_OERKKI1 = 4,
	ACTIVEOBJECT_TYPE_FIREFLY = 5,
	ACTIVEOBJECT_TYPE_MOBV2 = 6,
// End deprecated stuff
	ACTIVEOBJECT_TYPE_LUAENTITY = 7,
// Special type, not stored as a static object
	ACTIVEOBJECT_TYPE_PLAYER = 100,
// Special type, only exists as CAO
	ACTIVEOBJECT_TYPE_GENERIC = 101,
};
// Other types are defined in content_object.h

struct ActiveObjectMessage
{
	ActiveObjectMessage(u16 id_, bool reliable_=true, std::string data_=""):
		id(id_),
		reliable(reliable_),
		datastring(data_)
	{}

	u16 id;
	bool reliable;
	std::string datastring;
};

/*
	Parent class for ServerActiveObject and ClientActiveObject
*/
class ActiveObject
{
public:
	ActiveObject(u16 id):
		m_id(id)
	{
	}
	
	u16 getId()
	{
		return m_id;
	}

	void setId(u16 id)
	{
		m_id = id;
	}

	virtual ActiveObjectType getType() const = 0;
	virtual bool getCollisionBox(aabb3f *toset) = 0;
	virtual bool collideWithObjects() = 0;
protected:
	u16 m_id; // 0 is invalid, "no id"
};

#endif

