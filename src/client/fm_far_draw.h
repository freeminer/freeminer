// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mapblock.h"
#include "mapblock_mesh.h"
#include <unordered_map>

namespace farmesh
{

struct ClippedFarMesh
{
	MapBlock::mesh_type source;
	pos_t cell_size;
	std::vector<v3bpos_t> chunks;
	struct Buffer
	{
		irr_ptr<scene::SMeshBuffer> mesh;
		scene::IMeshBuffer *source;
	};
	std::vector<Buffer> buffers;
};

// Published with the near draw list. Holding the exact near meshes prevents
// eviction or LOD changes from invalidating the corresponding far cutouts.
struct FarDrawState
{
	std::unordered_map<v3bpos_t, MapBlock::mesh_type> near_meshes;
	std::unordered_map<v3bpos_t, std::shared_ptr<ClippedFarMesh>> clipped;
};

} // namespace farmesh
