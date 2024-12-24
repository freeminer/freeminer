// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "staticobject.h"
#include "constants.h"
#include "util/serialize.h"
#include "server/serveractiveobject.h"
#include "log_types.h"

StaticObject::StaticObject(const ServerActiveObject *s_obj, const v3f &pos_):
	type(s_obj->getType()),
	pos(pos_)
{
	assert(s_obj->isStaticAllowed());
	s_obj->getStaticData(&data);
}

void StaticObject::serialize(std::ostream &os) const
{
	if (pos.X > MAX_MAP_GENERATION_LIMIT * BS || pos.X > MAX_MAP_GENERATION_LIMIT * BS ||
			pos.Y > MAX_MAP_GENERATION_LIMIT * BS) {
		errorstream << "serialize broken static object: type=" << (int)type
					//<< " p=" << pos 
					<< std::endl;
		return;
	}

	// type
	writeU8(os, type);
	// pos
	writeV3F1000(os, clampToF1000(pos));
	// data
	os<<serializeString16(data);
}

bool StaticObject::deSerialize(std::istream &is, u8 version)
{
	// type
	type = readU8(is);
	// pos
	pos = readV3F1000(is);

	if (pos.X > MAX_MAP_GENERATION_LIMIT * BS || pos.X > MAX_MAP_GENERATION_LIMIT * BS ||
			pos.Y > MAX_MAP_GENERATION_LIMIT * BS) {
		errorstream << "deSerialize broken static object: type=" << (int)type
					//<< " p=" << pos 
					<< std::endl;
		return true;
	}

	// data
	data = deSerializeString16(is);
	return false;
}

void StaticObjectList::serialize(std::ostream &os)
{
	// Check for problems first
	auto problematic = [] (StaticObject &obj) -> bool {
		if (obj.data.size() > U16_MAX) {
			errorstream << "StaticObjectList::serialize(): "
				"object has excessive static data (" << obj.data.size() <<
				"), deleting it." << std::endl;
			return true;
		}
		return false;
	};
	for (auto it = m_stored.begin(); it != m_stored.end(); ) {
		if (problematic(*it))
			it = m_stored.erase(it);
		else
			it++;
	}
	for (auto it = m_active.begin(); it != m_active.end(); ) {
		if (problematic(it->second))
			it = m_active.erase(it);
		else
			it++;
	}

	// version
	u8 version = 0;
	writeU8(os, version);

	// count
	size_t count = m_stored.size() + m_active.size();
	// Make sure it fits into u16, else it would get truncated and cause e.g.
	// issue #2610 (Invalid block data in database: unsupported NameIdMapping version).
	if (count > U16_MAX) {
		warningstream << "StaticObjectList::serialize(): "
			<< "too many objects (" << count << ") in list, "
			<< "not writing them to disk." << std::endl;
		writeU16(os, 0);  // count = 0
		return;
	}
	writeU16(os, count);

	for (StaticObject &s_obj : m_stored) {
		s_obj.serialize(os);
	}

	for (auto &i : m_active) {
		StaticObject s_obj = i.second;
		s_obj.serialize(os);
	}
}

void StaticObjectList::deSerialize(std::istream &is)
{
	if (m_active.size()) {
		errorstream << "StaticObjectList::deSerialize(): "
			<< "deserializing objects while " << m_active.size()
			<< " active objects already exist (not cleared). "
			<< m_stored.size() << " stored objects _were_ cleared"
			<< std::endl;
	}
	m_stored.clear();

	// version
	u8 version = readU8(is);
	// count
	u16 count = readU16(is);

	if (count > 1000) {
		warningstream << "StaticObjectList::deSerialize(): "
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

bool StaticObjectList::storeActiveObject(u16 id)
{
	const auto i = m_active.find(id);
	if (i == m_active.end())
		return false;

	m_stored.push_back(i->second);
	m_active.erase(id);
	return true;
}
