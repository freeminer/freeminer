/*
voxelalgorithms.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef VOXELALGORITHMS_HEADER
#define VOXELALGORITHMS_HEADER

#include "voxel.h"
#include "mapnode.h"
#include <set>
#include <map>

namespace voxalgo
{

// TODO: Move unspreadLight and spreadLight from VoxelManipulator to here

void setLight(VoxelManipulator &v, VoxelArea a, u8 light,
		INodeDefManager *ndef);

void clearLightAndCollectSources(VoxelManipulator &v, VoxelArea a,
		enum LightBank bank, INodeDefManager *ndef,
		std::set<v3s16> & light_sources,
		std::map<v3s16, u8> & unlight_from);

struct SunlightPropagateResult
{
	bool bottom_sunlight_valid;

	SunlightPropagateResult(bool bottom_sunlight_valid_):
		bottom_sunlight_valid(bottom_sunlight_valid_)
	{}
};

SunlightPropagateResult propagateSunlight(VoxelManipulator &v, VoxelArea a,
		bool inexistent_top_provides_sunlight,
		std::set<v3s16> & light_sources,
		INodeDefManager *ndef);

} // namespace voxalgo

#endif

