// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include "irrlichttypes_bloated.h"

// fm:
#include "msgpack_fix.h"
#include "util/msgpack_serialize.h"


enum {
	SOUNDSPEC_NAME,
	SOUNDSPEC_GAIN,
	SOUNDSPEC_PITCH,
	SOUNDSPEC_FADE
};


/**
 * Describes the sound information for playback.
 * Positional handling is done separately.
 *
 * `SimpleSoundSpec`, as used by modding, is a `SoundSpec` with only name, fain,
 * pitch and fade.
*/
struct SoundSpec
{
	SoundSpec(std::string_view name = "", float gain = 1.0f,
			bool loop = false, float fade = 0.0f, float pitch = 1.0f,
			float start_time = 0.0f) :
			name(name), gain(gain), fade(fade), pitch(pitch), start_time(start_time),
			loop(loop)
	{
	}

	bool exists() const { return !name.empty(); }

	// Serialize/Deserialize a `SimpleSoundSpec`.
	void serializeSimple(std::ostream &os, u16 protocol_version) const;
	void deSerializeSimple(std::istream &is, u16 protocol_version);

	// Name of the sound-group
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




	float start_time = 0.0f;
	bool loop = false;
	// If true, a local fallback (ie. from the user's sound pack) is used if the
	// sound-group does not exist.
	bool use_local_fallback = true;
};


// The order must not be changed. This is sent over the network.
enum class SoundLocation : u8 {
	Local,
	Position,
	Object
};
