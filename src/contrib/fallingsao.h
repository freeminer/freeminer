/*
 * Epixel
 * Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef __FALLINGSAO_H__
#define __FALLINGSAO_H__

#include "content_sao.h"
#include "mapnode.h"

namespace epixel
{

class FallingSAO: public LuaEntitySAO {
public:
	FallingSAO(ServerEnvironment *env, v3f pos,
			const std::string &name, const std::string &state, int fast_ = 2);
	~FallingSAO();

	ActiveObjectType getType() const
	{ return ACTIVEOBJECT_TYPE_LUAFALLING; }

	static ServerActiveObject* create(ServerEnvironment *env, v3f pos,
			const std::string &data);

	virtual void addedToEnvironment(u32 dtime_s);

	void step(float dtime, bool send_recommended);

	void attachNode(const MapNode m) { m_node = m; }
private:
	MapNode m_node;
	int fast;
};

}
#endif // __FALLINGSAO_H__

