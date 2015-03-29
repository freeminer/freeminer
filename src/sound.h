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

#ifndef SOUND_HEADER
#define SOUND_HEADER

#include "irrlichttypes_bloated.h"
#include <string>
#include <set>
#include "msgpack.h"
#include "network/connection.h"
#include "util/msgpack_serialize.h"

class OnDemandSoundFetcher
{
public:
	virtual void fetchSounds(const std::string &name,
			std::set<std::string> &dst_paths,
			std::set<std::string> &dst_datas) = 0;
};

enum {
	SOUNDSPEC_NAME,
	SOUNDSPEC_GAIN
};

struct SimpleSoundSpec
{
	std::string name;
	float gain;
	SimpleSoundSpec(std::string name="", float gain=1.0):
		name(name),
		gain(gain)
	{}
	bool exists() {return name != "";}

	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const {
		pk.pack_map(2);
		PACK(SOUNDSPEC_NAME, name);
		PACK(SOUNDSPEC_GAIN, gain);
	}
	void msgpack_unpack(msgpack::object o) {
		MsgpackPacket packet = o.as<MsgpackPacket>();
		packet[SOUNDSPEC_NAME].convert(&name);
		packet[SOUNDSPEC_GAIN].convert(&gain);
	}
};

class ISoundManager
{
public:
	virtual ~ISoundManager(){}
	
	// Multiple sounds can be loaded per name; when played, the sound
	// should be chosen randomly from alternatives
	// Return value determines success/failure
	virtual bool loadSoundFile(const std::string &name,
			const std::string &filepath) = 0;
	virtual bool loadSoundData(const std::string &name,
			const std::string &filedata) = 0;

	virtual void updateListener(v3f pos, v3f vel, v3f at, v3f up) = 0;
	virtual void setListenerGain(float gain) = 0;

	// playSound functions return -1 on failure, otherwise a handle to the
	// sound. If name=="", call should be ignored without error.
	virtual int playSound(const std::string &name, bool loop,
			float volume) = 0;
	virtual int playSoundAt(const std::string &name, bool loop,
			float volume, v3f pos) = 0;
	virtual void stopSound(int sound) = 0;
	virtual bool soundExists(int sound) = 0;
	virtual void updateSoundPosition(int sound, v3f pos) = 0;

	int playSound(const SimpleSoundSpec &spec, bool loop)
		{ return playSound(spec.name, loop, spec.gain); }
	int playSoundAt(const SimpleSoundSpec &spec, bool loop, v3f pos)
		{ return playSoundAt(spec.name, loop, spec.gain, pos); }
};

class DummySoundManager: public ISoundManager
{
public:
	virtual bool loadSoundFile(const std::string &name,
			const std::string &filepath) {return true;}
	virtual bool loadSoundData(const std::string &name,
			const std::string &filedata) {return true;}
	void updateListener(v3f pos, v3f vel, v3f at, v3f up) {}
	void setListenerGain(float gain) {}
	int playSound(const std::string &name, bool loop,
			float volume) {return 0;}
	int playSoundAt(const std::string &name, bool loop,
			float volume, v3f pos) {return 0;}
	void stopSound(int sound) {}
	bool soundExists(int sound) {return false;}
	void updateSoundPosition(int sound, v3f pos) {}
};

// Global DummySoundManager singleton
extern DummySoundManager dummySoundManager;

#endif

