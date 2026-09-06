// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "irr_v3d.h"
#include <cstdint>
#include <vector>

namespace farmesh
{

// One mesh job owns this cache. Include the coarse neighbour halo used by face
// generation; unusual off-grid lighting samples are resolved by the caller.
template <typename Value>
class GridSampleCache
{
public:
	GridSampleCache(const v3pos_t &origin, pos_t side, block_step_t step) :
			m_spacing(static_cast<pos_t>(1) << step),
			m_min(origin - v3pos_t(m_spacing, m_spacing, m_spacing)),
			m_side(static_cast<size_t>(side) + 2), m_values(m_side * m_side * m_side),
			m_ready(m_values.size())
	{
	}

	template <typename Load>
	Value get(const v3pos_t &pos, Load load)
	{
		const auto d = pos - m_min;
		if (d.X < 0 || d.Y < 0 || d.Z < 0 || d.X % m_spacing || d.Y % m_spacing ||
				d.Z % m_spacing)
			return load();
		const size_t x = d.X / m_spacing, y = d.Y / m_spacing, z = d.Z / m_spacing;
		if (x >= m_side || y >= m_side || z >= m_side)
			return load();
		const size_t index = (z * m_side + y) * m_side + x;
		if (!m_ready[index]) {
			m_values[index] = load();
			m_ready[index] = true;
		}
		return m_values[index];
	}

private:
	pos_t m_spacing;
	v3pos_t m_min;
	size_t m_side;
	std::vector<Value> m_values;
	std::vector<uint8_t> m_ready;
};

} // namespace farmesh
