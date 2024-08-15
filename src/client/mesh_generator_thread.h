/*
Minetest
Copyright (C) 2013, 2017 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <ctime>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "irr_v3d.h"
#include "mapblock_mesh.h"
#include "threading/mutex_auto_lock.h"
#include "util/thread.h"
#include <vector>
#include <memory>
#include <unordered_map>

struct QueuedMeshUpdate
{
	v3bpos_t p = v3bpos_t(-1337, -1337, -1337);
	std::vector<v3bpos_t> ack_list;
	int crack_level = -1;
	v3pos_t crack_pos;
	MeshMakeData *data = nullptr; // This is generated in MeshUpdateQueue::pop()
	std::vector<MapBlock *> map_blocks;
	bool urgent = false;

	QueuedMeshUpdate() = default;
	~QueuedMeshUpdate();
};

/*
	A thread-safe queue of mesh update tasks and a cache of MapBlock data
*/
class MeshUpdateQueue
{
	enum UpdateMode
	{
		FORCE_UPDATE,
		SKIP_UPDATE_IF_ALREADY_CACHED,
	};

public:
	MeshUpdateQueue(Client *client);

	~MeshUpdateQueue();

	// Caches the block at p and its neighbors (if needed) and queues a mesh
	// update for the block at p
	bool addBlock(Map *map, v3bpos_t p, bool ack_block_to_server, bool urgent);

	// Returned pointer must be deleted
	// Returns NULL if queue is empty
	QueuedMeshUpdate *pop();

	// Marks a position as finished, unblocking the next update
	void done(v3pos_t pos);

	u32 size()
	{
		MutexAutoLock lock(m_mutex);
		return m_queue.size();
	}

private:
	Client *m_client;
	std::vector<QueuedMeshUpdate *> m_queue;
	std::unordered_set<v3bpos_t> m_urgents;
	std::unordered_set<v3bpos_t> m_inflight_blocks;
	std::mutex m_mutex;

	// TODO: Add callback to update these when g_settings changes
	bool m_cache_enable_shaders;
	bool m_cache_smooth_lighting;

	void fillDataFromMapBlocks(QueuedMeshUpdate *q);
	void cleanupCache();
};

struct MeshUpdateResult
{
	v3bpos_t p = v3bpos_t(-1338, -1338, -1338);
	MapBlockMesh *mesh = nullptr;
	u8 solid_sides;
	std::vector<v3bpos_t> ack_list;
	bool urgent = false;
	std::vector<MapBlock *> map_blocks;

	MeshUpdateResult() = default;
};

class MeshUpdateManager;

class MeshUpdateWorkerThread : public UpdateThread
{
public:
	MeshUpdateWorkerThread(Client *client, MeshUpdateQueue *queue_in, MeshUpdateManager *manager, v3pos_t *camera_offset);

protected:
	virtual void doUpdate();

private:
	Client *m_client;
	MeshUpdateQueue *m_queue_in;
	MeshUpdateManager *m_manager;
	v3pos_t *m_camera_offset;

	// TODO: Add callback to update these when g_settings changes
	int m_generation_interval;
};

class MeshUpdateManager
{
public:
	MeshUpdateManager(Client *client);

	// Caches the block at p and its neighbors (if needed) and queues a mesh
	// update for the block at p
	void updateBlock(Map *map, v3bpos_t p, bool ack_block_to_server, bool urgent,
			bool update_neighbors = false);
	void putResult(const MeshUpdateResult &r);
	bool getNextResult(MeshUpdateResult &r);


	v3pos_t m_camera_offset;

	void start();
	void stop();
	void wait();

	bool isRunning();

private:
	void deferUpdate();


	MeshUpdateQueue m_queue_in;
	MutexedQueue<MeshUpdateResult> m_queue_out;
	MutexedQueue<MeshUpdateResult> m_queue_out_urgent;

	std::vector<std::unique_ptr<MeshUpdateWorkerThread>> m_workers;
};
