// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "constants.h"
#include "irr_v3d.h"
#include "irr_aabb3d.h"
#include <IMesh.h>
#include <IMeshBuffer.h>

namespace farmesh
{

// MapBlockMesh normally uses its collector's bounding sphere. The near/far
// cutout lookup also needs actual mesh bounds, including directly appended
// light points. A single point has zero volume but is still drawable geometry.
inline void updateMeshBounds(scene::IMesh &mesh)
{
	aabb3f bounds;
	bool initialized = false;
	for (u32 i = 0; i < mesh.getMeshBufferCount(); ++i) {
		auto &buffer = *mesh.getMeshBuffer(i);
		buffer.recalculateBoundingBox();
		if (!buffer.getVertexCount())
			continue;
		if (initialized)
			bounds.addInternalBox(buffer.getBoundingBox());
		else
			bounds = buffer.getBoundingBox();
		initialized = true;
	}
	mesh.setBoundingBox(bounds);
}

inline bool intersectsNearChunk(const aabb3f &bounds, const v3bpos_t &far_origin,
		const v3bpos_t &near_origin, pos_t near_width)
{
	for (u8 axis = 0; axis < 3; ++axis) {
		const double minimum =
				(double(near_origin[axis]) - far_origin[axis]) * MAP_BLOCKSIZE * BS -
				0.5 * BS;
		if (bounds.MaxEdge[axis] < minimum ||
				bounds.MinEdge[axis] > minimum + double(near_width) * BS)
			return false;
	}
	return true;
}

} // namespace farmesh
