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

#ifndef __ITEMSAO_H__
#define __ITEMSAO_H__

#include "content_sao.h"

namespace epixel
{

class ItemSAO: public LuaEntitySAO {
public:
	ItemSAO(ServerEnvironment *env, v3f pos,
			const std::string &name, const std::string &state);
	~ItemSAO();

	ActiveObjectType getType() const
	{ return ACTIVEOBJECT_TYPE_LUAITEM; }

	static ServerActiveObject* create(ServerEnvironment *env, v3f pos,
			const std::string &data);

	virtual void addedToEnvironment(u32 dtime_s);

	void step(float dtime, bool send_recommended);

	void attachItems(ItemStack st) { m_item_stack = st; }
	ItemStack getAttachedItems() { return m_item_stack; }
	bool canBeLooted() { return (m_timer_before_loot < 0.0f); }
private:
	ItemStack m_item_stack;
	float m_timer_before_loot;
	float m_life_timer;
	float m_check_current_node_timer;
};

}
#endif // __ITEMSAO_H__

