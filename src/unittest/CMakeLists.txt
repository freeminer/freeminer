file(GLOB unittest_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# fm: Multi-transport dispatch, wakeup and fairness regressions.
list(APPEND unittest_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/test_fm_multi.cpp)

# ENet payload ownership and worker wakeup regressions.
if(USE_ENET)
	list(APPEND unittest_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/test_fm_enet.cpp)
endif()

# SCTP wakeup lifetime and message assembly regressions.
if(USE_SCTP)
	list(APPEND unittest_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/test_fm_sctp.cpp)
endif()

#  Arnis tests include its adapter, which requires Osmium headers.
if(USE_OSMIUM)
	list(APPEND unittest_SRCS
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_bridge_clipping.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_bedrock_models.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_aerialways.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_attractions.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_block_mapping.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_deferred_placement.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_floodfill.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_mapillary_atlas.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_mode_filter.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_natural.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_naturalize.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_peaks.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_planes.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_railways.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_range_fetch.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_relation_holes.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_road_markings.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_roof_layers.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_shoreline.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_swimming_pools.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_terrain_surface.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_train_speed.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_tree_packs.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_water_steps.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/test_fm_arnis_world_utils.cpp
	)
endif()

list(APPEND unittest_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/test_fm_indev.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_fm_blast_shell.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_fm_earth_projection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/fm_test_lock.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_terraindiffusion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_fm_world_merge.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_abm.cpp
# ===

	${unittest_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_address.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_authdatabase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_activeobject.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_areastore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_ban.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_collision.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_compression.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_craft.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_datastructures.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_filesys.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_inventory.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_irrptr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_logging.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_lbmmanager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_lua.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_map.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_mapblock.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_mapdatabase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_mapgen.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_map_settings_manager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_mapnode.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_modchannels.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_modstoragedatabase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_moveaction.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_noderesolver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_noise.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_objdef.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_profiler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_random.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_sao.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_schematic.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_scriptapi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_serialization.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_server_shutdown_state.cpp
	#FMTODO ${CMAKE_CURRENT_SOURCE_DIR}/test_settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_socket.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_servermodmanager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_threading.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_utilities.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_voxelarea.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_voxelalgorithms.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_voxelmanipulator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_gettext.cpp
)

set(unittest_SRCS ${unittest_SRCS} PARENT_SCOPE)

set(unittest_client_SRCS
	# fm: Sparse near-mesh readiness and far-geometry coverage regressions.
	${CMAKE_CURRENT_SOURCE_DIR}/fm_test_near_mesh_handoff.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_fm_far_calc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_fm_content_mapblock.cpp
	# ===

	${CMAKE_CURRENT_SOURCE_DIR}/mesh_compare.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_clientactiveobjectmgr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_content_mapblock.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_eventmanager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_gameui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_keycode.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/test_mesh_compare.cpp
	PARENT_SCOPE)
