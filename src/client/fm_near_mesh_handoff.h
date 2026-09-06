// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "constants.h"
#include "irr_v3d.h"
#include <IMesh.h>
#include <IMeshBuffer.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace farmesh
{

// A server sends a sparse set of blocks: air and occluded chunks may never
// arrive. Require completed meshes where they replace far geometry. Unfinished
// chunks outside that geometry must not hold back already covered terrain.
// Construct this for each draw-list update; omission/occlusion is not permanent.
class NearMeshHandoff
{
public:
	NearMeshHandoff(v3bpos_t origin, block_step_t step, pos_t cell_size) :
			m_origin(origin), m_side(1 << step), m_cell_size(cell_size)
	{
	}

	void addChunk(v3bpos_t pos, bool ready) { m_chunks.insert_or_assign(pos, ready); }

	bool hasReadyChunk() const
	{
		return std::any_of(m_chunks.begin(), m_chunks.end(),
				[](const auto &entry) { return entry.second; });
	}

	template <typename Occluded>
	bool covers(const scene::IMesh &mesh, Occluded occluded)
	{
		if (!hasReadyChunk())
			return false;
		for (u32 i = 0; i < mesh.getMeshBufferCount(); ++i) {
			const auto &buffer = *mesh.getMeshBuffer(i);
			const auto count = buffer.getIndexCount();
			const auto index = [&](u32 j) -> u32 {
				const auto *indices = buffer.getIndexBuffer();
				return indices->getType() == video::EIT_16BIT
							   ? static_cast<const u16 *>(indices->getData())[j]
							   : static_cast<const u32 *>(indices->getData())[j];
			};
			if (buffer.getPrimitiveType() == scene::EPT_TRIANGLES) {
				for (u32 j = 0; j + 2 < count; j += 3) {
					const auto a = index(j), b = index(j + 1), c = index(j + 2);
					const auto p = v3opos_t::from(buffer.getPosition(a));
					const auto q = v3opos_t::from(buffer.getPosition(b));
					const auto r = v3opos_t::from(buffer.getPosition(c));
					v3opos_t lo, hi;
					for (u8 axis = 0; axis < 3; ++axis) {
						lo[axis] = std::min({p[axis], q[axis], r[axis]});
						hi[axis] = std::max({p[axis], q[axis], r[axis]});
					}
					if (!coversBox(lo, hi, v3opos_t::from(buffer.getNormal(a)), occluded))
						return false;
				}
			} else if (buffer.getPrimitiveType() == scene::EPT_POINTS) {
				for (u32 j = 0; j < count; ++j) {
					const auto p = v3opos_t::from(buffer.getPosition(index(j)));
					if (!coversBox(p, p, {}, occluded))
						return false;
				}
			} else if (count) {
				const auto &box = buffer.getBoundingBox();
				if (!coversBox(v3opos_t::from(box.MinEdge), v3opos_t::from(box.MaxEdge),
							{}, occluded))
					return false;
			}
		}
		return true;
	}

private:
	template <typename Occluded>
	bool coversBox(const v3opos_t &lo, const v3opos_t &hi, const v3opos_t &normal,
			Occluded occluded)
	{
		v3pos_t first, last;
		const double width = m_cell_size * MAP_BLOCKSIZE;
		for (u8 axis = 0; axis < 3; ++axis) {
			// Vertices are local to the mesh, with node boundaries at n - 0.5.
			// A face exactly on a chunk boundary belongs to its solid side.
			double low = double(lo[axis]) / BS + 0.5;
			double high = double(hi[axis]) / BS + 0.5;
			if (high - low < 0.0001) {
				low -= normal[axis] * 0.0001;
				high = low;
			}
			first[axis] = std::clamp<int>(std::floor(low / width), 0, m_side - 1);
			last[axis] = std::clamp<int>(
					high > low ? std::ceil(high / width) - 1 : std::floor(high / width),
					0, m_side - 1);
		}
		for (pos_t z = first.Z; z <= last.Z; ++z)
			for (pos_t y = first.Y; y <= last.Y; ++y)
				for (pos_t x = first.X; x <= last.X; ++x) {
					const v3bpos_t pos = m_origin + v3bpos_t(x, y, z) * m_cell_size;
					if (auto it = m_chunks.find(pos); it != m_chunks.end()) {
						if (!it->second)
							return false;
					} else {
						auto [omitted, inserted] = m_omitted.try_emplace(pos, false);
						if (inserted)
							omitted->second = occluded(pos);
						if (!omitted->second)
							return false;
					}
				}
		return true;
	}

	v3bpos_t m_origin;
	int m_side;
	pos_t m_cell_size;
	std::unordered_map<v3bpos_t, bool> m_chunks;
	std::unordered_map<v3bpos_t, bool> m_omitted;
};

} // namespace farmesh
