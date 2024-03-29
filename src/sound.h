/*
sound.h
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

#pragma once

#include <set>
#include <string>
#include "util/serialize.h"
#include "irrlichttypes_bloated.h"

// fm:
#include "msgpack_fix.h"
#include "network/connection.h"
#include "util/msgpack_serialize.h"


enum {
	SOUNDSPEC_NAME,
	SOUNDSPEC_GAIN,
	SOUNDSPEC_PITCH,
	SOUNDSPEC_FADE
};


// This class describes the basic sound information for playback.
// Positional handling is done separately.

struct SimpleSoundSpec
{
	SimpleSoundSpec(const std::string &name = "", float gain = 1.0f,
			bool loop = false, float fade = 0.0f, float pitch = 1.0f) :
			name(name), gain(gain), fade(fade), pitch(pitch), loop(loop)
	{
	}

	bool exists() const { return !name.empty(); }

	void serialize(std::ostream &os, u16 protocol_version) const
	{
		os << serializeString16(name);
		writeF32(os, gain);
		writeF32(os, pitch);
		writeF32(os, fade);
	}

	void deSerialize(std::istream &is, u16 protocol_version)
	{
		name = deSerializeString16(is);
		gain = readF32(is);
		pitch = readF32(is);
		fade = readF32(is);
	}

	std::string name;
	float gain = 1.0f;
	float fade = 0.0f;
	float pitch = 1.0f;

//fm:
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const {
		pk.pack_map(4);
		PACK(SOUNDSPEC_NAME, name);
		PACK(SOUNDSPEC_GAIN, gain);
		PACK(SOUNDSPEC_PITCH, pitch);
		PACK(SOUNDSPEC_FADE, fade);
	}
	void msgpack_unpack(msgpack::object o) {
		MsgpackPacket packet = o.as<MsgpackPacket>();
		packet[SOUNDSPEC_NAME].convert(name);
		packet[SOUNDSPEC_GAIN].convert(gain);
		packet[SOUNDSPEC_PITCH].convert(pitch);
		packet[SOUNDSPEC_FADE].convert(fade);
	}




	bool loop = false;
};


// The order must not be changed. This is sent over the network.
enum class SoundLocation : u8 {
	Local,
	Position,
	Object
};
