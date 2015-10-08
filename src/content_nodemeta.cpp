/*
content_nodemeta.cpp
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

#include "content_nodemeta.h"
#include "nodemetadata.h"
#include "nodetimer.h"
#include "inventory.h"
#include "log.h"
#include "serialization.h"
#include "util/serialize.h"
#include "util/string.h"
#include "constants.h" // MAP_BLOCKSIZE
#include <sstream>

#define NODEMETA_GENERIC 1
#define NODEMETA_SIGN 14
#define NODEMETA_CHEST 15
#define NODEMETA_FURNACE 16
#define NODEMETA_LOCKABLE_CHEST 17

// Returns true if node timer must be set
static bool content_nodemeta_deserialize_legacy_body(
		std::istream &is, s16 id, NodeMetadata *meta)
{
	meta->clear();

	if(id == NODEMETA_GENERIC) // GenericNodeMetadata (0.4-dev)
	{
		meta->getInventory()->deSerialize(is);
		deSerializeLongString(is);  // m_text
		deSerializeString(is);  // m_owner

		meta->setString("infotext",deSerializeString(is));
		meta->setString("formspec",deSerializeString(is));
		readU8(is);  // m_allow_text_input
		readU8(is);  // m_allow_removal
		readU8(is);  // m_enforce_owner

		int num_vars = readU32(is);
		for(int i=0; i<num_vars; i++){
			std::string name = deSerializeString(is);
			std::string var = deSerializeLongString(is);
			meta->setString(name, var);
		}
		return false;
	}
	else if(id == NODEMETA_SIGN) // SignNodeMetadata
	{
		meta->setString("text", deSerializeString(is));
		//meta->setString("infotext","\"${text}\"");
		meta->setString("infotext",
				std::string("\"") + meta->getString("text") + "\"");
		meta->setString("formspec","field[text;;${text}]");
		return false;
	}
	else if(id == NODEMETA_CHEST) // ChestNodeMetadata
	{
		meta->getInventory()->deSerialize(is);

		// Rename inventory list "0" to "main"
		Inventory *inv = meta->getInventory();
		if(!inv->getList("main") && inv->getList("0")){
			inv->getList("0")->setName("main");
		}
		assert(inv->getList("main") && !inv->getList("0"));

		meta->setString("formspec","size[8,9]"
				"list[current_name;main;0,0;8,4;]"
				"list[current_player;main;0,5;8,4;]");
		return false;
	}
	else if(id == NODEMETA_LOCKABLE_CHEST) // LockingChestNodeMetadata
	{
		meta->setString("owner", deSerializeString(is));
		meta->getInventory()->deSerialize(is);

		// Rename inventory list "0" to "main"
		Inventory *inv = meta->getInventory();
		if(!inv->getList("main") && inv->getList("0")){
			inv->getList("0")->setName("main");
		}
		assert(inv->getList("main") && !inv->getList("0"));

		meta->setString("formspec","size[8,9]"
				"list[current_name;main;0,0;8,4;]"
				"list[current_player;main;0,5;8,4;]");
		return false;
	}
	else if(id == NODEMETA_FURNACE) // FurnaceNodeMetadata
	{
		meta->getInventory()->deSerialize(is);
		int temp = 0;
		is>>temp;
		meta->setString("fuel_totaltime", ftos((float)temp/10));
		temp = 0;
		is>>temp;
		meta->setString("fuel_time", ftos((float)temp/10));
		temp = 0;
		is>>temp;
		//meta->setString("src_totaltime", ftos((float)temp/10));
		temp = 0;
		is>>temp;
		meta->setString("src_time", ftos((float)temp/10));

		meta->setString("formspec","size[8,9]"
			"list[current_name;fuel;2,3;1,1;]"
			"list[current_name;src;2,1;1,1;]"
			"list[current_name;dst;5,1;2,2;]"
			"list[current_player;main;0,5;8,4;]");
		return true;
	}
	else
	{
		throw SerializationError("Unknown legacy node metadata");
	}
}

static bool content_nodemeta_deserialize_legacy_meta(
		std::istream &is, NodeMetadata *meta)
{
	// Read id
	s16 id = readS16(is);

	// Read data
	std::string data = deSerializeString(is);
	std::istringstream tmp_is(data, std::ios::binary);
	return content_nodemeta_deserialize_legacy_body(tmp_is, id, meta);
}

void content_nodemeta_deserialize_legacy(std::istream &is,
		NodeMetadataList *meta, NodeTimerList *timers,
		IItemDefManager *item_def_mgr)
{
	meta->clear();
	timers->clear();

	u16 version = readU16(is);

	if(version > 1)
	{
		infostream<<__FUNCTION_NAME<<": version "<<version<<" not supported"
				<<std::endl;
		throw SerializationError(__FUNCTION_NAME);
	}

	u16 count = readU16(is);

	for(u16 i=0; i<count; i++)
	{
		u16 p16 = readU16(is);

		v3s16 p(0,0,0);
		p.Z += p16 / MAP_BLOCKSIZE / MAP_BLOCKSIZE;
		p16 -= p.Z * MAP_BLOCKSIZE * MAP_BLOCKSIZE;
		p.Y += p16 / MAP_BLOCKSIZE;
		p16 -= p.Y * MAP_BLOCKSIZE;
		p.X += p16;

		if(meta->get(p) != NULL)
		{
			infostream<<"WARNING: "<<__FUNCTION_NAME<<": "
					<<"already set data at position"
					<<"("<<p.X<<","<<p.Y<<","<<p.Z<<"): Ignoring."
					<<std::endl;
			continue;
		}

		NodeMetadata *data = new NodeMetadata(item_def_mgr);
		bool need_timer = content_nodemeta_deserialize_legacy_meta(is, data);
		meta->set(p, data);

		if(need_timer)
			timers->set(p, NodeTimer(1., 0.));
	}
}

void content_nodemeta_serialize_legacy(std::ostream &os, NodeMetadataList *meta)
{
	// Sorry, I was too lazy to implement this. --kahrl
	writeU16(os, 1); // version
	writeU16(os, 0); // count
}

// END
