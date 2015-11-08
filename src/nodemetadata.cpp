/*
nodemetadata.cpp
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

#include "nodemetadata.h"
#include "exceptions.h"
#include "gamedef.h"
#include "inventory.h"
#include "log.h"
#include "util/serialize.h"
#include "constants.h" // MAP_BLOCKSIZE
#include <sstream>

/*
	NodeMetadata
*/

NodeMetadata::NodeMetadata(IItemDefManager *item_def_mgr):
	m_stringvars(),
	m_inventory(new Inventory(item_def_mgr))
{
}

NodeMetadata::~NodeMetadata()
{
	delete m_inventory;
}

void NodeMetadata::serialize(std::ostream &os) const
{
	int num_vars = m_stringvars.size();
	writeU32(os, num_vars);
	for (StringMap::const_iterator
			it = m_stringvars.begin();
			it != m_stringvars.end(); ++it) {
		os << serializeString(it->first);
		os << serializeLongString(it->second);
	}

	m_inventory->serialize(os);
}

void NodeMetadata::deSerialize(std::istream &is)
{
	m_stringvars.clear();
	int num_vars = readU32(is);
	for(int i=0; i<num_vars; i++){
		std::string name = deSerializeString(is);
		std::string var = deSerializeLongString(is);
		m_stringvars[name] = var;
	}

	m_inventory->deSerialize(is);
}

void NodeMetadata::clear()
{
	m_stringvars.clear();
	m_inventory->clear();
}

/*
	NodeMetadataList
*/

void NodeMetadataList::serialize(std::ostream &os) const
{
	/*
		Version 0 is a placeholder for "nothing to see here; go away."
	*/

	if(m_data.empty()){
		writeU8(os, 0); // version
		return;
	}

	writeU8(os, 1); // version

	u16 count = m_data.size();
	writeU16(os, count);

	for(std::map<v3s16, NodeMetadata*>::const_iterator
			i = m_data.begin();
			i != m_data.end(); ++i)
	{
		v3s16 p = i->first;
		NodeMetadata *data = i->second;

		u16 p16 = p.Z * MAP_BLOCKSIZE * MAP_BLOCKSIZE + p.Y * MAP_BLOCKSIZE + p.X;
		writeU16(os, p16);

		data->serialize(os);
	}
}

void NodeMetadataList::deSerialize(std::istream &is, IItemDefManager *item_def_mgr)
{
	clear();

	u8 version = readU8(is);

	if (version == 0) {
		// Nothing
		return;
	}

	if (version != 1) {
		std::string err_str = std::string(FUNCTION_NAME)
			+ ": version " + itos(version) + " not supported";
		infostream << err_str << std::endl;
		throw SerializationError(err_str);
	}

	u16 count = readU16(is);

	for (u16 i=0; i < count; i++) {
		u16 p16 = readU16(is);

		v3s16 p;
		p.Z = p16 / MAP_BLOCKSIZE / MAP_BLOCKSIZE;
		p16 &= MAP_BLOCKSIZE * MAP_BLOCKSIZE - 1;
		p.Y = p16 / MAP_BLOCKSIZE;
		p16 &= MAP_BLOCKSIZE - 1;
		p.X = p16;

		if (m_data.find(p) != m_data.end()) {
			warningstream<<"NodeMetadataList::deSerialize(): "
					<<"already set data at position"
					<<"("<<p.X<<","<<p.Y<<","<<p.Z<<"): Ignoring."
					<<std::endl;
			continue;
		}

		NodeMetadata *data = new NodeMetadata(item_def_mgr);
		data->deSerialize(is);
		m_data[p] = data;
	}
}

NodeMetadataList::~NodeMetadataList()
{
	clear();
}

std::vector<v3s16> NodeMetadataList::getAllKeys()
{
	std::vector<v3s16> keys;

	std::map<v3s16, NodeMetadata *>::const_iterator it;
	for (it = m_data.begin(); it != m_data.end(); ++it)
		keys.push_back(it->first);

	return keys;
}

NodeMetadata *NodeMetadataList::get(v3s16 p)
{
	std::map<v3s16, NodeMetadata *>::const_iterator n = m_data.find(p);
	if (n == m_data.end())
		return NULL;
	return n->second;
}

void NodeMetadataList::remove(v3s16 p)
{
	NodeMetadata *olddata = get(p);
	if (olddata) {
		m_data.erase(p);
		delete olddata;
	}
}

void NodeMetadataList::set(v3s16 p, NodeMetadata *d)
{
	remove(p);
	m_data.insert(std::make_pair(p, d));
}

void NodeMetadataList::clear()
{
	std::map<v3s16, NodeMetadata*>::iterator it;
	for (it = m_data.begin(); it != m_data.end(); ++it) {
		delete it->second;
	}
	m_data.clear();
}

std::string NodeMetadata::getString(const std::string &name,
	unsigned short recursion) const
{
	StringMap::const_iterator it = m_stringvars.find(name);
	if (it == m_stringvars.end())
		return "";

	return resolveString(it->second, recursion);
}

void NodeMetadata::setString(const std::string &name, const std::string &var)
{
	if (var.empty()) {
		m_stringvars.erase(name);
	} else {
		m_stringvars[name] = var;
	}
}

std::string NodeMetadata::resolveString(const std::string &str,
	unsigned short recursion) const
{
	if (recursion > 1) {
		return str;
	}
	if (str.substr(0, 2) == "${" && str[str.length() - 1] == '}') {
		return getString(str.substr(2, str.length() - 3), recursion + 1);
	}
	return str;
}

