/*
tool.h
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

#ifndef TOOL_HEADER
#define TOOL_HEADER

#include "irrlichttypes.h"
#include <string>
#include <iostream>
#include <map>
#include "itemgroup.h"

#include "connection.h"

enum {
	TOOLGROUPCAP_USES,
	TOOLGROUPCAP_MAXLEVEL,
	TOOLGROUPCAP_TIMES
};

struct ToolGroupCap
{
	std::map<int, float> times;
	int maxlevel;
	int uses;

	ToolGroupCap():
		maxlevel(1),
		uses(20)
	{}

	bool getTime(int rating, float *time) const
	{
		std::map<int, float>::const_iterator i = times.find(rating);
		if(i == times.end()){
			*time = 0;
			return false;
		}
		*time = i->second;
		return true;
	}

	template<typename Packer>
	void msgpack_pack(Packer& pk) const
	{
		pk.pack_map(3);
		PACK(TOOLGROUPCAP_USES, uses);
		PACK(TOOLGROUPCAP_MAXLEVEL, maxlevel);
		PACK(TOOLGROUPCAP_TIMES, times);
	}
	void msgpack_unpack(msgpack::object o)
	{
		MsgpackPacket packet;
		o.convert(&packet);

		packet[TOOLGROUPCAP_USES].convert(&uses);
		packet[TOOLGROUPCAP_MAXLEVEL].convert(&maxlevel);
		packet[TOOLGROUPCAP_TIMES].convert(&times);
	}
};


// CLANG SUCKS DONKEY BALLS
typedef std::map<std::string, struct ToolGroupCap> ToolGCMap;
typedef std::map<std::string, s16> DamageGroup;

enum {
	TOOLCAP_FULL_PUNCH_INTERVAL,
	TOOLCAP_MAX_DROP_LEVEL,
	TOOLCAP_GROUPCAPS,
	TOOLCAP_DAMAGEGROUPS
};

struct ToolCapabilities
{
	float full_punch_interval;
	int max_drop_level;
	// CLANG SUCKS DONKEY BALLS
	ToolGCMap groupcaps;
	DamageGroup damageGroups;

	ToolCapabilities(
			float full_punch_interval_=1.4,
			int max_drop_level_=1,
			// CLANG SUCKS DONKEY BALLS
			ToolGCMap groupcaps_=ToolGCMap(),
			DamageGroup damageGroups_=DamageGroup()
	):
		full_punch_interval(full_punch_interval_),
		max_drop_level(max_drop_level_),
		groupcaps(groupcaps_),
		damageGroups(damageGroups_)
	{}

	void serialize(std::ostream &os, u16 version) const;
	void deSerialize(std::istream &is);

	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const;
	void msgpack_unpack(msgpack::object o);
};

struct DigParams
{
	bool diggable;
	// Digging time in seconds
	float time;
	// Caused wear
	u16 wear;
	std::string main_group;

	DigParams(bool a_diggable=false, float a_time=0, u16 a_wear=0,
			std::string a_main_group=""):
		diggable(a_diggable),
		time(a_time),
		wear(a_wear),
		main_group(a_main_group)
	{}
};

DigParams getDigParams(const ItemGroupList &groups,
		const ToolCapabilities *tp, float time_from_last_punch);

DigParams getDigParams(const ItemGroupList &groups,
		const ToolCapabilities *tp);

struct HitParams
{
	s16 hp;
	s16 wear;

	HitParams(s16 hp_=0, s16 wear_=0):
		hp(hp_),
		wear(wear_)
	{}
};

HitParams getHitParams(const ItemGroupList &armor_groups,
		const ToolCapabilities *tp, float time_from_last_punch);

HitParams getHitParams(const ItemGroupList &armor_groups,
		const ToolCapabilities *tp);

struct PunchDamageResult
{
	bool did_punch;
	int damage;
	int wear;

	PunchDamageResult():
		did_punch(false),
		damage(0),
		wear(0)
	{}
};

struct ItemStack;

PunchDamageResult getPunchDamage(
		const ItemGroupList &armor_groups,
		const ToolCapabilities *toolcap,
		const ItemStack *punchitem,
		float time_from_last_punch
);

#endif

