#include "client.h"
#include "clientmap.h"
#include "emerge.h"
#include "irr_v3d.h"
#include "mapblock.h"
#include "network/fm_networkprotocol.h"
#include "server.h"
#include "filesys.h"
#include "map.h"
#include "mapgen/mapgen.h"
#include "network/networkpacket.h"
#include "util/directiontables.h"
#include "util/hex.h"

void Client::updateMeshTimestampWithEdge(const v3bpos_t &blockpos)
{
	for (const auto &dir : g_7dirs) {
		auto *block = m_env.getMap().getBlockNoCreateNoEx(blockpos + dir);
		if (!block)
			continue;
		block->setTimestampNoChangedFlag(m_uptime);
	}

	/*int to = FARMESH_STEP_MAX;
	for (int step = 1; step <= to; ++step) {
		v3pos_t actualpos = getFarmeshActual(blockpos, step);
		auto *block = m_env.getMap().getBlockNoCreateNoEx(actualpos); // todo maybe update bp1 too if differ
		if(!block)
			continue;
		block->setTimestampNoChangedFlag(m_uptime);
	}*/
}

void Client::sendInitFm()
{
	MSGPACK_PACKET_INIT((int)TOSERVER_INIT_FM, 1);

	PACK(TOSERVER_INIT_FM_VERSION, 1);

	NetworkPacket pkt(TOSERVER_INIT_FM, buffer.size());
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}

void Client::sendGetBlocks()
{
	const auto &far_blocks = *m_env.getClientMap().m_far_blocks_use;
	if (far_blocks.empty()) {
		return;
	}

	MSGPACK_PACKET_INIT((int)TOSERVER_GET_BLOCKS, 1);

	DUMP("send", far_blocks.size());
	PACK(TOSERVER_GET_BLOCKS_BLOCKS,
			static_cast<std::remove_reference_t<decltype(far_blocks)>::full_type>(
					far_blocks));

	NetworkPacket pkt(TOSERVER_GET_BLOCKS, buffer.size());
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}

void Client::handleCommand_FreeminerInit(NetworkPacket *pkt)
{
	if (!pkt->packet)
		if (!pkt->packet_unpack())
			return;

	auto &packet = *(pkt->packet);

	if (!m_world_path.empty() && packet.count(TOCLIENT_INIT_GAMEID)) {
		std::string gameid;
		packet[TOCLIENT_INIT_GAMEID].convert(gameid);
		std::string conf_path = m_world_path + DIR_DELIM + "world.mt";
		Settings conf;
		conf.readConfigFile(conf_path.c_str());
		conf.set("gameid", gameid);
		conf.updateConfigFile(conf_path.c_str());
	}

	const thread_local static auto farmesh_range = g_settings->getS32("farmesh");

	if (farmesh_range && !m_localserver) {
		m_localserver = std::make_unique<Server>(
				"farmesh", findSubgame("devtest"), false, Address{}, true);
	}

	{
		Settings settings;
		packet[TOCLIENT_INIT_MAP_PARAMS].convert(settings);

		std::string mg_name;
		MapgenType mgtype = settings.getNoEx("mg_name", mg_name)
									? Mapgen::getMapgenType(mg_name)
									: MAPGEN_DEFAULT;

		if (mgtype == MAPGEN_INVALID) {
			errorstream << "Client map save: mapgen '" << mg_name
						<< "' not valid; falling back to "
						<< Mapgen::getMapgenName(MAPGEN_DEFAULT) << std::endl;
			mgtype = MAPGEN_DEFAULT;
		}

		m_mapgen_params =
				std::unique_ptr<MapgenParams>(Mapgen::createMapgenParams(mgtype));
		m_mapgen_params->MapgenParams::readParams(&settings);
		m_mapgen_params->readParams(&settings);

		if (!m_simple_singleplayer_mode && farmesh_range) {
			const auto num_emerge_threads = g_settings->get("num_emerge_threads");
			g_settings->set("num_emerge_threads", "1");
			m_emerge = std::make_unique<EmergeManager>(
					m_localserver.get(), m_localserver->m_metrics_backend.get());
			m_emerge->initMapgens(m_mapgen_params.get());
			g_settings->set("num_emerge_threads", num_emerge_threads);
		}

		if (!m_world_path.empty()) {
			m_settings_mgr = std::make_unique<MapSettingsManager>(
					m_world_path + DIR_DELIM + "map_meta");
			m_settings_mgr->mapgen_params = m_mapgen_params.release();
			;
			m_settings_mgr->saveMapMeta();
		} else if (!m_emerge) {
			m_mapgen_params.reset();
		}
	}

	if (packet.count(TOCLIENT_INIT_WEATHER))
		packet[TOCLIENT_INIT_WEATHER].convert(use_weather);

	//if (packet.count(TOCLIENT_INIT_PROTOCOL_VERSION_FM))
	//	packet[TOCLIENT_INIT_PROTOCOL_VERSION_FM].convert( not used );
}

