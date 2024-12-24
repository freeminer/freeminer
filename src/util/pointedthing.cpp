// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "pointedthing.h"

#include "serialize.h"
#include "exceptions.h"
#include <sstream>

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
	type = static_cast<PointedThingType>(readU8(is));
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
				|| (node_real_undersurface != pt2.node_real_undersurface)
				|| (pointability != pt2.pointability))
			return false;
	}
	else if (type == POINTEDTHING_OBJECT)
	{
		if (object_id != pt2.object_id || pointability != pt2.pointability)
			return false;
	}
	return true;
}
