/*
staticobject.cpp
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

#include "staticobject.h"
#include "util/serialize.h"
#include "constants.h"
#include "log_types.h"

void StaticObject::serialize(std::ostream &os)
{
	if (pos.X > MAX_MAP_GENERATION_LIMIT * BS || pos.X > MAX_MAP_GENERATION_LIMIT * BS || pos.Y > MAX_MAP_GENERATION_LIMIT * BS) {
		errorstream << "serialize broken static object: type=" << (int)type << " p="<<pos<<std::endl;
		return;
	}
	// type
	writeU8(os, type);
	// pos
	writeV3F1000(os, pos);
	// data
	os<<serializeString(data);
}
bool StaticObject::deSerialize(std::istream &is, u8 version)
{
	// type
	type = readU8(is);
	// pos
	pos = readV3F1000(is);
	if (pos.X > MAX_MAP_GENERATION_LIMIT * BS || pos.X > MAX_MAP_GENERATION_LIMIT * BS || pos.Y > MAX_MAP_GENERATION_LIMIT * BS) {
		errorstream << "deSerialize broken static object: type=" << (int)type << " p="<<pos<<std::endl;
		return true;
	}
	// data
	data = deSerializeString(is);
	return false;
}

void StaticObjectList::serialize(std::ostream &os)
{
	// version
	u8 version = 0;
	writeU8(os, version);

	// count
	size_t count = m_stored.size() + m_active.size();
	// Make sure it fits into u16, else it would get truncated and cause e.g.
	// issue #2610 (Invalid block data in database: unsupported NameIdMapping version).
	if (count > (u16)-1) {
		errorstream << "StaticObjectList::serialize(): "
			<< "too many objects (" << count << ") in list, "
			<< "not writing them to disk." << std::endl;
		writeU16(os, 0);  // count = 0
		return;
	}
	writeU16(os, count);

	for(std::vector<StaticObject>::iterator
			i = m_stored.begin();
			i != m_stored.end(); ++i) {
		StaticObject &s_obj = *i;
		s_obj.serialize(os);
	}
	for(std::map<u16, StaticObject>::iterator
			i = m_active.begin();
			i != m_active.end(); ++i)
	{
		StaticObject s_obj = i->second;
		s_obj.serialize(os);
	}
}
void StaticObjectList::deSerialize(std::istream &is)
{
	// version
	u8 version = readU8(is);
	// count
	u16 count = readU16(is);

	if (count > 1000) {
		errorstream << "StaticObjectList::deSerialize(): "
			<< "too many objects count=" << count << " version="<<(int)version<<" in list, "
			<< "maybe corrupt block." << std::endl;
	}

	for(u16 i = 0; i < count; i++) {
		StaticObject s_obj;
		if (s_obj.deSerialize(is, version))
			return;
		m_stored.push_back(s_obj);
	}
}

