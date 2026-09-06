// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <chrono>
#include <future>
#include <vector>

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

// The caller holds the visible-map lock. Check every required cell before
// replacing anything: an empty completed mesh is ready, a missing mesh is not.
// Whole-grid replacement also handles changes spanning multiple octree levels.
template <typename Pending, typename Visible, typename Ready>
bool publishReadyGrid(const Pending &pending, Visible &visible, Ready ready)
{
	if (!std::all_of(pending.begin(), pending.end(),
				[&](const auto &entry) { return ready(entry.second); }))
		return false;
	visible.clear();
	visible.insert(pending.begin(), pending.end());
	return true;
}

} // namespace farmesh
