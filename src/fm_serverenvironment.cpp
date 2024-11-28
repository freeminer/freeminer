#include "irr_v3d.h"
#include "map.h"
#include "scripting_server.h"
#include "serverenvironment.h"
#include "util/timetaker.h"

size_t ServerEnvironment::blockStep(MapBlockPtr block, float dtime, uint8_t activate)
{
	if (!block)
		return {};

	u32 stamp = block->getTimestamp();
	if (!dtime && m_game_time > stamp && stamp != BLOCK_TIMESTAMP_UNDEFINED)
		dtime = m_game_time - stamp;

	// Set current time as timestamp
	block->setTimestampNoChangedFlag(m_game_time);

	if (!block->m_node_timers
					.m_uptime_last) // not very good place, but minimum modifications
		block->m_node_timers.m_uptime_last = m_game_time - dtime;
	const auto dtime_n = m_game_time - block->m_node_timers.m_uptime_last;
	block->m_node_timers.m_uptime_last = m_game_time;
	// DUMP("abm random ", block->getPos(), dtime_s, dtime_n,, stamp, block->getTimestamp(), block->m_node_timers.m_uptime_last, m_game_time);

	m_lbm_mgr.applyLBMs(this, block.get(), stamp, (float)dtime_n);
	if (block->isOrphan())
		return {};

	block->step((float)dtime, [&](v3pos_t p, MapNode n, f32 d) -> bool {
		return !block->isOrphan() && m_script->node_on_timer(p, n, d);
	});

	size_t triggers_run = 0;
	if (block->abm_triggers) {
		//ScopeProfiler sp354(g_profiler, "ABM random trigger blocks", SPT_ADD);
		triggers_run = block->abmTriggersRun(this, m_game_time, activate);
	}
	return triggers_run;
}

int ServerEnvironment::analyzeBlocks(float dtime, unsigned int max_cycle_ms)
{
	u32 n = 0, calls = 0;
	const auto end_ms = porting::getTimeMs() + max_cycle_ms;
	if (m_active_block_analyzed_last || m_analyze_blocks_interval.step(dtime, 1.0)) {
		//if (!m_active_block_analyzed_last) infostream<<"Start ABM analyze cycle s="<<m_active_blocks.m_list.size()<<std::endl;
		TimeTaker timer("env: block analyze and abm apply from " +
						itos(m_active_block_analyzed_last));

		std::set<v3pos_t> active_blocks_list;
		//auto active_blocks_list = m_active_blocks.m_list;
		{
			const auto lock = m_active_blocks.m_list.try_lock_shared_rec();
			if (lock->owns_lock())
				active_blocks_list = m_active_blocks.m_list;
		}

		for (const auto &p : active_blocks_list) {
			if (n++ < m_active_block_analyzed_last)
				continue;
			else
				m_active_block_analyzed_last = 0;
			++calls;

			auto block = m_map->getBlock(p, true);
			if (!block)
				continue;

			analyzeBlock(block);

			if (porting::getTimeMs() > end_ms) {
				m_active_block_analyzed_last = n;
				break;
			}
		}
		if (!calls)
			m_active_block_analyzed_last = 0;
	}

	if (g_settings->getBool("abm_random") &&
			(!m_abm_random_blocks.empty() || m_abm_random_interval.step(dtime, 10.0))) {
		TimeTaker timer("env: random abm " + itos(m_abm_random_blocks.size()));

		const auto end_ms = porting::getTimeMs() + max_cycle_ms / 10;

		if (m_abm_random_blocks.empty()) {
#if !ENABLE_THREADS
			auto lock_map = m_map->m_nothread_locker.try_lock_shared_rec();
			if (lock_map->owns_lock())
#endif
			{
				const auto lock = m_map->m_blocks.try_lock_shared_rec();
				if (lock->owns_lock())
					for (const auto &ir : m_map->m_blocks) {
						if (!ir.second || !ir.second->abm_triggers)
							continue;
						m_abm_random_blocks.emplace_back(ir.first);
					}
			}
			//infostream<<"Start ABM random cycle s="<<m_abm_random_blocks.size()<<std::endl;
		}

		for (auto i = m_abm_random_blocks.begin(); i != m_abm_random_blocks.end(); ++i) {
			auto block = m_map->getBlock(*i, true);
			i = m_abm_random_blocks.erase(i);
			//ScopeProfiler sp221(g_profiler, "ABM random look blocks", SPT_ADD);

			blockStep(block, dtime, 1 << 1);

			if (porting::getTimeMs() > end_ms) {
				break;
			}
		}
	}

	return calls;
}

size_t ServerEnvironment::nodeUpdate(
		const v3pos_t &pos, u8 recursion_limit, u8 fast, bool destroy)
{
	std::lock_guard<std::mutex> lock(m_nodeupdate_queue_mutex);
	m_nodeupdate_queue.emplace_back(pos, recursion_limit, fast, destroy);
	return 0;
}
