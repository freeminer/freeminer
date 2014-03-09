/*
script/lua_api/l_particles.h
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

#ifndef L_PARTICLES_H_
#define L_PARTICLES_H_

#include "lua_api/l_base.h"

class ModApiParticles : public ModApiBase {
private:
	static int l_add_particle(lua_State *L);
	static int l_add_particlespawner(lua_State *L);
	static int l_delete_particlespawner(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};



#endif /* L_PARTICLES_H_ */
