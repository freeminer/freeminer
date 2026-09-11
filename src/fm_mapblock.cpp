#include "gamedef.h"
#include "mapblock.h"
#include "nodedef.h"
#include "settings.h"
#include "servermap.h"

#if CHECK_CLIENT_BUILD()
#include "client/mapblock_mesh.h"
#include "client/node_visuals.h"
#endif

#ifndef NDEBUG
#include "profiler.h"
#endif

MapNode MapBlock::getNodeNoEx(v3pos_t p)
{
#ifndef NDEBUG
	ScopeProfiler sp(g_profiler, "Map: getNodeNoEx");
#endif
	const auto lock = lock_shared_rec();
	return getNodeNoLock(p);
}

void MapBlock::setNode(const v3pos_t &p, const MapNode &n, bool important)
{
#ifndef NDEBUG
	g_profiler->add("Map: setNode", 1);
#endif

	auto nodedef = m_gamedef->ndef();
	auto index = p.Z * zstride + p.Y * ystride + p.X;
	const auto &f1 = nodedef->get(n.getContent());

	const auto lock = lock_unique_rec();
	expandNodesIfNeeded();

	const auto &f0 = nodedef->get(data[index].getContent());

	data[index] = n;

	modified_light light = modified_light_no;
	if (f0.light_propagates != f1.light_propagates ||
#if CHECK_CLIENT_BUILD() // TODO use on server
			f0.visuals->solidness != f1.visuals->solidness ||
#endif
			f0.light_source != f1.light_source) /*|| f0.drawtype != f1.drawtype*/
		light = modified_light_yes;
	if (important)
		raiseModified(MOD_STATE_WRITE_NEEDED, light, important);
}

void MapBlock::raiseModified(u32 mod, modified_light light, bool important)
{
	static const thread_local auto save_changed_block =
			g_settings->getBool("save_changed_block");

	if (mod >= MOD_STATE_WRITE_NEEDED /*&& m_timestamp != BLOCK_TIMESTAMP_UNDEFINED*/) {
		m_changed_timestamp = (unsigned int)ServerMap::time_life;
	}

	if (mod > m_modified) {
		if (save_changed_block || important ||
				m_disk_timestamp != BLOCK_TIMESTAMP_UNDEFINED)
			m_modified = mod;
		if (m_modified >= MOD_STATE_WRITE_AT_UNLOAD)
			m_disk_timestamp.store(m_timestamp);
	}
	if (light == modified_light_yes) {
		setLightingComplete(0);
	}
}

void MapBlock::pushElementsToCircuit(Circuit *circuit)
{
}

bool MapBlock::analyzeContent()
{
	/*
    // TODO: really need here?

	const auto lock = try_lock_shared_rec();
	if (!lock->owns_lock())
		return false;
	content_only = data[0].param0;
	content_only_param1 = data[0].param1;
	content_only_param2 = data[0].param2;
	if (m_is_mono_block)
		return true;
	for (int i = 1; i < MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE; ++i) {
		if (data[i].param0 != content_only || data[i].param1 != content_only_param1 ||
				data[i].param2 != content_only_param2) {
			content_only = CONTENT_IGNORE;
			break;
		}
	}
	return true;
	*/
	return true;
}

const MapBlock::mesh_type empty_mesh;
#if CHECK_CLIENT_BUILD()
const MapBlock::mesh_type MapBlock::getLodMesh(block_step_t step, bool allow_other)
{
	auto m = m_lod_mesh[step].load();
	if (m || !allow_other)
		return m;

	for (size_t inc = 1; inc < 4; ++inc) {
		if (step + inc < m_lod_mesh.size()) {
			if (auto mn = m_lod_mesh[step + inc].load()) {
				return mn;
			}
		}
		if (inc <= step) {
			if (auto mp = m_lod_mesh[step - inc].load()) {
				return mp;
			}
		}
	}
	return empty_mesh;
}

const MapBlock::mesh_type MapBlock::getFarMesh(block_step_t step)
{
	return m_far_mesh[step];
}

void MapBlock::setLodMesh(const MapBlock::mesh_type &rmesh)
{
	const auto step = rmesh->lod_step;
	delete_mesh = m_lod_mesh[step].exchange(rmesh);
}

void MapBlock::clearLodMesh(block_step_t step)
{
	delete_mesh = m_lod_mesh[step].exchange(nullptr);
}

void MapBlock::setFarMesh(const MapBlock::mesh_type &rmesh, block_step_t step)
{
	delete_mesh = m_far_mesh[step].exchange(rmesh);
}

MapBlock::mesh_revision_t MapBlock::getMeshRevision() const
{
	return m_mesh_revision.load(std::memory_order_acquire);
}
void MapBlock::updateMeshRevision(mesh_revision_t revision)
{
	auto current = m_mesh_revision.load(std::memory_order_relaxed);
	while (current < revision &&
			!m_mesh_revision.compare_exchange_weak(current, revision,
					std::memory_order_release, std::memory_order_relaxed)) {
	}
}
bool MapBlock::tryMarkMeshRequested(block_step_t step, mesh_revision_t revision)
{
	assert(step <= LODMESH_STEP_MAX);
	assert(revision < (uint64_t{1} << 56));

	const uint64_t request = (revision << 8) | step;
	auto current = m_mesh_requested.load(std::memory_order_relaxed);
	for (;;) {
		const auto current_revision = current >> 8;
		if (current_revision > revision || current == request)
			return false;
		if (m_mesh_requested.compare_exchange_weak(current, request,
					std::memory_order_acq_rel, std::memory_order_relaxed))
			return true;
	}
}

#endif

void MapBlock::incrementUsageTimer(float dtime)
{
	std::lock_guard<std::mutex> lock(m_usage_timer_mutex);
	m_usage_timer += dtime * usage_timer_multiplier;
}

void MapBlock::setNodeNoLock(v3pos_t p, MapNode n, bool important)
{
	expandNodesIfNeeded();
	data[p.Z * zstride + p.Y * ystride + p.X] = n;
	raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_SET_NODE, important);
}

MapNode &MapBlock::getNodeRef(const v3pos_t &p)
{
	const auto lock = try_lock_shared_rec();
	if (!lock->owns_lock())
		return ignoreNode;
	return getNodeNoLock(p);
}

MapNode MapBlock::getNodeTry(const v3pos_t &p)
{
	const auto lock = try_lock_shared_rec();
	if (!lock->owns_lock())
		return ignoreNode;
	return getNodeNoLock(p);
}

u32 MapBlock::getActualTimestamp()
{
	u32 block_timestamp = 0;
	if (m_changed_timestamp && m_changed_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
		block_timestamp = m_changed_timestamp;
	} else if (m_disk_timestamp && m_disk_timestamp != BLOCK_TIMESTAMP_UNDEFINED) {
		block_timestamp = m_disk_timestamp;
	}
	return block_timestamp;
}
MapBlock::light_t MapBlock::makeLightPoint(u8 level, video::SColor color)
{
	if (level > LIGHT_MAX)
		level = LIGHT_MAX;

	return (static_cast<light_t>(level) << 24) |
		   (static_cast<light_t>(color.getRed()) << 16) |
		   (static_cast<light_t>(color.getGreen()) << 8) |
		   static_cast<light_t>(color.getBlue());
}
u8 MapBlock::getLightPointLevel(light_t light)
{
	if (light <= LIGHT_SUN)
		return static_cast<u8>(light);

	const auto level = static_cast<u8>((light >> 24) & 0xff);
	return level > LIGHT_MAX ? LIGHT_MAX : level;
}
video::SColor MapBlock::getLightPointColor(light_t light)
{
	if (light <= LIGHT_SUN)
		return video::SColor(255, 255, 255, 255);

	return video::SColor(255, (light >> 16) & 0xff, (light >> 8) & 0xff, light & 0xff);
}

inline std::string analyze_block(const MapBlockPtr &block)
{
	return analyze_block(block.get());
};
