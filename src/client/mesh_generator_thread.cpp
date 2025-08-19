// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013, 2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "mesh_generator_thread.h"
#include "client/clientmap.h"
#include "fm_far_calc.h"
#include "client/mapblock_mesh.h"
#include "settings.h"
#include "profiler.h"
#include "client.h"
#include "mapblock.h"
#include "map.h"
#include "util/directiontables.h"
#include "porting.h"

/*
	QueuedMeshUpdate
*/

QueuedMeshUpdate::~QueuedMeshUpdate()
{
	delete data;
}

void QueuedMeshUpdate::retrieveBlocks(Map *map, u16 cell_size)
{
	const size_t total = (cell_size+2)*(cell_size+2)*(cell_size+2);
	if (map_blocks.empty())
		map_blocks.resize(total);
	else
		assert(map_blocks.size() == total); // must not change
	size_t i = 0;
	v3s16 pos;
	for (pos.X = p.X - 1; pos.X <= p.X + cell_size; pos.X++)
	for (pos.Z = p.Z - 1; pos.Z <= p.Z + cell_size; pos.Z++)
	for (pos.Y = p.Y - 1; pos.Y <= p.Y + cell_size; pos.Y++) {
		if (!map_blocks[i]) {
			auto block = map->getBlock(pos);
			if (block) {
				block->refGrab();
				map_blocks[i] = block;
			}
		}
		i++;
	}
}

void QueuedMeshUpdate::dropBlocks()
{
	for (auto block : map_blocks) {
		if (block)
			block->refDrop();
	}
	map_blocks.clear();
}

/*
	MeshUpdateQueue
*/

MeshUpdateQueue::MeshUpdateQueue(Client *client):
	m_client(client)
{
	m_cache_smooth_lighting = g_settings->getBool("smooth_lighting");
	m_cache_enable_water_reflections = g_settings->getBool("enable_water_reflections");
}

MeshUpdateQueue::~MeshUpdateQueue()
{
	MutexAutoLock lock(m_mutex);

	for (QueuedMeshUpdate *q : m_queue) {
		q->dropBlocks();
		delete q;
	}
}

bool MeshUpdateQueue::addBlock(Map *map, v3s16 p, bool ack_block_to_server,
	bool urgent, bool from_neighbor)
{
	// FIXME: with cell_size > 1 there isn't a "main block" and this check is
	// probably incorrect and broken
	MapBlock *main_block = map->getBlockNoCreateNoEx(p);
	if (!main_block)
		return false;

	MeshGrid mesh_grid = m_client->getMeshGrid();

	// Mesh is placed at the corner block of a chunk
	// (where all coordinate are divisible by the chunk size)
	v3s16 mesh_position = mesh_grid.getMeshPos(p);

	MutexAutoLock lock(m_mutex);

	/*
		Mark the block as urgent if requested
	*/
	if (urgent)
		m_urgents.insert(mesh_position);

	/*
		Find if block is already in queue.
		If it is, update the data and quit.
	*/
	for (QueuedMeshUpdate *q : m_queue) {
		if (q->p == mesh_position) {
			if (ack_block_to_server)
				q->ack_list.push_back(p);
			q->crack_level = m_client->getCrackLevel();
			q->crack_pos = m_client->getCrackPos();
			q->urgent |= urgent;
			q->retrieveBlocks(map, mesh_grid.cell_size);
			return true;
		}
	}

	/*
		Air blocks won't suddenly become visible due to a neighbor update, so
		skip those.
		Note: this can be extended with more precise checks in the future
	*/
	if (from_neighbor && mesh_grid.cell_size == 1 && main_block->isAir()) {
		assert(!ack_block_to_server);
		m_urgents.erase(mesh_position);
		g_profiler->add("MeshUpdateQueue: updates skipped", 1);
		return true;
	}

	/*
		Add the block
	*/
	QueuedMeshUpdate *q = new QueuedMeshUpdate;
	q->p = mesh_position;
	if (ack_block_to_server)
		q->ack_list.push_back(p);
	q->crack_level = m_client->getCrackLevel();
	q->crack_pos = m_client->getCrackPos();
	q->urgent = urgent;
	q->retrieveBlocks(map, mesh_grid.cell_size);
	m_queue.push_back(q);

	return true;
}

// Returned pointer must be deleted
// Returns NULL if queue is empty
QueuedMeshUpdate *MeshUpdateQueue::pop()
{
	QueuedMeshUpdate *result = NULL;
	{
		MutexAutoLock lock(m_mutex);

		bool must_be_urgent = !m_urgents.empty();
		for (auto i = m_queue.begin(); i != m_queue.end(); ++i) {
			QueuedMeshUpdate *q = *i;
			if (must_be_urgent && m_urgents.count(q->p) == 0)
				continue;
			// Make sure no two threads are processing the same mapblock, as that causes racing conditions
			if (m_inflight_blocks.find(q->p) != m_inflight_blocks.end())
				continue;
			m_queue.erase(i);
			m_urgents.erase(q->p);
			m_inflight_blocks.insert(q->p);
			result = q;
			break;
		}
	}

	if (result)
		fillDataFromMapBlocks(result);

	return result;
}

void MeshUpdateQueue::done(v3s16 pos)
{
	MutexAutoLock lock(m_mutex);
	m_inflight_blocks.erase(pos);
}


void MeshUpdateQueue::fillDataFromMapBlocks(QueuedMeshUpdate *q)
{
	auto mesh_grid = m_client->getMeshGrid();

	const auto lod_step = getLodStep(m_client->m_env.getClientMap().getControl(),
			getNodeBlockPos(
					floatToInt(m_client->m_env.getLocalPlayer()->getPosition(), BS)),
			q->p, m_client->getEnv().getLocalPlayer()->getSpeed().getLength());

	MeshMakeData *data = new MeshMakeData(m_client->ndef(),
		MAP_BLOCKSIZE * mesh_grid.cell_size, mesh_grid,
		lod_step, 0);
	q->data = data;

	data->fillBlockDataBegin(q->p);

	v3s16 pos;
	int i = 0;
	for (pos.X = q->p.X - 1; pos.X <= q->p.X + mesh_grid.cell_size; pos.X++)
	for (pos.Z = q->p.Z - 1; pos.Z <= q->p.Z + mesh_grid.cell_size; pos.Z++)
	for (pos.Y = q->p.Y - 1; pos.Y <= q->p.Y + mesh_grid.cell_size; pos.Y++) {
		const auto block = q->map_blocks[i++];
/*
		if (block)
			block->copyTo(data->m_vmanip);
*/

        if (block) {
			const auto lock = block->lock_shared_rec();
			//data->fillBlockData(pos, block->getData());
			block->copyTo(data->m_vmanip);

			if (const auto bts = block->getTimestamp();
					bts != BLOCK_TIMESTAMP_UNDEFINED) {
				data->timestamp = std::max(data->timestamp, bts);
			}
		}


	}

	data->setCrack(q->crack_level, q->crack_pos);
	data->m_generate_minimap = !!m_client->getMinimap();
	data->m_smooth_lighting = m_cache_smooth_lighting;
	data->m_enable_water_reflections = m_cache_enable_water_reflections;

	data->range = getNodeBlockPos(floatToInt(m_client->m_env.getLocalPlayer()->getPosition(), BS)).getDistanceFrom(q->p);
}

/*
	MeshUpdateWorkerThread
*/

MeshUpdateWorkerThread::MeshUpdateWorkerThread(Client *client, MeshUpdateQueue *queue_in, MeshUpdateManager *manager) :
		UpdateThread("Mesh"), m_client(client), m_queue_in(queue_in), m_manager(manager)
{
	m_generation_interval = g_settings->getU16("mesh_generation_interval");
	m_generation_interval = rangelim(m_generation_interval, 0, 25);
}

void MeshUpdateWorkerThread::doUpdate()
{
	QueuedMeshUpdate *q;
	while ((q = m_queue_in->pop())) {
		ScopeProfiler sp(g_profiler, "Client: Mesh making (sum)");

		// This generates the mesh:
		const auto mesh_new = std::make_shared<MapBlockMesh>(m_client, q->data);

		MeshUpdateResult r;
		r.p = q->p;
		r.mesh = mesh_new;
		r.solid_sides = get_solid_sides(q->data);
		r.ack_list = std::move(q->ack_list);
		r.urgent = q->urgent;
		r.map_blocks = std::move(q->map_blocks);

		m_manager->putResult(r);
		m_queue_in->done(q->p);
		delete q;
		sp.stop();

		porting::TriggerMemoryTrim();

		// do this after we're done so the interval is enforced without
		// adding extra latency.
		if (m_generation_interval)
			sleep_ms(m_generation_interval);
	}
}

/*
	MeshUpdateManager
*/

MeshUpdateManager::MeshUpdateManager(Client *client):
	m_queue_in(client)
	, m_queue_in_urgent(client)
{
	int number_of_threads = rangelim(g_settings->getS32("mesh_generation_threads"), 0, 8);

	// Automatically use 25% of the system cores for mesh generation, max 3
	if (number_of_threads == 0)
		number_of_threads = std::min(3U, Thread::getNumberOfProcessors() / 3);

	// use at least one thread
	number_of_threads = std::max(1, number_of_threads);
	infostream << "MeshUpdateManager: using " << number_of_threads << " threads" << std::endl;

	for (int i = 0; i < number_of_threads; i++)
		m_workers.push_back(std::make_unique<MeshUpdateWorkerThread>(client, &m_queue_in, this));

	m_workers.push_back(std::make_unique<MeshUpdateWorkerThread>(client, &m_queue_in_urgent, this));
}

void MeshUpdateManager::updateBlock(Map *map, v3s16 p, bool ack_block_to_server,
		bool urgent, bool update_neighbors)
{
	if (static thread_local const bool headless_optimize =
					g_settings->getBool("headless_optimize");
			headless_optimize) {
		return;
	}

	static thread_local const bool many_neighbors =
			g_settings->getBool("smooth_lighting")
			&& !g_settings->getFlag("performance_tradeoffs");
	if (!(urgent ? m_queue_in_urgent : m_queue_in).addBlock(map, p, ack_block_to_server, urgent, false)) {
		warningstream << "Update requested for non-existent block at "
				<< p << std::endl;
		return;
	}
	if (update_neighbors) {
		if (many_neighbors) {
			for (v3s16 dp : g_26dirs)
				m_queue_in.addBlock(map, p + dp, false, urgent, true);
		} else {
			for (v3s16 dp : g_6dirs)
				m_queue_in.addBlock(map, p + dp, false, urgent, true);
		}
	}
	deferUpdate();
}

void MeshUpdateManager::putResult(const MeshUpdateResult &result)
{
	if (result.urgent)
		m_queue_out_urgent.push_back(result);
	else
		m_queue_out.push_back(result);
}

bool MeshUpdateManager::getNextResult(MeshUpdateResult &r)
{
	if (!m_queue_out_urgent.empty_try()) {
		r = m_queue_out_urgent.pop_frontNoEx();
		return true;
	}

	if (!m_queue_out.empty_try()) {
		r = m_queue_out.pop_frontNoEx();
		return true;
	}

	return false;
}

void MeshUpdateManager::deferUpdate()
{
	for (auto &thread : m_workers)
		thread->deferUpdate();
}

void MeshUpdateManager::start()
{
	for (auto &thread: m_workers)
		thread->start();
}

void MeshUpdateManager::stop()
{
	for (auto &thread: m_workers)
		thread->stop();
}

void MeshUpdateManager::wait()
{
	for (auto &thread: m_workers)
		thread->wait();
}

bool MeshUpdateManager::isRunning()
{
	for (auto &thread: m_workers)
		if (thread->isRunning())
			return true;
	return false;
}
