/*
Copyright (C) 2024 proller <proler@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "database/database.h"
#include "debug.h"
#include "fm_server.h"
#include "profiler.h"
#include "settings.h"
#include "server.h"
#include "porting.h"
#include "util/directiontables.h"

AbmWorldThread::AbmWorldThread(Server *server) :
		thread_vector("AbmWorld", 20), m_server(server)
{
}

void *AbmWorldThread::run()
{
	BEGIN_DEBUG_EXCEPTION_HANDLER

	{
		u64 abm_world = 0;
		g_settings->getU64NoEx("abm_world", abm_world);
		if (!abm_world)
			return nullptr;
	}

	int16_t abm_world_load_all = -1; // -1 : auto;  0 : disable;   1 : force
	g_settings->getS16NoEx("abm_world_load_all", abm_world_load_all);
	u64 abm_world_throttle = m_server->isSingleplayer() ? 10 : 0;
	g_settings->getU64NoEx("abm_world_throttle", abm_world_throttle);
	u64 abm_world_max_clients = m_server->isSingleplayer() ? 1 : 0;
	g_settings->getU64NoEx("abm_world_max_clients", abm_world_max_clients);
	u64 abm_world_max_blocks = m_server->isSingleplayer() ? 2000 : 10000;
	g_settings->getU64NoEx("abm_world_max_blocks", abm_world_max_blocks);

	auto &abm_world_last = m_server->getEnv().abm_world_last;

	const auto can_work = [&]() {
		return (m_server->getEnv().getPlayerCount() <= abm_world_max_clients &&
				m_server->getMap().m_blocks.size() <= abm_world_max_blocks);
	};

	int32_t run = 0;
	size_t pos_dir; // random start

	while (!stopRequested()) {
		++run;

		if (!can_work()) {
			tracestream << "Abm world wait" << '\n';
			sleep(10);
			continue;
		}

		std::vector<v3bpos_t> loadable_blocks;

		auto time_start = porting::getTimeMs();

		if (abm_world_load_all <= 0) {
// Yes, very bad
#if USE_LEVELDB
			if (const auto it =
							m_server->getEnv().blocks_with_abm.database.new_iterator();
					it) {
				for (it->SeekToFirst(); it->Valid(); it->Next()) {
					const auto &key = it->key().ToString();
					if (key.starts_with("a")) {
						const v3bpos_t pos = MapDatabase::getStringAsBlock(key);
						loadable_blocks.emplace_back(pos);
					}
				}
			}
#endif
		}

		// Load whole world firts time, fill blocks_with_abm
		if (abm_world_load_all && loadable_blocks.empty()) {
			actionstream << "Abm world full load" << '\n';
			m_server->getEnv().getServerMap().listAllLoadableBlocks(loadable_blocks);
		}

		std::map<bpos_t, std::map<bpos_t, std::set<bpos_t>>> volume;

		size_t cur_n = 0;

		const auto loadable_blocks_size = loadable_blocks.size();
		infostream << "Abm world run " << run << " blocks " << loadable_blocks_size
				   << " per " << (porting::getTimeMs() - time_start) / 1000 << "s from "
				   << abm_world_last << " max_clients " << abm_world_max_clients
				   << " throttle " << abm_world_throttle << " vxs " << volume.size()
				   << '\n';
		size_t processed = 0, triggers_total = 0;

		time_start = porting::getTimeMs();

		const auto printstat = [&]() {
			auto time = porting::getTimeMs();

			infostream << "Abm world run " << run << " " << cur_n << "/"
					   << loadable_blocks_size << " blocks loaded "
					   << m_server->getMap().m_blocks.size() << " processed " << processed
					   << " triggers " << triggers_total << " per "
					   << (time - time_start) / 1000 << " speed "
					   << processed / (((time - time_start) / 1000) ?: 1) << " vxs "
					   << volume.size() << '\n';
		};

#if 1
		for (const auto &pos : loadable_blocks) {
			volume[pos.X][pos.Y].emplace(pos.Z);
		}

		const auto contains = [&](const v3bpos_t &pos) -> bool {
			if (!volume.contains(pos.X))
				return false;
			if (!volume[pos.X].contains(pos.Y))
				return false;
			return volume[pos.X][pos.Y].contains(pos.Z);
		};

		const auto erase = [&](const v3bpos_t &pos) {
			if (!volume.contains(pos.X))
				return;
			if (!volume[pos.X].contains(pos.Y))
				return;
			if (!volume[pos.X][pos.Y].contains(pos.Z))
				return;
			volume[pos.X][pos.Y].erase(pos.Z);
			if (volume[pos.X][pos.Y].empty())
				volume[pos.X].erase(pos.Y);
			if (volume[pos.X].empty())
				volume.erase(pos.X);
		};

		std::optional<v3bpos_t> pos_opt;
		while (!volume.empty()) {
			if (pos_opt.has_value()) {
				const auto pos_old = pos_opt.value();
				pos_opt.reset();
				// Random better
				for (size_t dirs = 0; dirs < 6; ++dirs, ++pos_dir) {
					const auto pos_new = pos_old + g_6dirs[pos_dir % sizeof(g_6dirs)];
					//DUMP(dirs, pos_new, pos_dir);
					if (contains(pos_new)) {
						//DUMP("ok", dirs, pos_opt, "->", pos_new);
						pos_opt = pos_new;
						break;
					}
				}
			}

			if (!pos_opt.has_value()) {
				// always first: pos_opt = {volume.begin()->first,volume.begin()->second.begin()->first,*volume.begin()->second.begin()->second.begin()};

				auto xend = volume.end();
				--xend;
				const auto xi = pos_dir & 1 ? volume.begin() : xend;
				auto yend = xi->second.end();
				--yend;
				const auto yi = pos_dir & 2 ? xi->second.begin() : yend;
				auto zend = yi->second.end();
				--zend;
				const auto zi = pos_dir & 4 ? yi->second.begin() : zend;
				pos_opt = {xi->first, yi->first, *zi};
			}
			const auto pos = pos_opt.value();
			erase(pos);
			++cur_n;

#else
		cur_n = 0;
		for (const auto &pos : loadable_blocks) {
			++cur_n;

			if (cur_n < abm_world_last) {
				continue;
			}
			abm_world_last = cur_n;
#endif

			if (stopRequested()) {
				return nullptr;
			}
			try {
				const auto load_block = [&](const v3bpos_t &pos) -> MapBlockPtr {
					auto block = m_server->getEnv().getServerMap().getBlock(pos);
					if (block) {
						return block;
					}
					block.reset(m_server->getEnv().getServerMap().emergeBlock(pos));
					if (!block) {
						return nullptr;
					}
					if (!block->isGenerated()) {
						return nullptr;
					}
					return block;
				};

				auto block = load_block(pos);
				if (!block) {
					continue;
				}

				// Load neighbours for better liquids flows
				for (const auto &dir : g_6dirs) {
					load_block(pos + dir);
				}

				g_profiler->add("Server: Abm world blocks", 1);

				++processed;

				//m_server->getEnv().activateBlock(block);

				const auto activate = (1 << 2) | m_server->getEnv().analyzeBlock(block);
				const auto triggers = m_server->getEnv().blockStep(block, 0, activate);
				triggers_total += triggers;

				//DUMP("ok", pos, cur_n, m_server->getMap().m_blocks.size(), block->getTimestamp(), block->getActualTimestamp(), m_server->getEnv().getGameTime(), triggers);

				if (!(cur_n % 10000)) {
					printstat();
				}

				if (!can_work()) {
					tracestream << "Abm world throttle" << '\n';

					std::this_thread::sleep_for(std::chrono::seconds(1));
				} else if (abm_world_throttle) {
					std::this_thread::sleep_for(
							std::chrono::milliseconds(abm_world_throttle));
				}

#if !EXCEPTION_DEBUG
			} catch (const std::exception &e) {
				errorstream << m_name << ": exception: " << e.what() << "\n"
							<< stacktrace() << '\n';
			} catch (...) {
				errorstream << m_name << ": Unknown unhandled exception at "
							<< __PRETTY_FUNCTION__ << ":" << __LINE__ << '\n'
							<< stacktrace() << '\n';
#else
			} catch (int) { // nothing
#endif
			}
		}
		printstat();
		abm_world_last = 0;

		sleep(60);
	}
	END_DEBUG_EXCEPTION_HANDLER
	return nullptr;
}
