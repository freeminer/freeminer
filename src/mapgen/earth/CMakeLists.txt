
if(USE_OSMIUM)
    file(GLOB_RECURSE mapgen_earth_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/arnis-cpp/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/arnis-cpp/src/*.cpp")
    list(APPEND mapgen_earth_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/arnis_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/blocks.cpp
    )
    message(STATUS "Using osmium + arnis")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/luanti-earth/native/src/voxelizer.h")
    option(USE_VOXEL_EARTH "Earth: Use voxel earth" 1)
endif()

# if(USE_VOXEL_EARTH)
#     set(mapgen_earth_SRCS
#         ${mapgen_earth_SRCS}
#         #${CMAKE_CURRENT_SOURCE_DIR}/voxel_earth.cpp
#     )
# endif()


list(APPEND mapgen_earth_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/hgt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/http.cpp
)

if(USE_VOXEL_EARTH)
    set(TINYGLTF_BUILD_LOADER_EXAMPLE 0 CACHE INTERNAL "")
    set(TINYGLTF_INSTALL 0 CACHE INTERNAL "")
    set(TINYGLTF_INSTALL_VENDOR 0 CACHE INTERNAL "")

    include_directories(tinygltf)

    set(HAVE_NLOHMANN_JSON 1 CACHE INTERNAL "")
    add_subdirectory(luanti-earth/native)
    #set(FREEMINER_CLIENT_LIBRARIES ${FREEMINER_CLIENT_LIBRARIES} tinygltf::tinygltf PARENT_SCOPE)
    #set(FREEMINER_SERVER_LIBRARIES ${FREEMINER_SERVER_LIBRARIES} tinygltf::tinygltf PARENT_SCOPE)
    set(FREEMINER_CLIENT_LIBRARIES ${FREEMINER_CLIENT_LIBRARIES} earth_native PARENT_SCOPE)
    set(FREEMINER_SERVER_LIBRARIES ${FREEMINER_SERVER_LIBRARIES} earth_native PARENT_SCOPE)

    list(APPEND mapgen_earth_SRCS
        #${CMAKE_CURRENT_SOURCE_DIR}/voxel_earth.cpp
    )
    message(STATUS "Earth: Using voxel-earth")
endif()


if(1)
    message(WARNING "mapgen_earth_SRCS=${mapgen_earth_SRCS}")
    set(freeminer_mapgen_SRCS ${freeminer_mapgen_SRCS}
        ${mapgen_earth_SRCS}
        PARENT_SCOPE
    )
endif()

if(0)
    add_library(mapgen_earth
        ${mapgen_earth_SRCS}
    )

    if(USE_VOXEL_EARTH)
        #target_include_directories(mapgen_earth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf)
    endif()


    target_compile_definitions(mapgen_earth PRIVATE "MT_BUILDTARGET=1")

    #  ${CMAKE_SOURCE_DIR}/irr/include
    target_include_directories(mapgen_earth PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
    target_link_libraries(
        mapgen_earth
        #${ZLIB_LIBRARIES}
        IrrlichtMt::IrrlichtMt
        #${ZSTD_LIBRARY}
        #${SOUND_LIBRARIES}
        #${LUA_LIBRARY}
        #${GMP_LIBRARY}
        ${JSON_LIBRARY}
    )

endif()
