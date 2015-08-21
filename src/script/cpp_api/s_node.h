/*
script/cpp_api/s_node.h
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

#ifndef S_NODE_H_
#define S_NODE_H_

#include "irr_v3d.h"
#include "cpp_api/s_base.h"
#include "cpp_api/s_nodemeta.h"
#include "util/string.h"

struct MapNode;
class ServerActiveObject;

class ScriptApiNode
		: virtual public ScriptApiBase,
		  public ScriptApiNodemeta
{
public:
	ScriptApiNode();
	virtual ~ScriptApiNode();

	bool node_on_punch(v3s16 p, MapNode node,
			ServerActiveObject *puncher, PointedThing pointed);
	bool node_on_dig(v3s16 p, MapNode node,
			ServerActiveObject *digger);
	void node_on_construct(v3s16 p, MapNode node);
	void node_on_destruct(v3s16 p, MapNode node);
	void node_after_destruct(v3s16 p, MapNode node);
	void node_on_activate(v3s16 p, MapNode node);
	void node_on_deactivate(v3s16 p, MapNode node);
	bool node_on_timer(v3s16 p, MapNode node, f32 dtime);
	void node_on_receive_fields(v3s16 p,
			const std::string &formname,
			const StringMap &fields,
			ServerActiveObject *sender);
	void node_falling_update(v3s16 p);
	void node_falling_update_single(v3s16 p);
	void node_drop(v3s16 p, int fast);
public:
	static struct EnumString es_DrawType[];
	static struct EnumString es_ContentParamType[];
	static struct EnumString es_ContentParamType2[];
	static struct EnumString es_LiquidType[];
	static struct EnumString es_NodeBoxType[];
};



#endif /* S_NODE_H_ */
