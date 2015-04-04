/*
util/msgpack_serialize.h
Copyright (C) 2014 xyz, Ilya Zhuravlev <whatever@xyz.is>
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

#include "../msgpack_fix.h"

#define PACK(x, y) {pk.pack((int)x); pk.pack(y);}
#define MSGPACK_COMMAND -1
#define MSGPACK_PACKET_INIT(id, x) \
	msgpack::sbuffer buffer; \
	msgpack::packer<msgpack::sbuffer> pk(&buffer); \
	pk.pack_map((x)+1); \
	PACK(MSGPACK_COMMAND, id);

#if MSGPACK_VERSION_MAJOR < 1
#include <map>
typedef std::map<int, msgpack::object> MsgpackPacket;
#else
#include <unordered_map>
typedef std::unordered_map<int, msgpack::object> MsgpackPacket;
#endif