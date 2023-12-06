/*
util/pointedthing.cpp
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

#include "pointedthing.h"

#include "serialize.h"
#include "exceptions.h"
#include <sstream>

PointedThing::PointedThing(const v3s16 &under, const v3s16 &above,
	const v3s16 &real_under, const v3f &point, const v3f &normal,
	u16 box_id, f32 distSq):
	type(POINTEDTHING_NODE),
	node_undersurface(under),
	node_abovesurface(above),
	node_real_undersurface(real_under),
	intersection_point(point),
	intersection_normal(normal),
	box_id(box_id),
	distanceSq(distSq)
{}

PointedThing::PointedThing(u16 id, const v3f &point,
  const v3f &normal, const v3f &raw_normal, f32 distSq) :
	type(POINTEDTHING_OBJECT),
	object_id(id),
	intersection_point(point),
	intersection_normal(normal),
	raw_intersection_normal(raw_normal),
	distanceSq(distSq)
{}

std::string PointedThing::dump() const
{
	std::ostringstream os(std::ios::binary);
	switch (type) {
	case POINTEDTHING_NOTHING:
		os << "[nothing]";
		break;
	case POINTEDTHING_NODE:
	{
		const v3s16 &u = node_undersurface;
		const v3s16 &a = node_abovesurface;
		os << "[node under=" << u.X << "," << u.Y << "," << u.Z << " above="
			<< a.X << "," << a.Y << "," << a.Z << "]";
	}
		break;
	case POINTEDTHING_OBJECT:
		os << "[object " << object_id << "]";
		break;
	default:
		os << "[unknown PointedThing]";
	}
	return os.str();
}

void PointedThing::serialize(std::ostream &os) const
{
	writeU8(os, 0); // version
	writeU8(os, (u8)type);
	switch (type) {
	case POINTEDTHING_NOTHING:
		break;
	case POINTEDTHING_NODE:
		writeV3S16(os, node_undersurface);
		writeV3S16(os, node_abovesurface);
		break;
	case POINTEDTHING_OBJECT:
		writeU16(os, object_id);
		break;
	}
}

void PointedThing::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if (version != 0) throw SerializationError(
			"unsupported PointedThing version");
	type = (PointedThingType) readU8(is);
	switch (type) {
	case POINTEDTHING_NOTHING:
		break;
	case POINTEDTHING_NODE:
		node_undersurface = readV3S16(is);
		node_abovesurface = readV3S16(is);
		break;
	case POINTEDTHING_OBJECT:
		object_id = readU16(is);
		break;
	default:
		throw SerializationError("unsupported PointedThingType");
	}
}

void PointedThing::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const {
	static int sizes[3] = {1, 3, 2};
	int t = static_cast<int>(type);
	pk.pack_map(sizes[t]);
	PACK(POINTEDTHING_TYPE, t);
	switch (type) {
	case POINTEDTHING_NOTHING:
		break;
	case POINTEDTHING_NODE:
		PACK(POINTEDTHING_UNDER, node_undersurface);
		PACK(POINTEDTHING_ABOVE, node_abovesurface);
		break;
	case POINTEDTHING_OBJECT:
		PACK(POINTEDTHING_OBJECT_ID, object_id);
		break;
	}
}

void PointedThing::msgpack_unpack(msgpack::object o) {
	int t;
	MsgpackPacket packet = o.as<MsgpackPacket>();
	packet[POINTEDTHING_TYPE].convert(t);
	type = static_cast<PointedThingType>(t);
	switch (type) {
	case POINTEDTHING_NOTHING:
		break;
	case POINTEDTHING_NODE:
		packet[POINTEDTHING_UNDER].convert(node_undersurface);
		packet[POINTEDTHING_ABOVE].convert(node_abovesurface);
		break;
	case POINTEDTHING_OBJECT:
		packet[POINTEDTHING_OBJECT_ID].convert(object_id);
		break;
	default:
		throw SerializationError("unsupported PointedThingType");
	}
}

bool PointedThing::operator==(const PointedThing &pt2) const
{
	if (type != pt2.type)
	{
		return false;
	}
	if (type == POINTEDTHING_NODE)
	{
		if ((node_undersurface != pt2.node_undersurface)
				|| (node_abovesurface != pt2.node_abovesurface)
				|| (node_real_undersurface != pt2.node_real_undersurface))
			return false;
	}
	else if (type == POINTEDTHING_OBJECT)
	{
		if (object_id != pt2.object_id)
			return false;
	}
	return true;
}

bool PointedThing::operator!=(const PointedThing &pt2) const
{
	return !(*this == pt2);
}
