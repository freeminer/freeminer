#include <atomic>
#include <exception>
#include <future>
#include <memory>
#include "client.h"
#include "fm_far_calc.h"
#include "client/mapblock_mesh.h"
#include "clientmap.h"
#include "emerge.h"
#include "fm_world_merge.h"
#include "irr_v3d.h"
#include "log.h"
#include "mapblock.h"
#include "network/fm_networkprotocol.h"
#include "server.h"
#include "filesys.h"
#include "map.h"
#include "mapgen/mapgen.h"
#include "network/networkpacket.h"
#include "threading/lock.h"
#include "util/directiontables.h"

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

	static thread_local const auto farmesh_server = g_settings->getU16("farmesh_server");
	if (!farmesh_server)
		return;

	auto &far_blocks = m_env.getClientMap().m_far_blocks_ask;
	const auto lock = far_blocks.lock_unique_rec();

	if (far_blocks.empty()) {
		return;
	}

	MSGPACK_PACKET_INIT((int)TOSERVER_GET_BLOCKS, 1);

	// DUMP("ask far blocks", far_blocks.size());

	PACK(TOSERVER_GET_BLOCKS_BLOCKS,
			static_cast<std::remove_reference_t<decltype(far_blocks)>::full_type>(
					far_blocks));
	far_blocks.clear();
	lock->unlock();

	NetworkPacket pkt(TOSERVER_GET_BLOCKS, buffer.size());
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}

void Client::handleCommand_FreeminerInit(NetworkPacket *pkt)
{
	if (!pkt->packet_unpack())
		return;

	auto &packet = *(pkt->packet);

	if (!m_world_path.empty() && packet.contains(TOCLIENT_INIT_GAMEID)) {
		std::string gameid;
		packet[TOCLIENT_INIT_GAMEID].convert(gameid);
		std::string conf_path = m_world_path + DIR_DELIM + "world.mt";
		Settings conf;
		conf.readConfigFile(conf_path.c_str());
		conf.set("gameid", gameid);
		conf.updateConfigFile(conf_path.c_str());
	}

	{
		Settings settings;
		packet[TOCLIENT_INIT_MAP_PARAMS].convert(settings);
		std::string mg_name;
		MapgenType mgtype = settings.getNoEx("mg_name", mg_name)
									? Mapgen::getMapgenType(mg_name)
									: FARMESH_DEFAULT_MAPGEN;

		if (mgtype == MAPGEN_INVALID) {
			errorstream << "Client map save: mapgen '" << mg_name
						<< "' not valid; falling back to "
						<< Mapgen::getMapgenName(FARMESH_DEFAULT_MAPGEN) << "\n";
			mgtype = FARMESH_DEFAULT_MAPGEN;
			far_container.use_weather = false;
		} else {
			far_container.have_params = true;
		}

		MakeEmerge(settings, mgtype);
	}

	if (packet.contains(TOCLIENT_INIT_WEATHER)) {
		packet[TOCLIENT_INIT_WEATHER].convert(use_weather);
	}

	//if (packet.count(TOCLIENT_INIT_PROTOCOL_VERSION_FM))
	//	packet[TOCLIENT_INIT_PROTOCOL_VERSION_FM].convert( not used );
}

void Client::MakeEmerge(const Settings &settings, const MapgenType &mgtype)
{
	const thread_local static auto farmesh_range = g_settings->getS32("farmesh");

	if (farmesh_range && !m_localserver) {
		// Todo: make very small special game with only far nodes definitions
		for (const auto &game : {"devtest", "default"}) {
			try {
				m_localserver = std::make_unique<Server>(
						"farmesh", findSubgame(game), false, Address{}, true);
				break;
			} catch (const std::exception &ex) {
				errorstream << "Failed to make local mapgen server with game " << game
							<< " : " << ex.what() << "\n";
			}
		}
	}
	m_mapgen_params = std::unique_ptr<MapgenParams>(Mapgen::createMapgenParams(mgtype));
	m_mapgen_params->MapgenParams::readParams(&settings);
	m_mapgen_params->readParams(&settings);

	if (!m_simple_singleplayer_mode && farmesh_range && m_localserver) {
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
		m_settings_mgr->saveMapMeta();
	} else if (!m_emerge) {
		m_mapgen_params.reset();
	}
}

void Client::createFarMesh(MapBlockPtr &block)
{
	if (bool cmp = false; block->creating_far_mesh.compare_exchange_weak(cmp, true)) {
		const auto &m_client = this;
		const auto &blockpos_actual = block->getPos();
		const auto &m_camera_offset = m_camera->getOffset();
		const auto &step = block->far_step;
#if FARMESH_SHADOWS
		static const auto m_cache_enable_shaders = g_settings->getBool("enable_shaders");
#else
		static const auto m_cache_enable_shaders = false;
#endif
		MeshMakeData mdat(m_client->getNodeDefManager(),
				MAP_BLOCKSIZE * m_client->getMeshGrid().cell_size, m_cache_enable_shaders,
				0, step, &m_client->far_container);
		mdat.m_blockpos = blockpos_actual;
		const auto mbmsh =
				std::make_shared<MapBlockMesh>(m_client, &mdat, m_camera_offset);
		block->setFarMesh(mbmsh, step);
		block->creating_far_mesh = false;
	}
}

void Client::handleCommand_BlockDataFm(NetworkPacket *pkt)
{
	const auto str = std::string{pkt->getString(0), pkt->getSize()};
	if (!pkt->packet_unpack()) {
		return;
	}
	auto &packet = *(pkt->packet);
	v3bpos_t bpos = packet[TOCLIENT_BLOCKDATA_POS].as<v3bpos_t>();
	block_step_t step = 0;
	packet[TOCLIENT_BLOCKDATA_STEP].convert(step);
	std::istringstream istr(
			packet[TOCLIENT_BLOCKDATA_DATA].as<std::string>(), std::ios_base::binary);

	MapBlockPtr block{};
	if (step) {
		block = m_env.getMap().createBlankBlockNoInsert(bpos);
	} else {
		block = m_env.getMap().getBlock(bpos);
		if (!block)
			block = m_env.getMap().createBlankBlock(bpos);
	}
	const auto lock = block->lock_unique_rec();
	block->far_step = step;
	content_t content_only{};
	packet.convert_safe(TOCLIENT_BLOCKDATA_CONTENT_ONLY, content_only);
	block->content_only = content_only;
	packet.convert_safe(
			TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM1, block->content_only_param1);
	packet.convert_safe(
			TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM2, block->content_only_param2);

	if (block->content_only == CONTENT_IGNORE) {
		try {
			block->deSerialize(istr, m_server_ser_ver, false);
		} catch (const std::exception &ex) {
			errorstream << "fm block deSerialize fail " << bpos << " " << block->far_step
						<< " : " << ex.what() << " : " << pkt->getSize() << " "
						<< packet.size() << " v=" << (short)m_server_ser_ver << "\n";
#if !NDEBUG
			errorstream << "bad data " << istr.str().size() << " : " << istr.str()
						<< "\n";
#endif
			return;
		}
	} else {
		block->fill({block->content_only, block->content_only_param1,
				block->content_only_param2});
	}
	s32 h = 0; // for convert to atomic
	packet[TOCLIENT_BLOCKDATA_HEAT].convert(h);
	block->heat = h;
	h = 0;
	packet[TOCLIENT_BLOCKDATA_HUMIDITY].convert(h);
	block->humidity = h;

	if (m_localdb && !is_simple_singleplayer_game) {
		if (const auto db = GetFarDatabase({}, far_dbases, m_world_path, step); db) {
			ServerMap::saveBlock(block.get(), db);

			if (!step && !far_container.have_params) {
				merger->add_changed(bpos);
			}
		}
	}

	if (!step) {
		updateMeshTimestampWithEdge(bpos);
		if (!overload && block->content_only != CONTENT_IGNORE &&
				block->content_only != CONTENT_AIR) {
			if (getNodeBlockPos(floatToInt(m_env.getLocalPlayer()->getPosition(), BS))
							.getDistanceFrom(bpos) <= 1)
				addUpdateMeshTaskWithEdge(bpos);
		}
	} else {
		static thread_local const auto farmesh_server =
				g_settings->getU16("farmesh_server");
		static thread_local const auto farmesh = g_settings->getU16("farmesh");
		if (!farmesh_server || !farmesh) {
			return;
		}

		auto &far_blocks_storage = getEnv().getClientMap().far_blocks_storage[step];
		{
			const auto lock = far_blocks_storage.lock_unique_rec();
			if (far_blocks_storage.find(bpos) != far_blocks_storage.end()) {
				return;
			}
			far_blocks_storage.insert_or_assign(block->getPos(), block);
		}
		++m_new_farmeshes;

		//todo: step ordered thread pool
		mesh_thread_pool.enqueue([this, block]() mutable {
			createFarMesh(block);
			auto &client_map = getEnv().getClientMap();
			const auto &control = client_map.getControl();
			const auto bpos = block->getPos();
			int fmesh_step_ = getFarStep(control,
					getNodeBlockPos(client_map.far_blocks_last_cam_pos), block->getPos());
			if (!inFarGrid(block->getPos(),
						getNodeBlockPos(client_map.far_blocks_last_cam_pos), fmesh_step_,
						control)) {
				return;
			}
			auto &far_blocks = client_map.m_far_blocks;
			if (const auto &it = far_blocks.find(bpos); it != far_blocks.end()) {
				if (it->second->far_step != block->far_step) {
					return;
				}
				block->far_iteration =
						it->second->far_iteration.load(std::memory_order::relaxed);
				far_blocks.at(bpos) = block;
			}
		});

// if decide to generate empty areas on server:
#if 0
				class BlockContainer : public NodeContainer
				{
					MapBlockPtr block;

				public:
					Mapgen *m_mg{};
					BlockContainer(Client *client, MapBlockPtr block_) :
							//m_client{client},
							block{std::move(block_)} {};
					const MapNode &getNodeRefUnsafe(const v3pos_t &pos) override
					{
						auto bpos = getNodeBlockPos(pos);
						const auto fmesh_step = block->far_step;
						const auto &shift = fmesh_step; // + cell_size_pow;
						v3bpos_t bpos_aligned((bpos.X >> shift) << shift,
								(bpos.Y >> shift) << shift, (bpos.Z >> shift) << shift);

						v3pos_t relpos = pos - bpos_aligned * MAP_BLOCKSIZE;

						const auto &relpos_shift = fmesh_step; // + 1;
						auto relpos_shifted = v3pos_t(relpos.X >> relpos_shift,
								relpos.Y >> relpos_shift, relpos.Z >> relpos_shift);

						const auto &n = block->getNodeNoLock(relpos_shifted);
						return n;
					};
				};

				BlockContainer bc{this, block};

				const auto &m_client = this;
				const auto &blockpos_actual = bpos;
				MeshMakeData mdat(m_client, false, 0, step, &bc);
				mdat.m_blockpos = blockpos_actual;
				const auto m_camera_offset = getCamera()->getOffset();
				auto mbmsh = std::make_shared<MapBlockMesh>(&mdat, m_camera_offset);
				block->setFarMesh(mbmsh, step, m_client->m_uptime);
				block->setTimestampNoChangedFlag(
						getEnv().getClientMap().m_far_blocks_use_timestamp);
				getEnv().getClientMap().far_blocks_storage[step].insert_or_assign(
						bpos, block);
				++m_client->m_new_meshes;
			}
#endif
	}
}

void Client::sendDrawControl()
{
	MSGPACK_PACKET_INIT((int)TOSERVER_DRAWCONTROL, 4);
	const auto &draw_control = m_env.getClientMap().getControl();
	PACK(TOSERVER_DRAWCONTROL_WANTED_RANGE, (int32_t)draw_control.wanted_range);
	//PACK(TOSERVER_DRAWCONTROL_RANGE_ALL, draw_control.range_all);
	PACK(TOSERVER_DRAWCONTROL_FARMESH, draw_control.farmesh);
	//PACK(TOSERVER_DRAWCONTROL_LODMESH, draw_control.lodmesh);
	//PACK(TOSERVER_DRAWCONTROL_FOV, draw_control.fov);
	//PACK(TOSERVER_DRAWCONTROL_BLOCK_OVERFLOW, false /*draw_control.block_overflow*/);
	//PACK(TOSERVER_DRAWCONTROL_LODMESH, draw_control.lodmesh);
	PACK(TOSERVER_DRAWCONTROL_FARMESH_QUALITY, draw_control.farmesh_quality);
	PACK(TOSERVER_DRAWCONTROL_FARMESH_ALL_CHANGED, draw_control.farmesh_all_changed);

	NetworkPacket pkt(TOSERVER_DRAWCONTROL, buffer.size());
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}
