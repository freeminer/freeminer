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

#ifndef S_ENV_H_
#define S_ENV_H_

#include "cpp_api/s_base.h"
#include "irr_v3d.h"

class ServerEnvironment;
struct MapgenParams;

class ScriptApiEnv
		: virtual public ScriptApiBase
{
public:
	// On environment step
	void environment_Step(float dtime);
	// After generating a piece of map
	void environment_OnGenerated(v3s16 minp, v3s16 maxp,u32 blockseed);

	//called on player event
	void player_event(ServerActiveObject* player, std::string type);

	void initializeEnvironment(ServerEnvironment *env);
};

#endif /* S_ENV_H_ */
