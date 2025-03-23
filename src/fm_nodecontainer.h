/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#include "irr_v3d.h"
#include "mapnode.h"

class VoxelArea;

class NodeContainer
{
public:
	//	virtual const MapNode &getNodeRefUnsafeCheckFlags(const v3pos_t &p) = 0;
	virtual const MapNode &getNodeRefUnsafe(const v3pos_t &p) = 0;
	virtual MapNode getNodeNoExNoEmerge(const v3pos_t &p) { return getNodeRefUnsafe(p); };
	virtual MapNode getNodeNoEx(const v3pos_t &p) { return getNodeRefUnsafe(p); };
	virtual const MapNode &getNodeRefUnsafeCheckFlags(const v3pos_t &p)
	{
		return getNodeRefUnsafe(p);
	};
    virtual void setNode(const v3pos_t &p, const MapNode &n, bool important = false) {};
	virtual void clear() {}
	virtual void addArea(const VoxelArea &a) {};
	virtual void copyFrom(MapNode *src, const VoxelArea &src_area, v3pos_t from_pos,
			v3pos_t to_pos, const v3pos_t &size) {};
};
