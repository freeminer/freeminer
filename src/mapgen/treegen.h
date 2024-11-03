/*
treegen.h
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>,
Copyright (C) 2012-2018 RealBadAngel, Maciej Kasatkin
Copyright (C) 2015-2018 paramat
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

#include <string>
#include "irr_v3d.h"
#include "nodedef.h"
#include "mapnode.h"

class MMVManip;
class NodeDefManager;
class ServerMap;

namespace treegen {

	enum error {
		SUCCESS,
		UNBALANCED_BRACKETS
	};

	struct TreeDef : public NodeResolver {
		TreeDef() :
			// Initialize param1 and param2
			trunknode(CONTENT_IGNORE),
			leavesnode(CONTENT_IGNORE),
			leaves2node(CONTENT_IGNORE),
			fruitnode(CONTENT_IGNORE)
		{}
		virtual void resolveNodeNames();

		std::string initial_axiom;
		std::string rules_a;
		std::string rules_b;
		std::string rules_c;
		std::string rules_d;

		MapNode trunknode;
		MapNode leavesnode;
		MapNode leaves2node;

		int leaves2_chance;
		int angle;
		int iterations;
		int iterations_random_level;
		std::string trunk_type;
		bool thin_branches;
		MapNode fruitnode;
		int fruit_chance;
		s32 seed;
		bool explicit_seed;
	};

	// Add default tree
	void make_tree(MMVManip &vmanip, v3s16 p0,
		bool is_apple_tree, const NodeDefManager *ndef, s32 seed);
	// Add jungle tree
	void make_jungletree(MMVManip &vmanip, v3s16 p0,
		const NodeDefManager *ndef, s32 seed);
	// Add pine tree
	void make_pine_tree(MMVManip &vmanip, v3s16 p0,
		const NodeDefManager *ndef, s32 seed);

	void make_cavetree(MMVManip &vmanip, v3pos_t p0,
		bool is_jungle_tree, const NodeDefManager *ndef, s32 seed);

	// Spawn L-Systems tree on VManip
	treegen::error make_ltree(MMVManip &vmanip, v3s16 p0, const TreeDef &def);
	// Helper to spawn it directly on map
	treegen::error spawn_ltree(ServerMap *map, v3s16 p0, const TreeDef &def);
}; // namespace treegen
