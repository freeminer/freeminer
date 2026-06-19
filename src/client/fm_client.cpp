#include "client/fm_farmesh.h"
#include <atomic>
#include <exception>
#include <memory>
#include "client.h"
#include "client/fm_farmesh.h"
#include "client/localplayer.h"
#include "client/mapblock_mesh.h"
#include "clientmap.h"
#include "emerge.h"
#include "filesys.h"
#include "fm_far_calc.h"
#include "fm_weather.h"
#include "fm_world_merge.h"
#include "irr_v3d.h"
#include "log.h"
#include "map.h"
#include "mapblock.h"
#include "mapgen/mapgen.h"
#include "network/fm_networkprotocol.h"
#include "network/networkpacket.h"
#include "profiler.h"
#include "server.h"
#include "threading/lock.h"
#include "util/directiontables.h"
#include <atomic>
#include <exception>
#include <memory>

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

	PACK(TOSERVER_INIT_FM_VERSION, CLIENT_PROTOCOL_VERSION_FM);

	NetworkPacket pkt(TOSERVER_INIT_FM, buffer.size());
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}

void Client::sendGetBlocks()
{
	static thread_local const auto farmesh_server = g_settings->getU16("farmesh_server");
	if (!farmesh_server)
		return;

	ServerMap::far_blocks_ask_t far_blocks;
	{
		const auto lock = m_env.getClientMap().m_far_blocks_ask.lock_unique_rec();
		std::swap(far_blocks, m_env.getClientMap().m_far_blocks_ask);
	}

	if (far_blocks.empty()) {
		return;
	}

	MSGPACK_PACKET_INIT((int)TOSERVER_GET_BLOCKS, 1);

	// DUMP("ask far blocks", far_blocks.size());

	PACK(TOSERVER_GET_BLOCKS_BLOCKS,
			static_cast<std::remove_reference_t<decltype(far_blocks)>::full_type>(
					far_blocks));

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
				// auto m_env = new ServerEnvironment({}, //std::move(startup_server_map),
				// 		m_localserver.get(), &m_metrics_backend		   //	m_metrics_backend.get()
				// );
				// m_localserver->m_env = m_env;

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
		// m_emerge->env = &m_localserver->getEnv();
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
	//if (bool cmp = false; block->creating_far_mesh.compare_exchange_weak(cmp, true))
	block->creating_far_mesh = true;
	if (1) {
		g_profiler->add("Client: Farmesh mesh", 1);
		TimeTaker timer("Client: Farmesh mesh [ms]");
		block->far_make_mesh_timestamp = -1;
		block->far_status = MapBlock::far_status_e::s5_mesh_start;

		const auto &m_client = this;
		const auto &blockpos = block->getPos();
		//const auto &m_camera_offset = m_camera->getOffset();
		const auto &step = block->far_step;
		MeshMakeData mesh_make_data(m_client->getNodeDefManager(),
				MAP_BLOCKSIZE * m_mesh_grid.cell_size, m_mesh_grid, 0, step,
				&m_client->far_container);
		mesh_make_data.m_blockpos = blockpos;
		const auto mesh = std::make_shared<MapBlockMesh>(m_client, &mesh_make_data);
		block->setFarMesh(mesh, step);
		block->far_step_draw = block->far_step;
		block->creating_far_mesh = false;
		block->far_status = MapBlock::far_status_e::s6_mesh_complete;
		if (m_client->farmesh) {
			m_client->farmesh->publishFarBlock(block);
		}
		++m_client->m_new_meshes;
		g_profiler->avg("Client: Farmesh mesh [ms]", timer.stop(true));
	}
}

void Client::handleCommand_BlockDataFm(NetworkPacket *pkt)
{
	const auto str = std::string{pkt->getString(0), pkt->getSize()};
	if (!pkt->packet_unpack()) {
		return;
	}
	auto &packet = *(pkt->packet);
	processSingleBlockData(packet);
}

void Client::handleCommand_BlockDatasFm(NetworkPacket *pkt)
{
	if (!pkt->packet_unpack()) {
		return;
	}

	auto &packet = pkt->packet;

	// Check if this is an array of blocks packet
	size_t blocks_count = 0;
	if (packet->contains(TOCLIENT_BLOCKDATA_BLOCKS)) {
		(*packet)[TOCLIENT_BLOCKDATA_BLOCKS].convert(blocks_count);
	}
	if (blocks_count > 0 && packet->contains(TOCLIENT_BLOCKDATA_BLOCKS_DATA)) {
		// Handle array of blocks
		std::string blocks_data;
		(*packet)[TOCLIENT_BLOCKDATA_BLOCKS_DATA].convert(blocks_data);

		// Unpack the array of blocks
		msgpack::unpacked unpacked;
		msgpack::unpack(unpacked, blocks_data.data(), blocks_data.size());
		msgpack::object obj = unpacked.get();

		if (obj.type == msgpack::type::ARRAY) {
			auto blocks_array = obj.as<std::vector<msgpack::object>>();

			for (const auto &block_obj : blocks_array) {
				if (block_obj.type == msgpack::type::MAP) {
					MsgpackPacket block_packet = block_obj.as<MsgpackPacket>();
					MsgpackPacketSafe block_packet_safe;
					block_packet_safe.insert(block_packet.begin(), block_packet.end());

					// Process each block in the array
					processSingleBlockData(block_packet_safe);
				}
			}
		}
	} else {
		// Handle single block (legacy compatibility)
		processSingleBlockData(*packet);
	}
}

void Client::processSingleBlockData(MsgpackPacketSafe &packet)
{
	v3bpos_t bpos = packet[TOCLIENT_BLOCKDATA_POS].as<v3bpos_t>();
	block_step_t step = 0;
	packet.convert_safe(TOCLIENT_BLOCKDATA_STEP, step);
	std::istringstream istr(
			packet[TOCLIENT_BLOCKDATA_DATA].as<std::string>(), std::ios_base::binary);

	MapBlockPtr block{};
	if (step) {
		auto &far_blocks_storage = getEnv().getClientMap().far_blocks_storage[step];
		const auto lock = far_blocks_storage.lock_unique_rec();
		if (const auto it = far_blocks_storage.find(bpos);
				it != far_blocks_storage.end() && it->second.block) {
			block = it->second.block;
		}
		if (!block) {
			block = m_env.getMap().createBlankBlockNoInsert(bpos);
			far_blocks_storage.insert_or_assign(
					bpos, Map::BlockUsed{block, (int32_t)m_uptime});
		}
	} else {
		block = m_env.getMap().getBlock(bpos);
		if (!block) {
			block = m_env.getMap().createBlankBlock(bpos);
		}
	}
	{
		const auto lock = block->lock_unique_rec();
		block->far_step = step;

		content_t content_only{CONTENT_IGNORE};
		packet.convert_safe(TOCLIENT_BLOCKDATA_CONTENT_ONLY, content_only);
		/*
		if (content_only != CONTENT_IGNORE) {
			block->data[0].param0 = content_only;
			packet.convert_safe(
					TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM1, block->data[0].param1);
			packet.convert_safe(
					TOCLIENT_BLOCKDATA_CONTENT_ONLY_PARAM2, block->data[0].param2);
		}
*/
		if (content_only == CONTENT_IGNORE) {
			//block->m_is_mono_block = false;
			try {
				block->deSerialize(istr, m_server_ser_ver, false);
			} catch (const std::exception &ex) {
				errorstream << "fm block deSerialize fail " << bpos << " "
							<< block->far_step << " : " << ex.what() << " : "
							<< packet.size() << " v=" << (short)m_server_ser_ver << "\n";
#if !NDEBUG
				errorstream << "bad data " << istr.str().size() << " : " << istr.str()
							<< "\n";
#endif
				return;
			}
		} else {
			//block->m_is_mono_block = true;
			//block->fill(block->data[0]);
			block->fill(content_only);
		}
		weather::heat_t heat = 0; // for convert to atomic
		packet[TOCLIENT_BLOCKDATA_HEAT].convert(heat);
		block->heat = heat;
		weather::humidity_t humidity = 0;
		packet[TOCLIENT_BLOCKDATA_HUMIDITY].convert(humidity);
		block->humidity = humidity;
		if (packet.contains(TOCLIENT_BLOCKDATA_WIND)) {
			weather::wind_t wind;
			packet[TOCLIENT_BLOCKDATA_WIND].convert(wind);
			block->wind = wind;
		}
	}

	mesh_thread_pool.enqueue([this, block, bpos, step]() {
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
			if (!overload && block->m_is_mono_block &&
					block->data[0].param0 != CONTENT_AIR) {
				if (getNodeBlockPos(floatToInt(m_env.getLocalPlayer()->getPosition(), BS))
								.getDistanceFrom(bpos) <= 1)
					addUpdateMeshTaskWithEdge(bpos);
			}
		} else {
			static thread_local const auto settings_farmesh_server =
					g_settings->getU16("farmesh_server");
			static thread_local const auto settings_farmesh =
					g_settings->getU16("farmesh");
			if (!settings_farmesh_server || !settings_farmesh || !farmesh) {
				return;
			}

			const auto block_status = [this](const MapBlockPtr &block, const auto &step) {
				constexpr auto update_farmesh = true;
				const auto far_make_mesh_timestamp = m_uptime + 1 + step / 4;
				if (block->far_make_mesh_timestamp <= 0 ||
						block->far_make_mesh_timestamp == -1 ||
						(update_farmesh && block->far_make_mesh_timestamp <
												   far_make_mesh_timestamp)) {
					block->far_make_mesh_timestamp = far_make_mesh_timestamp;
				}
				block->far_status = MapBlock::far_status_e::s3_recieved;
			};

			block_status(block, step);

			++m_new_farmeshes;

			{
				auto &client_map = getEnv().getClientMap();
				const auto &control = client_map.getControl();
				auto blockpos = block->getPos();
				//const auto blockpos_original = blockpos;
				//const auto step_original = step;

				const auto tree_result = farmesh::getFarParams(
						control, getNodeBlockPos(client_map.far_cam_pos_mesh), blockpos);
				if (!tree_result)
					return;
				auto &far_blocks = client_map.m_far_blocks;
				bool other_draw_block = false;
				if (tree_result->pos != blockpos || tree_result->step != step) {
					other_draw_block = true;
					auto &step = tree_result->step;
					blockpos = tree_result->pos;
					const auto lock = far_blocks.lock_unique_rec();
					if (const auto &it = far_blocks.find(blockpos);
							it != far_blocks.end() && it->second->far_step == step) {
						auto &block = it->second;
						block_status(block, step);
						/*
						if (block->far_make_mesh_timestamp <= 0 ||
								(update_farmesh && block->far_make_mesh_timestamp <
														   far_make_mesh_timestamp)) {
							block->far_make_mesh_timestamp = far_make_mesh_timestamp;
						}
						block->far_status = MapBlock::far_status_e::s3_recieved;
*/
					} else {
						return;
					}
				}

				// if (other_draw_block) {
				// 	return;
				// }

				// farmesh->enqueueFarMeshForBlock(blockpos, step, block, m_uptime, other_draw_block);

#if 0
				if (0) {
					const auto lock = far_blocks.lock_unique_rec();
					if (const auto &it = far_blocks.find(blockpos);
							it != far_blocks.end()) {
						if (it->second->far_step != block->far_step) {
							return;
						}
						block->far_iteration = it->second->far_iteration.load(
								std::memory_order::relaxed);

						far_blocks.at(blockpos) = block;
					}
				}
#endif
			} //();
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
	PACK(TOSERVER_DRAWCONTROL_FARMESH_QUALITY,
			std::max<block_step_t>(draw_control.farmesh_quality, draw_control.cell_size));
	PACK(TOSERVER_DRAWCONTROL_FARMESH_ALL_CHANGED, draw_control.farmesh_all_changed);

	NetworkPacket pkt(TOSERVER_DRAWCONTROL, buffer.size());
	pkt.putLongString({buffer.data(), buffer.size()});
	Send(&pkt);
}

void ClientMap::cleanPerodic(uint32_t uptime)
{
#if FARMESH_CLEAN
	for (const auto &[pos, block] : m_blocks) {
		thread_local static const auto client_unload_unused_data_timeout =
				g_settings->getFloat("client_unload_unused_data_timeout") * 2;

		{
			int step = 0;
			for (auto &ma : block->m_lod_mesh) {
				if (auto m = ma.load()) {
					if (m->last_used + client_unload_unused_data_timeout < uptime) {
						m.reset();
					}
				}
				++step;
			}
		}

		{
			int step = 0;
			for (auto &ma : block->m_far_mesh) {
				if (auto m = ma.load()) {
					if (m->last_used + client_unload_unused_data_timeout < uptime) {
						m.reset();
					}
				}
				++step;
			}
		}
	}
#endif
}

void Client::registerClientSettingsCallbacks()
{
	/*
Via FarMesh
	g_settings->registerChangedCallback(
			"client_mesh_chunk",
			[](const std::string &name, void *data) {
				static_cast<Client *>(data)->onSettingChanged(name);
			},
			this);
*/
}

void Client::onSettingChanged(const std::string &name)
{
	if (name == "client_mesh_chunk") {
		m_mesh_grid = {g_settings->getU16("client_mesh_chunk")};
		// Update the control values that depend on mesh grid
		auto &control = m_env.getClientMap().getControl();
		control.cell_size = m_mesh_grid.cell_size;
		control.cell_size_pow = farmesh::rangeToStep(control.cell_size);
	}
}
