option(ENABLE_OSMIUM_TOOL "Enable Osmium tool" 1)
if(ENABLE_OSMIUM_TOOL AND EXISTS
    ${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/osmium-tool/CMakeLists.txt
)
    set(USE_OSMIUM_TOOL 1)
endif()

if(USE_OSMIUM_TOOL)
    list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/osmium-tool/cmake/Modules/"
    )
    set(OSMIUM_TOOL_LIBRARY osmium-tool-lib)
    set(OSMIUM_TOOL_SRC mapgen/earth/osmium-tool/src/)

    configure_file(
        ${OSMIUM_TOOL_SRC}/version.cpp.in ${PROJECT_BINARY_DIR}/${OSMIUM_TOOL_SRC}/version.cpp
    )

    add_library(
        ${OSMIUM_TOOL_LIBRARY}
        ${PROJECT_BINARY_DIR}/${OSMIUM_TOOL_SRC}/version.cpp
        ${OSMIUM_TOOL_SRC}cmd_factory.cpp
        ${OSMIUM_TOOL_SRC}cmd.cpp
        ${OSMIUM_TOOL_SRC}command_extract.cpp
        ${OSMIUM_TOOL_SRC}command_help.cpp
        ${OSMIUM_TOOL_SRC}export/export_format_json.cpp
        ${OSMIUM_TOOL_SRC}export/export_format_pg.cpp
        ${OSMIUM_TOOL_SRC}export/export_format_text.cpp
        ${OSMIUM_TOOL_SRC}export/export_handler.cpp
        ${OSMIUM_TOOL_SRC}extract/extract_bbox.cpp
        ${OSMIUM_TOOL_SRC}extract/extract_polygon.cpp
        ${OSMIUM_TOOL_SRC}extract/extract.cpp
        ${OSMIUM_TOOL_SRC}extract/geojson_file_parser.cpp
        ${OSMIUM_TOOL_SRC}extract/geometry_util.cpp
        ${OSMIUM_TOOL_SRC}extract/osm_file_parser.cpp
        ${OSMIUM_TOOL_SRC}extract/poly_file_parser.cpp
        ${OSMIUM_TOOL_SRC}extract/strategy_complete_ways_with_history.cpp
        ${OSMIUM_TOOL_SRC}extract/strategy_complete_ways.cpp
        ${OSMIUM_TOOL_SRC}extract/strategy_simple.cpp
        ${OSMIUM_TOOL_SRC}extract/strategy_smart.cpp
        ${OSMIUM_TOOL_SRC}id_file.cpp
        ${OSMIUM_TOOL_SRC}io.cpp
        ${OSMIUM_TOOL_SRC}option_clean.cpp
        ${OSMIUM_TOOL_SRC}util.cpp
    )
    target_link_libraries(
        ${OSMIUM_TOOL_LIBRARY} PRIVATE fm_osmium nlohmann_json::nlohmann_json
        PUBLIC Boost::program_options
    )

    list(APPEND FREEMINER_COMMON_LIBRARIES ${OSMIUM_TOOL_LIBRARY})

endif()
message(STATUS "Using osmiumtool ${USE_OSMIUM_TOOL} : ${OSMIUM_TOOL_LIBRARY}")
