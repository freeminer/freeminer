// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <chrono>
#include <future>
#include <vector>
#include <unordered_set>
#include "irr_v3d.h"
#include "mapblock.h"
#include <limits>

namespace farmesh
{

// Track our own submitted work, including tasks already removed from the shared
// pool's queue. Keep the batch small so near meshes can still use the pool.
class MeshJobs
{
public:
	static constexpr size_t limit = 16;

	void add(std::future<void> job) { m_jobs.push_back(std::move(job)); }
	bool empty() const { return m_jobs.empty(); }
	bool full() const { return m_jobs.size() >= limit; }

	template <typename OnError>
	void poll(OnError on_error)
	{
		for (auto it = m_jobs.begin(); it != m_jobs.end();) {
			if (it->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
				++it;
				continue;
			}
			try {
				it->get();
			} catch (const std::exception &e) {
				on_error(e);
			}
			it = m_jobs.erase(it);
		}
	}

private:
	std::vector<std::future<void>> m_jobs;
};

// A far mesh spans cell_size^3 storage blocks. Storage spacing depends on
// far_step; client_mesh_chunk controls their count, not their spacing.
template <typename Visit>
void forEachMeshSource(
		const v3bpos_t &origin, block_step_t step, uint8_t cell_pow, Visit visit)
{
	const pos_t count = pos_t(1) << cell_pow;
	const pos_t stride = pos_t(1) << step;
	for (pos_t z = 0; z < count; ++z)
		for (pos_t y = 0; y < count; ++y)
			for (pos_t x = 0; x < count; ++x)
				visit(origin + v3bpos_t(x, y, z) * stride);
}

// Grid cells are aligned octree cubes. Lookup uses their actual step, since a
// partially published grid can contain cells sampled for different origins.
template <typename Grid, typename Step>
auto findCoveringCell(
		const Grid &grid, const v3bpos_t &pos, Step step, uint8_t cell_pow = 0)
{
	for (block_step_t s = 0; s < FARMESH_STEP_MAX; ++s) {
		const auto pow = s + cell_pow;
		if (pow >= std::numeric_limits<bpos_t>::digits)
			break;
		const v3bpos_t origin(
				(pos.X >> pow) << pow, (pos.Y >> pow) << pow, (pos.Z >> pow) << pow);
		const auto it = grid.find(origin);
		if (it != grid.end() && step(it->second) == s)
			return it;
	}
	return grid.end();
}

// The caller holds the visible-map lock and has finished enumerating pending.
// A coarse owner is replaced only when all requested descendants are ready.
// Coarsening similarly replaces every old descendant in one publication. The
// dyadic alignment means intersecting cells always contain one another, even
// when a camera move skips several LOD levels. Empty meshes count as ready.
template <typename Pending, typename Visible, typename Ready, typename Step,
		typename KeepOld>
bool publishReadyGrid(const Pending &pending, Visible &visible, Ready ready, Step step,
		uint8_t cell_pow, KeepOld keep_old)
{
	std::unordered_set<v3bpos_t> blocked;
	bool complete = true;
	for (const auto &[pos, mesh] : pending) {
		if (ready(mesh))
			continue;
		complete = false;
		const auto owner = findCoveringCell(visible, pos, step, cell_pow);
		if (owner != visible.end() && step(owner->second) >= step(mesh))
			blocked.insert(owner->first);
	}

	std::unordered_set<v3bpos_t> replace;
	std::vector<typename Pending::value_type> publish;
	for (const auto &entry : pending) {
		if (!ready(entry.second))
			continue;
		const auto owner = findCoveringCell(visible, entry.first, step, cell_pow);
		if (owner != visible.end() && step(owner->second) >= step(entry.second)) {
			if (blocked.contains(owner->first))
				continue;
			replace.insert(owner->first);
		}
		publish.push_back(entry);
	}
	for (auto it = visible.begin(); it != visible.end();) {
		if (blocked.contains(it->first)) {
			++it;
			continue;
		}
		const auto target = findCoveringCell(pending, it->first, step, cell_pow);
		const bool covered = target != pending.end() &&
							 step(target->second) >= step(it->second) &&
							 ready(target->second);
		if (replace.contains(it->first) || covered || (complete && !keep_old(*it)))
			it = visible.erase(it);
		else
			++it;
	}
	for (const auto &entry : publish)
		visible.insert_or_assign(entry.first, entry.second);
	return complete;
}

template <typename Pending, typename Visible, typename Ready, typename Step>
bool publishReadyGrid(const Pending &pending, Visible &visible, Ready ready, Step step,
		uint8_t cell_pow = 0)
{
	return publishReadyGrid(
			pending, visible, ready, step, cell_pow, [](const auto &) { return false; });
}

} // namespace farmesh
