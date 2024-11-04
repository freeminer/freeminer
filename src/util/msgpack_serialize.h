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

#include <cstdint>
#include "../msgpack_fix.h"

#include "../serialization.h" //decompressZlib

using packet_field_t = uint8_t;

#define PACK(x, y)                                                                       \
	{                                                                                    \
		pk.pack((packet_field_t)x);                                                      \
		pk.pack(y);                                                                      \
	}

#define PACK_ZIP(x, y)                                                                   \
	{                                                                                    \
		msgpack::sbuffer buffer_zip;                                                     \
		msgpack::packer<msgpack::sbuffer> pk_zip(&buffer_zip);                           \
		pk_zip.pack(y);                                                                  \
		std::string s;                                                                   \
		compressZlib(std::string(buffer_zip.data(), buffer_zip.size()), s);              \
		PACK(x, s);                                                                      \
	}

#define MSGPACK_COMMAND -1
#define MSGPACK_PACKET_INIT(id, x)                                                       \
	msgpack::sbuffer buffer;                                                             \
	msgpack::packer<msgpack::sbuffer> pk(&buffer);                                       \
	pk.pack_map((x) + 1);                                                                \
	PACK(MSGPACK_COMMAND, id);

#if MSGPACK_VERSION_MAJOR < 1
#include <map>
typedef std::map<packet_field_t, msgpack::object> MsgpackPacket;
#else
#include <unordered_map>
typedef std::unordered_map<packet_field_t, msgpack::object> MsgpackPacket;
#endif

template <typename T>
bool packet_convert_safe(const MsgpackPacket &packet, packet_field_t field, T &to)
{
	const auto it = packet.find(field);
	if (it == packet.end()) {
		return false;
	}
	it->second.convert(to);
	return true;
}

template <typename T>
bool packet_convert_safe_zip(const MsgpackPacket &packet, packet_field_t field, T &to)
{
	const auto it = packet.find(field);
	if (it == packet.end()) {
		return false;
	}
	try {
		std::string sz, s;
		it->second.convert(sz);
		decompressZlib(sz, s);
		msgpack::unpacked msg;
		msgpack::unpack(msg, s.c_str(), s.size());
		msgpack::object obj = msg.get();
		obj.convert(to);
	} catch (...) {
		return false;
	}
	return true;
}

class MsgpackPacketSafe : public MsgpackPacket
{
public:
	template <typename T>
	bool convert_safe(packet_field_t field, T &to)
	{
		return packet_convert_safe(*this, field, to);
	}
};
