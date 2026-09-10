// SPDX-License-Identifier: GPL-3.0-or-later
// Included from mesh_generator_thread.cpp.

#include "fm_mesh_priority.h"
#include <algorithm>

bool MeshUpdateQueue::hasPending(const v3bpos_t &pos)
{
	MutexAutoLock lock(m_mutex);
	return m_inflight_blocks.contains(pos) ||
		   std::any_of(m_queue.begin(), m_queue.end(),
				   [&](const auto *job) { return job->p == pos; });
}

bool MeshUpdateManager::ResultQueue::hasPending(const v3bpos_t &pos)
{
	MutexAutoLock lock(m_mutex);
	return std::any_of(m_queue.begin(), m_queue.end(),
			[&](const auto &result) { return result.p == pos; });
}

bool MeshUpdateManager::hasPending(const v3bpos_t &pos)
{
	// Workers publish the result before clearing their inflight entry. Looking
	// at input queues first also covers a job completing during these checks.
	return m_queue_in.hasPending(pos) || m_queue_in_urgent.hasPending(pos) ||
		   m_queue_out.hasPending(pos) || m_queue_out_urgent.hasPending(pos);
}

void MeshUpdateQueue::prioritize(const v3bpos_t &camera)
{
	MutexAutoLock lock(m_mutex);
	sortMeshUpdatesNearFirst(m_queue, camera, [](const auto *job) { return job->p; });
}

void MeshUpdateManager::prioritize(const v3bpos_t &camera)
{
	m_queue_in.prioritize(camera);
	m_queue_in_urgent.prioritize(camera);
}

void Client::prioritizeMeshUpdates(const v3bpos_t &camera)
{
	m_mesh_update_manager->prioritize(camera);
}
