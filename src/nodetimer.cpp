/*
nodetimer.cpp
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

#include "nodetimer.h"
#include "log.h"
#include "serialization.h"
#include "util/serialize.h"
#include "constants.h" // MAP_BLOCKSIZE

/*
	NodeTimer
*/

void NodeTimer::serialize(std::ostream &os) const
{
	writeF1000(os, timeout);
	writeF1000(os, elapsed);
}

void NodeTimer::deSerialize(std::istream &is)
{
	timeout = readF1000(is);
	elapsed = readF1000(is);
}

/*
	NodeTimerList
*/

void NodeTimerList::serialize(std::ostream &os, u8 map_format_version) const
{
	if (map_format_version == 24) {
		// Version 0 is a placeholder for "nothing to see here; go away."
		if (m_data.empty()) {
			writeU8(os, 0); // version
			return;
		}
		writeU8(os, 1); // version
		writeU16(os, m_data.size());
	}

	if (map_format_version >= 25) {
		writeU8(os, 2 + 4 + 4); // length of the data for a single timer
		writeU16(os, m_data.size());
	}

	for (std::map<v3s16, NodeTimer>::const_iterator
			i = m_data.begin();
			i != m_data.end(); ++i) {
		v3s16 p = i->first;
		NodeTimer t = i->second;

		u16 p16 = p.Z * MAP_BLOCKSIZE * MAP_BLOCKSIZE + p.Y * MAP_BLOCKSIZE + p.X;
		writeU16(os, p16);
		t.serialize(os);
	}
}

void NodeTimerList::deSerialize(std::istream &is, u8 map_format_version)
{
	m_data.clear();

	if(map_format_version == 24){
		u8 timer_version = readU8(is);
		if(timer_version == 0)
			return;
		if(timer_version != 1)
			throw SerializationError("unsupported NodeTimerList version");
	}

	if(map_format_version >= 25){
		u8 timer_data_len = readU8(is);
		if(timer_data_len != 2+4+4)
			throw SerializationError("unsupported NodeTimer data length");
	}

	u16 count = readU16(is);

	for(u16 i=0; i<count; i++)
	{
		u16 p16 = readU16(is);

		v3s16 p;
		p.Z = p16 / MAP_BLOCKSIZE / MAP_BLOCKSIZE;
		p16 &= MAP_BLOCKSIZE * MAP_BLOCKSIZE - 1;
		p.Y = p16 / MAP_BLOCKSIZE;
		p16 &= MAP_BLOCKSIZE - 1;
		p.X = p16;

		NodeTimer t;
		t.deSerialize(is);

		if(t.timeout <= 0)
		{
			infostream<<"WARNING: NodeTimerList::deSerialize(): "
					<<"invalid data at position"
					<<"("<<p.X<<","<<p.Y<<","<<p.Z<<"): Ignoring."
					<<std::endl;
			continue;
		}

		if(m_data.find(p) != m_data.end())
		{
			infostream<<"WARNING: NodeTimerList::deSerialize(): "
					<<"already set data at position"
					<<"("<<p.X<<","<<p.Y<<","<<p.Z<<"): Ignoring."
					<<std::endl;
			continue;
		}

		m_data.insert(std::make_pair(p, t));
	}
}

std::map<v3s16, NodeTimer> NodeTimerList::step(float dtime)
{
	std::map<v3s16, NodeTimer> elapsed_timers;
	// Increment timers
	for(std::map<v3s16, NodeTimer>::iterator
			i = m_data.begin();
			i != m_data.end(); ++i){
		v3s16 p = i->first;
		NodeTimer t = i->second;
		t.elapsed += dtime;
		if(t.elapsed >= t.timeout)
			elapsed_timers.insert(std::make_pair(p, t));
		else
			i->second = t;
	}
	// Delete elapsed timers
	for(std::map<v3s16, NodeTimer>::const_iterator
			i = elapsed_timers.begin();
			i != elapsed_timers.end(); ++i){
		v3s16 p = i->first;
		m_data.erase(p);
	}
	return elapsed_timers;
}
