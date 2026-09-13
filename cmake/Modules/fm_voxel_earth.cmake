# Configure voxel Earth dependencies before its native subdirectory is added.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    option(ENABLE_VOXEL_EARTH "Voxel earth mapgen" ON)
endif()
option(FETCH_EARTH_DEPS "Download missing Earth mapgen dependencies" ${FETCH_DEPS})
if(NOT ENABLE_VOXEL_EARTH
    OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/luanti-earth/native/src/voxelizer.h"
)
    set(USE_VOXEL_EARTH 0)
    return()
endif()
option(USE_VOXEL_EARTH "Earth: Use voxel earth" ON)
if(NOT USE_VOXEL_EARTH)
    return()
endif()

# Reuse checkouts without allowing an implicit download on an offline build.
if(FETCHCONTENT_SOURCE_DIR_DRACO)
    set(FM_DRACO_SOURCE "${FETCHCONTENT_SOURCE_DIR_DRACO}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/draco/CMakeLists.txt")
    set(FM_DRACO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/external/draco")
elseif(EXISTS "${FETCHCONTENT_BASE_DIR}/draco-src/CMakeLists.txt")
    set(FM_DRACO_SOURCE "${FETCHCONTENT_BASE_DIR}/draco-src")
elseif(NOT FETCH_EARTH_DEPS)
    message(STATUS "Voxel Earth disabled: Draco missing; set FETCH_EARTH_DEPS=ON to download")
    set(USE_VOXEL_EARTH 0)
    return()
endif()
if(FM_DRACO_SOURCE)
    set(FETCHCONTENT_SOURCE_DIR_DRACO "${FM_DRACO_SOURCE}")
endif()
# This declaration also controls the native project's FetchContent call.
FetchContent_Declare(
    draco GIT_REPOSITORY https://github.com/google/draco.git GIT_TAG 1.5.7 GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
include(fm_tinygltf)
if(NOT TARGET fm_tinygltf)
    set(USE_VOXEL_EARTH 0)
    return()
endif()
list(APPEND FREEMINER_COMMON_LIBRARIES fm_tinygltf)

if(TARGET nlohmann_json::nlohmann_json)
    set(HAVE_NLOHMANN_JSON 1)
else()
    set(HAVE_NLOHMANN_JSON 0)
    if(NOT FETCH_EARTH_DEPS)
        message(STATUS "Voxel Earth disabled: nlohmann_json missing")
        set(USE_VOXEL_EARTH 0)
    endif()
endif()
