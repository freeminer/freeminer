/*
staticobject.h
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

#pragma once

#include "irrlichttypes_bloated.h"
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "debug.h"
#include "threading/concurrent_map.h"

class ServerActiveObject;

struct StaticObject
{
	u8 type = 0;
	v3opos_t pos;
	std::string data;

	StaticObject() = default;
	StaticObject(const ServerActiveObject *s_obj, const v3opos_t &pos_);

	void serialize(std::ostream &os) const;
	bool deSerialize(std::istream &is, u8 version);
};

class StaticObjectList
{
public:
	/*
		Inserts an object to the container.
		Id must be unique (active) or 0 (stored).
	*/
	void insert(u16 id, const StaticObject &obj)
	{
		auto lock = m_active.lock_unique_rec();
		if(id == 0)
		{
			m_stored.push_back(obj);
		}
		else
		{
			if(m_active.find(id) != m_active.end())
			{
				dstream<<"ERROR: StaticObjectList::insert(): "
						<<"id already exists"<<std::endl;
				return;
			}
			m_active.emplace(id, obj);
		}
	}

	void remove(u16 id)
	{
		if (!id)
			return;
		auto lock = m_active.lock_shared_rec();
		if(m_active.find(id) == m_active.end())
		{
			/*
			warningstream<<"StaticObjectList::remove(): id="<<id
					<<" not found"<<std::endl;
			*/
			return;
		}
		m_active.erase(id);
	}

	void serialize(std::ostream &os);
	void deSerialize(std::istream &is);

	/*
		NOTE: When an object is transformed to active, it is removed
		from m_stored and inserted to m_active.
		The caller directly manipulates these containers.
	*/
	std::vector<StaticObject> m_stored;
	concurrent_map<u16, StaticObject> m_active;

private:
};
