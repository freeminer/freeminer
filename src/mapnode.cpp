/*
mapnode.cpp
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

#include "irrlichttypes_extrabloated.h"
#include "mapnode.h"
#include "porting.h"
#include "main.h" // For g_settings
#include "nodedef.h"
#include "content_mapnode.h" // For mapnode_translate_*_internal
#include "serialization.h" // For ser_ver_supported
#include "util/serialize.h"
#include "log.h"
#include "util/numeric.h"
#include <string>
#include <sstream>

static const Rotation wallmounted_to_rot[] = {
	ROTATE_0, ROTATE_180, ROTATE_90, ROTATE_270
};

static const u8 rot_to_wallmounted[] = {
	2, 4, 3, 5
};


/*
	MapNode
*/

// Create directly from a nodename
// If name is unknown, sets CONTENT_IGNORE
MapNode::MapNode(INodeDefManager *ndef, const std::string &name,
		u8 a_param1, u8 a_param2)
{
	content_t id = CONTENT_IGNORE;
	ndef->getId(name, id);
	param0 = id;
	param1 = a_param1;
	param2 = a_param2;
}

void MapNode::setLight(enum LightBank bank, u8 a_light, INodeDefManager *nodemgr)
{
	// If node doesn't contain light data, ignore this
	if(nodemgr->get(*this).param_type != CPT_LIGHT)
		return;
	if(bank == LIGHTBANK_DAY)
	{
		param1 &= 0xf0;
		param1 |= a_light & 0x0f;
	}
	else if(bank == LIGHTBANK_NIGHT)
	{
		param1 &= 0x0f;
		param1 |= (a_light & 0x0f)<<4;
	}
	else
		assert(0);
}

u8 MapNode::getLight(enum LightBank bank, INodeDefManager *nodemgr) const
{
	// Select the brightest of [light source, propagated light]
	const ContentFeatures &f = nodemgr->get(*this);

	u8 light;
	if(f.param_type == CPT_LIGHT)
		light = bank == LIGHTBANK_DAY ? param1 & 0x0f : (param1 >> 4) & 0x0f;
	else
		light = 0;

	return MYMAX(f.light_source, light);
}

u8 MapNode::getLightNoChecks(enum LightBank bank, const ContentFeatures *f) const
{
	return MYMAX(f->light_source,
	             bank == LIGHTBANK_DAY ? param1 & 0x0f : (param1 >> 4) & 0x0f);
}

bool MapNode::getLightBanks(u8 &lightday, u8 &lightnight, INodeDefManager *nodemgr) const
{
	// Select the brightest of [light source, propagated light]
	const ContentFeatures &f = nodemgr->get(*this);
	if(f.param_type == CPT_LIGHT)
	{
		lightday = param1 & 0x0f;
		lightnight = (param1>>4)&0x0f;
	}
	else
	{
		lightday = 0;
		lightnight = 0;
	}
	if(f.light_source > lightday)
		lightday = f.light_source;
	if(f.light_source > lightnight)
		lightnight = f.light_source;
	return f.param_type == CPT_LIGHT || f.light_source != 0;
}

u8 MapNode::getFaceDir(INodeDefManager *nodemgr) const
{
	const ContentFeatures &f = nodemgr->get(*this);
	if(f.param_type_2 == CPT2_FACEDIR)
		return (getParam2() & 0x1F) % 24;
	return 0;
}

u8 MapNode::getWallMounted(INodeDefManager *nodemgr) const
{
	const ContentFeatures &f = nodemgr->get(*this);
	if(f.param_type_2 == CPT2_WALLMOUNTED)
		return getParam2() & 0x07;
	return 0;
}

v3s16 MapNode::getWallMountedDir(INodeDefManager *nodemgr) const
{
	switch(getWallMounted(nodemgr))
	{
	case 0: default: return v3s16(0,1,0);
	case 1: return v3s16(0,-1,0);
	case 2: return v3s16(1,0,0);
	case 3: return v3s16(-1,0,0);
	case 4: return v3s16(0,0,1);
	case 5: return v3s16(0,0,-1);
	}
}

void MapNode::rotateAlongYAxis(INodeDefManager *nodemgr, Rotation rot) {
	ContentParamType2 cpt2 = nodemgr->get(*this).param_type_2;

	if (cpt2 == CPT2_FACEDIR) {
		u8 newrot = param2 & 3;
		param2 &= ~3;
		param2 |= (newrot + rot) & 3;
	} else if (cpt2 == CPT2_WALLMOUNTED) {
		u8 wmountface = (param2 & 7);
		if (wmountface <= 1)
			return;

		Rotation oldrot = wallmounted_to_rot[wmountface - 2];
		param2 &= ~7;
		param2 |= rot_to_wallmounted[(oldrot - rot) & 3];
	}
}

static std::vector<aabb3f> transformNodeBox(const MapNode &n,
		const NodeBox &nodebox, INodeDefManager *nodemgr)
{
	std::vector<aabb3f> boxes;
	if(nodebox.type == NODEBOX_FIXED || nodebox.type == NODEBOX_LEVELED)
	{
		const std::vector<aabb3f> &fixed = nodebox.fixed;
		int facedir = n.getFaceDir(nodemgr);
		u8 axisdir = facedir>>2;
		facedir&=0x03;
		for(std::vector<aabb3f>::const_iterator
				i = fixed.begin();
				i != fixed.end(); i++)
		{
			aabb3f box = *i;

			if (nodebox.type == NODEBOX_LEVELED) {
				box.MaxEdge.Y = -BS/2 + BS*((float)1/LEVELED_MAX) * n.getLevel(nodemgr);
			}

			switch (axisdir)
			{
			case 0:
				if(facedir == 1)
				{
					box.MinEdge.rotateXZBy(-90);
					box.MaxEdge.rotateXZBy(-90);
				}
				else if(facedir == 2)
				{
					box.MinEdge.rotateXZBy(180);
					box.MaxEdge.rotateXZBy(180);
				}
				else if(facedir == 3)
				{
					box.MinEdge.rotateXZBy(90);
					box.MaxEdge.rotateXZBy(90);
				}
				break;
			case 1: // z+
				box.MinEdge.rotateYZBy(90);
				box.MaxEdge.rotateYZBy(90);
				if(facedir == 1)
				{
					box.MinEdge.rotateXYBy(90);
					box.MaxEdge.rotateXYBy(90);
				}
				else if(facedir == 2)
				{
					box.MinEdge.rotateXYBy(180);
					box.MaxEdge.rotateXYBy(180);
				}
				else if(facedir == 3)
				{
					box.MinEdge.rotateXYBy(-90);
					box.MaxEdge.rotateXYBy(-90);
				}
				break;
			case 2: //z-
				box.MinEdge.rotateYZBy(-90);
				box.MaxEdge.rotateYZBy(-90);
				if(facedir == 1)
				{
					box.MinEdge.rotateXYBy(-90);
					box.MaxEdge.rotateXYBy(-90);
				}
				else if(facedir == 2)
				{
					box.MinEdge.rotateXYBy(180);
					box.MaxEdge.rotateXYBy(180);
				}
				else if(facedir == 3)
				{
					box.MinEdge.rotateXYBy(90);
					box.MaxEdge.rotateXYBy(90);
				}
				break;
			case 3:  //x+
				box.MinEdge.rotateXYBy(-90);
				box.MaxEdge.rotateXYBy(-90);
				if(facedir == 1)
				{
					box.MinEdge.rotateYZBy(90);
					box.MaxEdge.rotateYZBy(90);
				}
				else if(facedir == 2)
				{
					box.MinEdge.rotateYZBy(180);
					box.MaxEdge.rotateYZBy(180);
				}
				else if(facedir == 3)
				{
					box.MinEdge.rotateYZBy(-90);
					box.MaxEdge.rotateYZBy(-90);
				}
				break;
			case 4:  //x-
				box.MinEdge.rotateXYBy(90);
				box.MaxEdge.rotateXYBy(90);
				if(facedir == 1)
				{
					box.MinEdge.rotateYZBy(-90);
					box.MaxEdge.rotateYZBy(-90);
				}
				else if(facedir == 2)
				{
					box.MinEdge.rotateYZBy(180);
					box.MaxEdge.rotateYZBy(180);
				}
				else if(facedir == 3)
				{
					box.MinEdge.rotateYZBy(90);
					box.MaxEdge.rotateYZBy(90);
				}
				break;
			case 5:
				box.MinEdge.rotateXYBy(-180);
				box.MaxEdge.rotateXYBy(-180);
				if(facedir == 1)
				{
					box.MinEdge.rotateXZBy(90);
					box.MaxEdge.rotateXZBy(90);
				}
				else if(facedir == 2)
				{
					box.MinEdge.rotateXZBy(180);
					box.MaxEdge.rotateXZBy(180);
				}
				else if(facedir == 3)
				{
					box.MinEdge.rotateXZBy(-90);
					box.MaxEdge.rotateXZBy(-90);
				}
				break;
			default:
				break;
			}
			box.repair();
			boxes.push_back(box);
		}
	}
	else if(nodebox.type == NODEBOX_WALLMOUNTED)
	{
		v3s16 dir = n.getWallMountedDir(nodemgr);

		// top
		if(dir == v3s16(0,1,0))
		{
			boxes.push_back(nodebox.wall_top);
		}
		// bottom
		else if(dir == v3s16(0,-1,0))
		{
			boxes.push_back(nodebox.wall_bottom);
		}
		// side
		else
		{
			v3f vertices[2] =
			{
				nodebox.wall_side.MinEdge,
				nodebox.wall_side.MaxEdge
			};

			for(s32 i=0; i<2; i++)
			{
				if(dir == v3s16(-1,0,0))
					vertices[i].rotateXZBy(0);
				if(dir == v3s16(1,0,0))
					vertices[i].rotateXZBy(180);
				if(dir == v3s16(0,0,-1))
					vertices[i].rotateXZBy(90);
				if(dir == v3s16(0,0,1))
					vertices[i].rotateXZBy(-90);
			}

			aabb3f box = aabb3f(vertices[0]);
			box.addInternalPoint(vertices[1]);
			boxes.push_back(box);
		}
	}
	else // NODEBOX_REGULAR
	{
		const ContentFeatures &f = nodemgr->get(n);
		float top = BS/2;
		if (f.param_type_2 == CPT2_LEVELED || f.param_type_2 == CPT2_FLOWINGLIQUID)
			top = -BS/2 + BS*((float)1/f.getMaxLevel()) * n.getLevel(nodemgr);

		boxes.push_back(aabb3f(-BS/2,-BS/2,-BS/2,BS/2,top,BS/2));
	}
	return boxes;
}

std::vector<aabb3f> MapNode::getNodeBoxes(INodeDefManager *nodemgr) const
{
	const ContentFeatures &f = nodemgr->get(*this);
	return transformNodeBox(*this, f.node_box, nodemgr);
}

std::vector<aabb3f> MapNode::getCollisionBoxes(INodeDefManager *nodemgr) const
{
	const ContentFeatures &f = nodemgr->get(*this);
	if (f.collision_box.fixed.empty())
		return transformNodeBox(*this, f.node_box, nodemgr);
	else
		return transformNodeBox(*this, f.collision_box, nodemgr);
}

std::vector<aabb3f> MapNode::getSelectionBoxes(INodeDefManager *nodemgr) const
{
	const ContentFeatures &f = nodemgr->get(*this);
	return transformNodeBox(*this, f.selection_box, nodemgr);
}

u8 MapNode::getMaxLevel(INodeDefManager *nodemgr, bool compress) const
{
	return nodemgr->get(*this).getMaxLevel(compress);
}

u8 MapNode::getLevel(INodeDefManager *nodemgr) const
{
	const ContentFeatures &f = nodemgr->get(*this);
	if (f.param_type_2 == CPT2_LEVELED) {
		u8 level = getParam2() & LEVELED_MASK;
		if(level)
			return level;
	} 
	if(f.leveled) {
		if(f.leveled > LEVELED_MAX)
			return LEVELED_MAX;
		//if(f.leveled > f.getMaxLevel()) return f.getMaxLevel();
		return f.leveled; //default
	}
	if(f.liquid_type == LIQUID_SOURCE) {
		if (nodemgr->get(nodemgr->getId(f.liquid_alternative_flowing)).param_type_2 == CPT2_LEVELED)
			return LEVELED_MAX;
		return LIQUID_LEVEL_SOURCE;
	}
	if (f.param_type_2 == CPT2_FLOWINGLIQUID || f.liquid_type == LIQUID_FLOWING) //remove liquid_type later
		return getParam2() & LIQUID_LEVEL_MASK;
	return 0;
}

u8 MapNode::setLevel(INodeDefManager *nodemgr, s8 level, bool compress)
{
	u8 rest = 0;
	if (level < 1) {
		setContent(CONTENT_AIR);
		return 0;
	}
	const ContentFeatures &f = nodemgr->get(*this);
	if (f.param_type_2 == CPT2_LEVELED) {
		if (level > f.getMaxLevel(compress)) {
			rest = level - f.getMaxLevel(compress);
			level = f.getMaxLevel(compress);
		}
		if (level >= f.getMaxLevel()) {
			if (!f.liquid_alternative_source.empty())
				setContent(nodemgr->getId(f.liquid_alternative_source));
		} else if (!f.liquid_alternative_flowing.empty()) {
			setContent(nodemgr->getId(f.liquid_alternative_flowing));
		}
		setParam2(level & LEVELED_MASK);
		//debug: if(getLevel(nodemgr)!=level) errorstream<<"AFTERSET not match want="<<level<< " res="<< getLevel(nodemgr) <<std::endl;
	} else if (f.param_type_2 == CPT2_FLOWINGLIQUID
		|| f.liquid_type == LIQUID_FLOWING
		|| f.liquid_type == LIQUID_SOURCE) {
		if (level >= LIQUID_LEVEL_SOURCE) {
			rest = level - LIQUID_LEVEL_SOURCE;
			setContent(nodemgr->getId(f.liquid_alternative_source));
		} else {
			setContent(nodemgr->getId(f.liquid_alternative_flowing));
			setParam2(level & LIQUID_LEVEL_MASK);
		}
	}
	return rest;
}

u8 MapNode::addLevel(INodeDefManager *nodemgr, s8 add, bool compress)
{
	s8 level = getLevel(nodemgr);
	if (add == 0) level = 1;
	level += add;
	return setLevel(nodemgr, level, compress);
}

int MapNode::freeze_melt(INodeDefManager *ndef, int direction) {
	content_t to = ndef->getId(direction < 0 ? ndef->get(*this).freeze : ndef->get(*this).melt);
	if (to == CONTENT_IGNORE)
		return 0;
	u8 level_was_max = this->getMaxLevel(ndef);
	u8 level_was = this->getLevel(ndef);
	this->setContent(to);
	u8 level_now_max = this->getMaxLevel(ndef);
	if (level_was_max && level_was_max != level_now_max) {
		u8 want = (float)level_now_max / level_was_max * level_was;
		if (!want)
			want = 1;
		if (want != level_was)
			this->setLevel(ndef, want);
	}
	if (this->getMaxLevel(ndef) && !this->getLevel(ndef))
		this->addLevel(ndef);
	return direction;
}

u32 MapNode::serializedLength(u8 version)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	if(version == 0)
		return 1;
	else if(version <= 9)
		return 2;
	else if(version <= 23)
		return 3;
	else
		return 4;
}
void MapNode::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	pk.pack_array(3);
	pk.pack(param0);
	pk.pack(param1);
	pk.pack(param2);
}
void MapNode::msgpack_unpack(msgpack::object o)
{
	std::vector<int> data;
	o.convert(&data);
	if (data.size() < 3)
		throw msgpack::type_error();

	param0 = data[0];
	param1 = data[1];
	param2 = data[2];
}
void MapNode::serialize(u8 *dest, u8 version)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	// Can't do this anymore; we have 16-bit dynamically allocated node IDs
	// in memory; conversion just won't work in this direction.
	if(version < 24)
		throw SerializationError("MapNode::serialize: serialization to "
				"version < 24 not possible");

	writeU16(dest+0, param0);
	writeU8(dest+2, param1);
	writeU8(dest+3, param2);
}
void MapNode::deSerialize(u8 *source, u8 version)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	if(version <= 21)
	{
		deSerialize_pre22(source, version);
		return;
	}

	if(version >= 24){
		param0 = readU16(source+0);
		param1 = readU8(source+2);
		param2 = readU8(source+3);
	}else{
		param0 = readU8(source+0);
		param1 = readU8(source+1);
		param2 = readU8(source+2);
		if(param0 > 0x7F){
			param0 |= ((param2&0xF0)<<4);
			param2 &= 0x0F;
		}
	}
}
void MapNode::serializeBulk(std::ostream &os, int version,
		const MapNode *nodes, u32 nodecount,
		u8 content_width, u8 params_width, bool compressed)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	assert(content_width == 2);
	assert(params_width == 2);

	// Can't do this anymore; we have 16-bit dynamically allocated node IDs
	// in memory; conversion just won't work in this direction.
	if(version < 24)
		throw SerializationError("MapNode::serializeBulk: serialization to "
				"version < 24 not possible");

	SharedBuffer<u8> databuf(nodecount * (content_width + params_width));

	// Serialize content
	for(u32 i=0; i<nodecount; i++)
		writeU16(&databuf[i*2], nodes[i].param0);

	// Serialize param1
	u32 start1 = content_width * nodecount;
	for(u32 i=0; i<nodecount; i++)
		writeU8(&databuf[start1 + i], nodes[i].param1);

	// Serialize param2
	u32 start2 = (content_width + 1) * nodecount;
	for(u32 i=0; i<nodecount; i++)
		writeU8(&databuf[start2 + i], nodes[i].param2);

	/*
		Compress data to output stream
	*/

	if(compressed)
	{
		compressZlib(databuf, os);
	}
	else
	{
		os.write((const char*) &databuf[0], databuf.getSize());
	}
}

// Deserialize bulk node data
void MapNode::deSerializeBulk(std::istream &is, int version,
		MapNode *nodes, u32 nodecount,
		u8 content_width, u8 params_width, bool compressed)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapNode format not supported");

	assert(version >= 22);
	assert(content_width == 1 || content_width == 2);
	assert(params_width == 2);

	// Uncompress or read data
	u32 len = nodecount * (content_width + params_width);
	SharedBuffer<u8> databuf(len);
	if(compressed)
	{
		std::ostringstream os(std::ios_base::binary);
		decompressZlib(is, os);
		std::string s = os.str();
		if(s.size() != len)
			throw SerializationError("deSerializeBulkNodes: "
					"decompress resulted in invalid size");
		memcpy(&databuf[0], s.c_str(), len);
	}
	else
	{
		is.read((char*) &databuf[0], len);
		if(is.eof() || is.fail())
			throw SerializationError("deSerializeBulkNodes: "
					"failed to read bulk node data");
	}

	// Deserialize content
	if(content_width == 1)
	{
		for(u32 i=0; i<nodecount; i++)
			nodes[i].param0 = readU8(&databuf[i]);
	}
	else if(content_width == 2)
	{
		for(u32 i=0; i<nodecount; i++)
			nodes[i].param0 = readU16(&databuf[i*2]);
	}

	// Deserialize param1
	u32 start1 = content_width * nodecount;
	for(u32 i=0; i<nodecount; i++)
		nodes[i].param1 = readU8(&databuf[start1 + i]);

	// Deserialize param2
	u32 start2 = (content_width + 1) * nodecount;
	if(content_width == 1)
	{
		for(u32 i=0; i<nodecount; i++) {
			nodes[i].param2 = readU8(&databuf[start2 + i]);
			if(nodes[i].param0 > 0x7F){
				nodes[i].param0 <<= 4;
				nodes[i].param0 |= (nodes[i].param2&0xF0)>>4;
				nodes[i].param2 &= 0x0F;
			}
		}
	}
	else if(content_width == 2)
	{
		for(u32 i=0; i<nodecount; i++)
			nodes[i].param2 = readU8(&databuf[start2 + i]);
	}
}

/*
	Legacy serialization
*/
void MapNode::deSerialize_pre22(u8 *source, u8 version)
{
	if(version <= 1)
	{
		param0 = source[0];
	}
	else if(version <= 9)
	{
		param0 = source[0];
		param1 = source[1];
	}
	else
	{
		param0 = source[0];
		param1 = source[1];
		param2 = source[2];
		if(param0 > 0x7f){
			param0 <<= 4;
			param0 |= (param2&0xf0)>>4;
			param2 &= 0x0f;
		}
	}

	// Convert special values from old version to new
	if(version <= 19)
	{
		// In these versions, CONTENT_IGNORE and CONTENT_AIR
		// are 255 and 254
		// Version 19 is fucked up with sometimes the old values and sometimes not
		if(param0 == 255)
			param0 = CONTENT_IGNORE;
		else if(param0 == 254)
			param0 = CONTENT_AIR;
	}

	// Translate to our known version
	*this = mapnode_translate_to_internal(*this, version);
}
