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

#include "irr_v3d.h"
#include "server/luaentity_sao.h"
#include "mapnode.h"
#include "nodetimer.h"

namespace epixel
{

class FallingSAO : public LuaEntitySAO
{
public:
	FallingSAO(ServerEnvironment *env, v3opos_t pos, const std::string &name,
			const std::string &state, int fast_ = 2);
	~FallingSAO();

	ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_LUAFALLING; }

	static ServerActiveObject *create(
			ServerEnvironment *env, v3opos_t pos, const std::string &data);

	virtual void addedToEnvironment(u32 dtime_s);

	void step(float dtime, bool send_recommended);

	void attachNode(const MapNode m);
	bool captureNodeState(const v3pos_t &pos);
	void getStaticData(std::string *result) const override;

private:
	bool placeNode(const v3pos_t &pos, const MapNode &node);
	bool escapeFromNode(const v3opos_t &pos, const v3pos_t &node_pos);
	MapNode m_node;
	std::string m_metadata;
	NodeTimer m_timer;
	int fast;
	unsigned int m_blocked_attempts = 0;
	float m_stuck_check_timer = 0.0f;
};

}
#endif // __FALLINGSAO_H__
