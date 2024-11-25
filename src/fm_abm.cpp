#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include "irr_v3d.h"
#include "map.h"
#include "profiler.h"
#include "server.h"
#include "server/abmhandler.h"
#include "serverenvironment.h"

ABMHandler::ABMHandler(ServerEnvironment *env) : m_env(env)
{
	m_aabms.fill(nullptr);
}

void ABMHandler::init(std::vector<ABMWithState> &abms)
{
	for (auto &abmws : abms) {
		auto i = &abmws;
		ActiveABM aabm;
		aabm.abmws = i;

		aabm.min_y = i->abm->getMinY();
		aabm.max_y = i->abm->getMaxY();

		// Trigger contents
		for (auto &c : i->trigger_ids) {
			if (!m_aabms[c]) {
				m_aabms[c] = new std::vector<ActiveABM>;
				m_aabms_list.emplace_back(m_aabms[c]);
			}
			m_aabms[c]->emplace_back(aabm);
			m_aabms_empty = false;
		}
	}
}

ABMHandler::~ABMHandler()
{
	for (auto i = m_aabms_list.begin(); i != m_aabms_list.end(); ++i)
		delete *i;
}

// Find out how many objects the given block and its neighbours contain.
// Returns the number of objects in the block, and also in 'wider' the
// number of objects in the block and all its neighbours. The latter
// may an estimate if any neighbours are unloaded.
u32 ABMHandler::countObjects(MapBlock *block, ServerMap *map, u32 &wider)
{
	wider = 0;
	u32 wider_unknown_count = 0;
	for (s16 x = -1; x <= 1; x++)
		for (s16 y = -1; y <= 1; y++)
			for (s16 z = -1; z <= 1; z++) {
				MapBlock *block2 = map->getBlockNoCreateNoEx(
						block->getPos() + v3bpos_t(x, y, z), true);
				if (block2 == NULL) {
					wider_unknown_count++;
					continue;
				}
				const auto lock = block2->m_static_objects.m_active.lock_shared_rec();
				wider += block2->m_static_objects.m_active.size() +
						 block2->m_static_objects.m_stored.size();
			}
	// Extrapolate
	u32 active_object_count = block->m_static_objects.m_active.size();
	u32 wider_known_count = 3 * 3 * 3 - wider_unknown_count;
	if (wider_known_count)
		wider += wider_unknown_count * wider / wider_known_count;
	return active_object_count;
}

void ABMHandler::apply(MapBlock *block, uint8_t activate)
{
	if (m_aabms_empty)
		return;

	// infostream<<"ABMHandler::apply p="<<block->getPos()<<" block->abm_triggers="<<block->abm_triggers<<std::endl;
	{
		std::lock_guard<std::mutex> lock(block->abm_triggers_mutex);
		if (block->abm_triggers)
			block->abm_triggers->clear();
	}

#if ENABLE_THREADS
	auto map = std::unique_ptr<VoxelManipulator>(new VoxelManipulator);
	{
		// ScopeProfiler sp(g_profiler, "ABM copy", SPT_ADD);
		m_env->getServerMap().copy_27_blocks_to_vm(block, *map);
	}
#else
	ServerMap *map = &m_env->getServerMap();
#endif

	{
		// const auto lock = block->try_lock_unique_rec();
		// if (!lock->owns_lock())
		//	return;
	}

	ScopeProfiler sp(g_profiler, "ABM select", SPT_ADD);

	u32 active_object_count_wider;
	u32 active_object_count =
			this->countObjects(block, &m_env->getServerMap(), active_object_count_wider);
	m_env->m_added_objects = 0;

	auto *ndef = m_env->getGameDef()->ndef();

#if !ENABLE_THREADS
	auto lock_map = m_env->getServerMap().m_nothread_locker.try_lock_shared_rec();
	if (!lock_map->owns_lock())
		return;
#endif

	int heat_num = 0;
	int heat_sum = 0;
	int humidity_num = 0;

	v3pos_t bpr = block->getPosRelative();
	v3pos_t p0;
	for (p0.X = 0; p0.X < MAP_BLOCKSIZE; p0.X++)
		for (p0.Y = 0; p0.Y < MAP_BLOCKSIZE; p0.Y++)
			for (p0.Z = 0; p0.Z < MAP_BLOCKSIZE; p0.Z++) {
				v3pos_t p = p0 + bpr;
#if ENABLE_THREADS
				MapNode n = map->getNodeTry(p);
#else
				MapNode n = block->getNodeTry(p0);
#endif
				content_t c = n.getContent();
				if (c == CONTENT_IGNORE)
					continue;

				{
					int hot = ((ItemGroupList)ndef->get(n).groups)["hot"];
					// todo: int cold = ((ItemGroupList) ndef->get(n).groups)["cold"];
					if (hot) {
						++heat_num;
						heat_sum += hot;
					}

					int humidity = ((ItemGroupList)ndef->get(n).groups)["water"];
					if (humidity) {
						++humidity_num;
					}
				}

				if (!m_aabms[c]) {
					if (block->content_only != CONTENT_IGNORE)
						return;
					continue;
				}

				for (auto &ir : *(m_aabms[c])) {
					auto i = &ir;
					// Check neighbors
					v3pos_t neighbor_pos;
					auto &required_neighbors =
							activate == 1 ? ir.abmws->required_neighbors_activate
									 : ir.abmws->required_neighbors;
					if (required_neighbors.count() > 0) {
						v3pos_t p1;
						int neighbors_range = i->abmws->neighbors_range;
						for (p1.X = p.X - neighbors_range; p1.X <= p.X + neighbors_range;
								++p1.X)
							for (p1.Y = p.Y - neighbors_range;
									p1.Y <= p.Y + neighbors_range; ++p1.Y)
								for (p1.Z = p.Z - neighbors_range;
										p1.Z <= p.Z + neighbors_range; ++p1.Z) {
									if (p1 == p)
										continue;
									MapNode n = map->getNodeTry(p1);
									content_t c = n.getContent();
									if (c == CONTENT_IGNORE)
										continue;
									if (required_neighbors.get(c)) {
										neighbor_pos = p1;
										goto neighbor_found;
									}
								}
						// No required neighbor found
						continue;
					}
				neighbor_found:

					std::lock_guard<std::mutex> lock(block->abm_triggers_mutex);

					if (!block->abm_triggers)
						block->abm_triggers =
								std::make_unique<MapBlock::abm_triggers_type>();

					block->abm_triggers->emplace_back(
							abm_trigger_one{i, p, c, active_object_count,
									active_object_count_wider, neighbor_pos, activate});
				}
			}
	if (heat_num) {
		float heat_avg = heat_sum / heat_num;
		const int min = 2 * MAP_BLOCKSIZE;
		float magic = heat_avg >= 1 ? min + (1024 - min) / (4096 / heat_avg) : min;
		float heat_add = ((block->heat < 0 ? -block->heat : 0) + heat_avg) *
						 (heat_num < magic ? heat_num / magic : 1);
		if (block->heat > heat_add) {
			block->heat_add = 0;
		} else if (block->heat + heat_add > heat_avg) {
			block->heat_add = heat_avg - block->heat;
		} else {
			block->heat_add = heat_add;
		}
		// infostream<<"heat_num=" << heat_num << " heat_sum="<<heat_sum<<" heat_add="<<heat_add << " bheat_add"<<block->heat_add<< " heat_avg="<<heat_avg  << " heatnow="<<block->heat<< " magic="<<magic << std::endl;
	}

	const float max_effect = 70;
	if (humidity_num && block->humidity < max_effect) {
		const float max_nodes = 4 * MAP_BLOCKSIZE;
		float humidity_add = (max_effect - block->humidity) *
							 (std::min<int>(humidity_num, max_nodes) / max_nodes);
		if (block->humidity + humidity_add > max_effect) {
			block->humidity_add = block->humidity - humidity_add;
		} else {
			block->humidity_add = humidity_add;
		}
		// infostream<<"humidity_num=" << humidity_num <<" humidity_add="<<humidity_add << " bhumidity_add"<<block->humidity_add<< " humiditynow="<<block->humidity<< std::endl;
	}

	// infostream<<"ABMHandler::apply reult p="<<block->getPos()<<" apply result:"<< (block->abm_triggers ? block->abm_triggers->size() : 0) <<std::endl;
}

size_t MapBlock::abmTriggersRun(ServerEnvironment *m_env, u32 time, uint8_t activate)
{
	ScopeProfiler sp(g_profiler, "ABM trigger blocks", SPT_ADD);

	std::unique_lock<std::mutex> lock(abm_triggers_mutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};

	if (!abm_triggers)
		return {};

	ServerMap *map = &m_env->getServerMap();

#if !ENABLE_THREADS
	auto lock_map = m_env->getServerMap().m_nothread_locker.try_lock_shared_rec();
	if (!lock_map->owns_lock())
		return;
#endif

	float dtime = 0;
	if (m_abm_timestamp) {
		dtime = time - m_abm_timestamp;
	} else {
		u32 ts = getActualTimestamp();
		if (ts)
			dtime = time - ts;
		else
			dtime = 1;
	}
	if (!dtime)
		dtime = 1;
	size_t triggers_count = 0;
	unordered_map_v3bpos<int> active_object_added;

	// infostream<<"MapBlock::abmTriggersRun " << " abm_triggers="<<abm_triggers.get()<<" size()="<<abm_triggers->size()<<" time="<<time<<" dtime="<<dtime<<" activate="<<activate<<std::endl;
	m_abm_timestamp = time;
	for (auto abm_trigger = abm_triggers->begin(); abm_trigger != abm_triggers->end();
			++abm_trigger) {
		// ScopeProfiler sp2(g_profiler, "ABM trigger nodes test", SPT_ADD);
		auto &aabm = *abm_trigger->abm;
		if (!abm_trigger->abm || !aabm.abmws || !aabm.abmws->interval) {
			infostream << "remove strange abm trigger dtime=" << dtime << '\n';
			abm_trigger = abm_triggers->erase(abm_trigger);
			continue;
		}

		const auto &p = abm_trigger->pos;

		/*if ((p.Y < aabm.min_y) || (p.Y > aabm.max_y))
			continue;*/
		if ((p.Y < aabm.abmws->abm->getMinY()) || (p.Y > aabm.abmws->abm->getMaxY()))
			continue;

		float intervals = dtime / aabm.abmws->interval;

		if (!aabm.abmws->simple_catchup)
			intervals = 1;

		if (!intervals) {
			verbosestream << "abm: intervals=" << intervals << " dtime=" << dtime << '\n';
			intervals = 1;
		}
		int chance = (aabm.abmws->chance / intervals);
		// infostream<<"TST: dtime="<<dtime<<" Achance="<<abm->abmws->chance<<"
		// Ainterval="<<abm->abmws->interval<< " Rchance="<<chance<<"
		// Rintervals="<<intervals << std::endl;
		if (chance && myrand() % chance)
			continue;
		// infostream<<"HIT! dtime="<<dtime<<" Achance="<<abm->abmws->chance<<"
		// Ainterval="<<abm->abmws->interval<< " Rchance="<<chance<<"
		// Rintervals="<<intervals << std::endl;

		MapNode node = map->getNodeTry(abm_trigger->pos);
		if (node.getContent() != abm_trigger->content) {
			if (node)
				abm_trigger = abm_triggers->erase(abm_trigger);
			continue;
		}
		// ScopeProfiler sp3(g_profiler, "ABM trigger nodes call", SPT_ADD);
		const auto blockpos = getNodeBlockPos(abm_trigger->pos);
		int active_object_add = 0;
		if (active_object_added.count(blockpos))
			active_object_add = active_object_added[blockpos];
		aabm.abmws->abm->trigger(m_env, abm_trigger->pos, node,
				abm_trigger->active_object_count + active_object_add,
				abm_trigger->active_object_count_wider + active_object_add,
				abm_trigger->neighbor_pos, activate);
		++triggers_count;
		// Count surrounding objects again if the abms added any
		// infostream<<" m_env->m_added_objects="<<m_env->m_added_objects<<"
		// add="<<active_object_add<<"
		// bp="<<getNodeBlockPos(abm_trigger->pos)<<std::endl;
		if (m_env->m_added_objects > 0) {
			auto block = map->getBlock(blockpos);
			if (block) {
				auto was = abm_trigger->active_object_count;
				abm_trigger->active_object_count = m_env->m_abmhandler.countObjects(
						block.get(), map, abm_trigger->active_object_count_wider);
				// infostream<<" was="<<was<<" now
				// abm_trigger->active_object_count="<<abm_trigger->active_object_count<<std::endl;
				if (abm_trigger->active_object_count > was)
					active_object_added[blockpos] =
							abm_trigger->active_object_count - was;
			}
			m_env->m_added_objects = 0;
		}
	}
	if (abm_triggers->empty())
		abm_triggers.reset();

	if (triggers_count) {
		std::stringstream key;
		key << "a" << getPos().X << "," << getPos().Y << "," << getPos().Z;
		m_env->blocks_with_abm.put(key.str(), std::to_string(time));
	}

	return triggers_count;
}

uint8_t ServerEnvironment::analyzeBlock(MapBlockPtr block)
{
	u32 block_timestamp = block->getActualTimestamp();
	if (block->m_next_analyze_timestamp > block_timestamp) {
		// infostream<<"not anlalyzing: "<< block->getPos() <<"ats="<<block->m_next_analyze_timestamp<< " bts="<< block_timestamp<<std::endl;
		return {};
	}
	ScopeProfiler sp(g_profiler, "ABM analyze", SPT_ADD);
	if (!block->analyzeContent())
		return {};
	uint8_t activate = block_timestamp - block->m_next_analyze_timestamp > 3600 ? 1 : 0;
	m_abmhandler.apply(block.get(), activate);
	// infostream<<"ServerEnvironment::analyzeBlock p="<<block->getPos()<< " tdiff="<<block_timestamp - block->m_next_analyze_timestamp <<" co="<<block->content_only <<" triggers="<<(block->abm_triggers ? block->abm_triggers->size() : -1) <<std::endl;
	block->m_next_analyze_timestamp = block_timestamp + 2;
	return activate;
}
