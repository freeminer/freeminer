// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"
#include "cpp_api/s_base.h"
#include "cpp_api/s_nodemeta.h"
#include "threading/concurrent_vector.h"
#include "util/string.h"

struct MapNode;
class ServerActiveObject;

class ScriptApiNode
		: virtual public ScriptApiBase,
		  public ScriptApiNodemeta
{
public:
	ScriptApiNode() = default;
	virtual ~ScriptApiNode() = default;

	bool node_on_punch(v3pos_t p, MapNode node,
			ServerActiveObject *puncher, const PointedThing &pointed);
	bool node_on_dig(v3pos_t p, MapNode node,
			ServerActiveObject *digger);
	void node_on_construct(v3pos_t p, MapNode node);
	void node_on_destruct(v3pos_t p, MapNode node);
	bool node_on_flood(v3pos_t p, MapNode node, MapNode newnode);
	void node_after_destruct(v3pos_t p, MapNode node);
	bool node_on_timer(v3pos_t p, MapNode node, f32 dtime);
	void node_on_receive_fields(v3pos_t p,
			const std::string &formname,
			const StringMap &fields,
			ServerActiveObject *sender);

//fm:
	concurrent_vector<std::function<void()>> postponed;
	void node_on_activate(v3pos_t p, MapNode node);
	void node_on_deactivate(v3pos_t p, MapNode node);
	void node_drop(v3pos_t p, int fast);

public:
	static struct EnumString es_DrawType[];
	static struct EnumString es_ContentParamType[];
	static struct EnumString es_ContentParamType2[];
	static struct EnumString es_LiquidType[];
	static struct EnumString es_LiquidMoveType[];
	static struct EnumString es_NodeBoxType[];
	static struct EnumString es_TextureAlphaMode[];
};
