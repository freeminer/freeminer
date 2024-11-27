// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "irr_v3d.h"
#include "mapnode.h"
#include "threading/concurrent_vector.h"
#include <unordered_set>
#include <vector>

class ServerEnvironment;
class MapBlock;
struct ScriptCallbackState;

class ScriptApiEnv : virtual public ScriptApiBase
{
public:
	// Called on environment step
	void environment_Step(float dtime);

	// Called after generating a piece of map
	void environment_OnGenerated(v3s16 minp, v3s16 maxp, u32 blockseed);

	// Called on player event
	void player_event(ServerActiveObject *player, const std::string &type);

// fm:
	void player_event_real(ServerActiveObject *player, const std::string &type);
	struct pevent
	{
		ServerActiveObject *player;
		std::string type;
	};
	concurrent_vector<pevent> player_events;
	void player_event_process();
// ==
	// Called after emerge of a block queued from core.emerge_area()
	void on_emerge_area_completion(v3s16 blockpos, int action,
		ScriptCallbackState *state);

	void check_for_falling(v3s16 p);

	// Called after liquid transform changes
	void on_liquid_transformed(const std::vector<std::pair<v3s16, MapNode>> &list);

	// Called after mapblock changes
	void on_mapblocks_changed(const std::unordered_set<v3s16> &set);

	// Determines whether there are any on_mapblocks_changed callbacks
	bool has_on_mapblocks_changed();

	// Initializes environment and loads some definitions from Lua
	void initializeEnvironment(ServerEnvironment *env);

	void triggerABM(int id, v3s16 p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider
			, v3pos_t neighbor_pos, uint8_t activate
			);

	void triggerLBM(int id, MapBlock *block,
		const std::unordered_set<v3s16> &positions, float dtime_s);

private:
	void readABMs();

	void readLBMs();

	// Reads a single or a list of node names into a vector
	static bool read_nodenames(lua_State *L, int idx, std::vector<std::string> &to);
};
