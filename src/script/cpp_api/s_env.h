/*
script/cpp_api/s_env.h
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

#include "cpp_api/s_base.h"
#include "irr_v3d.h"
#include "mapnode.h"
#include <unordered_set>
#include <vector>

class ServerEnvironment;
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

	void initializeEnvironment(ServerEnvironment *env);
};
