/*
block_active.h
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

#ifndef MAPBLOCK_ACTIVE_HEADER
#define MAPBLOCK_ACTIVE_HEADER

#include "irr_aabb3d.h"


/*
	Active block modifier interface.

	These are fed into ServerEnvironment at initialization time;
	ServerEnvironment handles deleting them.
*/

class ServerEnvironment;

class ActiveBlockModifier
{
public:
	ActiveBlockModifier(){};
	virtual ~ActiveBlockModifier(){};
	
	// Set of contents to trigger on
	virtual std::set<std::string> getTriggerContents()=0;
	// Set of required neighbors (trigger doesn't happen if none are found)
	// Empty = do not check neighbors
	virtual std::set<std::string> getRequiredNeighbors(bool activate)
	{ return std::set<std::string>(); }
	// Maximum range to neighbors
	virtual u32 getNeighborsRange()
	{ return 1; };
	// Trigger interval in seconds
	virtual float getTriggerInterval() = 0;
	// Random chance of (1 / return value), 0 is disallowed
	virtual u32 getTriggerChance() = 0;
	// This is called usually at interval for 1/chance of the nodes
	//virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n){};
	//virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n, MapNode neighbor){};
	virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, MapNode neighbor, bool activate = false){};
};

struct ABMWithState
{
	ActiveBlockModifier *abm;
	float interval;
	float chance;
	float timer;
	std::unordered_set<content_t> trigger_ids;
	FMBitset required_neighbors, required_neighbors_activate;

	ABMWithState(ActiveBlockModifier *abm_, ServerEnvironment *senv);
};


struct ActiveABM
{
	ActiveABM()
	{}
	ABMWithState *abmws;
	//ActiveBlockModifier *abm; //delete me, abm in ws ^
	int chance;
};


struct abm_trigger_one {
	ActiveABM * i;
	v3s16 p;
	MapNode n;
	u32 active_object_count;
	u32 active_object_count_wider;
	MapNode neighbor;
};


#endif
