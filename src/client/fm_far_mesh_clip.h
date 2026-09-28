// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "constants.h"
#include "irr_v3d.h"
#include "irr_ptr.h"
#include <CMeshBuffer.h>
#include <array>
#include <memory>
#include <vector>

namespace farmesh
{

// Remove completed near chunks from a far mesh, including triangles crossing
// chunk boundaries. The sparse octree visits only regions with near coverage;
// a coarse teleport fallback never requires enumerating its entire volume.
class FarMeshClipMask
{
	struct Node
	{
		bool covered = false;
		std::array<std::unique_ptr<Node>, 8> children;
	};
	using Polygon = std::vector<video::S3DVertex>;

public:
	FarMeshClipMask(v3bpos_t origin, block_step_t step, pos_t cell_size) :
			m_origin(origin), m_step(step), m_cell_size(cell_size)
	{
	}

	void addChunk(const v3bpos_t &pos)
	{
		// Grid extents can exceed pos_t even in a 16-bit world. Keep offset
		// arithmetic wide; only actual map positions use v3bpos_t.
		std::array<int64_t, 3> relative;
		for (u8 axis = 0; axis < 3; ++axis) {
			relative[axis] = int64_t(pos[axis]) - m_origin[axis] - m_grid_offset[axis];
			if (relative[axis] % m_cell_size)
				return;
		}
		for (;;) {
			const int64_t side = int64_t(m_cell_size) << m_step;
			bool inside = true;
			unsigned old_child = 0;
			for (u8 axis = 0; axis < 3; ++axis) {
				inside &= relative[axis] >= 0 && relative[axis] < side;
				if (relative[axis] < 0) {
					m_grid_offset[axis] -= side;
					relative[axis] += side;
					old_child |= 1U << axis;
				}
			}
			if (inside)
				break;
			Node parent;
			parent.children[old_child] = std::make_unique<Node>(std::move(m_root));
			m_root = std::move(parent);
			++m_step;
		}
		for (auto &coordinate : relative)
			coordinate /= m_cell_size;
		add(m_root, relative, m_step);
	}

	std::vector<irr_ptr<scene::SMeshBuffer>> clip(const scene::IMeshBuffer &source) const
	{
		std::vector<irr_ptr<scene::SMeshBuffer>> result;
		// MapBlockMesh generates standard vertices, triangles and light points.
		assert(source.getVertexType() == video::EVT_STANDARD);
		assert(source.getPrimitiveType() == scene::EPT_TRIANGLES ||
				source.getPrimitiveType() == scene::EPT_POINTS);
		const auto *vertices =
				static_cast<const video::S3DVertex *>(source.getVertices());
		const auto index = [&](u32 i) -> u32 {
			const auto *indices = source.getIndexBuffer();
			return indices->getType() == video::EIT_16BIT
						   ? static_cast<const u16 *>(indices->getData())[i]
						   : static_cast<const u32 *>(indices->getData())[i];
		};
		const auto emit = [&](const Polygon &polygon) {
			const bool point = source.getPrimitiveType() == scene::EPT_POINTS;
			if (polygon.size() < (point ? 1 : 3))
				return;
			if (result.empty() ||
					result.back()->getVertexCount() + polygon.size() > U16_MAX) {
				auto buffer = make_irr<scene::SMeshBuffer>();
				buffer->setPrimitiveType(source.getPrimitiveType());
				result.push_back(std::move(buffer));
			}
			auto &buffer = *result.back();
			const u16 base = buffer.getVertexCount();
			buffer.Vertices->Data.insert(
					buffer.Vertices->Data.end(), polygon.begin(), polygon.end());
			if (point) {
				buffer.Indices->Data.push_back(base);
			} else {
				for (size_t i = 1; i + 1 < polygon.size(); ++i) {
					buffer.Indices->Data.push_back(base);
					buffer.Indices->Data.push_back(base + i);
					buffer.Indices->Data.push_back(base + i + 1);
				}
			}
		};
		const float width = (int64_t(m_cell_size) << m_step) * MAP_BLOCKSIZE * BS;
		v3opos_t minimum;
		for (u8 axis = 0; axis < 3; ++axis)
			minimum[axis] = m_grid_offset[axis] * (MAP_BLOCKSIZE * BS) - 0.5 * BS;
		const u32 stride = source.getPrimitiveType() == scene::EPT_POINTS ? 1 : 3;
		for (u32 i = 0; i + stride <= source.getIndexCount(); i += stride) {
			Polygon polygon;
			for (u32 j = 0; j < stride; ++j)
				polygon.push_back(vertices[index(i + j)]);
			// Oversized nodes can extend outside the nominal far cell. Preserve
			// geometry outside the mask root instead of clamping it into an edge.
			for (u8 axis = 0; axis < 3 && !polygon.empty(); ++axis) {
				auto lower = split(polygon, axis, minimum[axis]);
				emit(lower[0]);
				polygon = std::move(lower[1]);
				if (polygon.empty())
					break;
				auto upper = split(polygon, axis, minimum[axis] + width);
				emit(upper[1]);
				polygon = std::move(upper[0]);
			}
			if (!polygon.empty())
				visit(m_root, polygon, minimum, width, emit);
		}
		for (auto &buffer : result) {
			buffer->recalculateBoundingBox();
			buffer->setHardwareMappingHint(scene::EHM_STATIC);
		}
		return result;
	}

private:
	static void add(Node &node, const std::array<int64_t, 3> &pos, block_step_t step)
	{
		if (node.covered)
			return;
		if (!step) {
			node.covered = true;
			return;
		}
		const auto bit = step - 1;
		const unsigned child = ((pos[0] >> bit) & 1) | (((pos[1] >> bit) & 1) << 1) |
							   (((pos[2] >> bit) & 1) << 2);
		if (!node.children[child])
			node.children[child] = std::make_unique<Node>();
		add(*node.children[child], pos, step - 1);
		for (const auto &entry : node.children)
			if (!entry || !entry->covered)
				return;
		node.covered = true;
		for (auto &entry : node.children)
			entry.reset();
	}

	static std::array<Polygon, 2> split(const Polygon &polygon, u8 axis, float plane)
	{
		std::array<Polygon, 2> parts;
		bool below = false, above = false;
		for (const auto &vertex : polygon) {
			below |= vertex.Pos[axis] < plane;
			above |= vertex.Pos[axis] > plane;
		}
		if (!below || !above) {
			// A face on a boundary belongs to the solid side of its normal.
			const unsigned side = above || (!below && polygon[0].Normal[axis] <= 0);
			parts[side] = polygon;
			return parts;
		}
		auto previous = polygon.back();
		for (const auto &vertex : polygon) {
			const float a = previous.Pos[axis] - plane;
			const float b = vertex.Pos[axis] - plane;
			if ((a < 0 && b > 0) || (a > 0 && b < 0)) {
				auto intersection = previous.getInterpolated(vertex, b / (b - a));
				intersection.Pos[axis] = plane;
				parts[0].push_back(intersection);
				parts[1].push_back(intersection);
			}
			if (b <= 0)
				parts[0].push_back(vertex);
			if (b >= 0)
				parts[1].push_back(vertex);
			previous = vertex;
		}
		return parts;
	}

	template <typename Emit>
	static void visit(const Node &node, const Polygon &polygon, const v3opos_t &minimum,
			float width, const Emit &emit)
	{
		if (node.covered)
			return;
		std::array<Polygon, 8> parts;
		parts[0] = polygon;
		const float half = width / 2;
		for (u8 axis = 0; axis < 3; ++axis)
			for (unsigned i = 0; i < (1U << axis); ++i) {
				if (parts[i].empty())
					continue;
				auto halves = split(parts[i], axis, minimum[axis] + half);
				parts[i] = std::move(halves[0]);
				parts[i | (1U << axis)] = std::move(halves[1]);
			}
		for (unsigned i = 0; i < 8; ++i) {
			if (parts[i].empty())
				continue;
			if (!node.children[i]) {
				emit(parts[i]);
				continue;
			}
			auto child_min = minimum;
			for (u8 axis = 0; axis < 3; ++axis)
				if (i & (1U << axis))
					child_min[axis] += half;
			visit(*node.children[i], parts[i], child_min, half, emit);
		}
	}

	v3bpos_t m_origin;
	std::array<int64_t, 3> m_grid_offset{};
	block_step_t m_step;
	pos_t m_cell_size;
	Node m_root;
};

} // namespace farmesh
