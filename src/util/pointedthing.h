/*
util/pointedthing.h
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

#ifndef UTIL_POINTEDTHING_HEADER
#define UTIL_POINTEDTHING_HEADER

#include "../irrlichttypes.h"
#include "../irr_v3d.h"
#include <iostream>
#include <string>
#include "util/msgpack_serialize.h"

enum PointedThingType
{
	POINTEDTHING_NOTHING,
	POINTEDTHING_NODE,
	POINTEDTHING_OBJECT
};

enum PointedThingSerialization {
	POINTEDTHING_TYPE,
	POINTEDTHING_UNDER,
	POINTEDTHING_ABOVE,
	POINTEDTHING_OBJECT_ID
};

struct PointedThing
{
	PointedThingType type;
	v3s16 node_undersurface;
	v3s16 node_abovesurface;
	u16 object_id;

	PointedThing();
	std::string dump() const;
	bool operator==(const PointedThing &pt2) const;
	bool operator!=(const PointedThing &pt2) const;
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const;
	void msgpack_unpack(msgpack::object o);
};

#endif

