include(fm_cmake_compat)

# Keep support for system Boost installations without BoostConfig.cmake.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

# Recompute detected features; never reuse results from an earlier configure.
foreach(feature OSMIUM OSMIUM_TOOL)
    unset(USE_${feature} CACHE)
    set(USE_${feature} 0)
endforeach()

option(ENABLE_OSMIUM "Enable Osmium" ON)
option(ENABLE_OSMIUM_TOOL "Enable Osmium tool" ON)
if(NOT ENABLE_OSMIUM)
    return()
endif()

if(NOT OSMIUM_INCLUDE_DIR AND ENABLE_SYSTEM_OSMIUM)
    find_path(OSMIUM_INCLUDE_DIR NAMES osmium/osm.hpp)
endif()
if(NOT OSMIUM_INCLUDE_DIR)
    if(NOT FETCH_OSMIUM
        AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/libosmium/include/osmium/osm.hpp"
    )
        set(OSMIUM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/libosmium/include")
    elseif(FETCH_DEPS)
        FetchContent_Declare(
            libosmium
            GIT_REPOSITORY https://github.com/osmcode/libosmium
            GIT_TAG v2.23.1
            GIT_SHALLOW TRUE # Header-only dependency; do not configure its tools or tests.
            SOURCE_DIR "${DEPS_SOURCE_ROOT}/libosmium"
            BINARY_DIR "${DEPS_BINARY_ROOT}/libosmium"
            SOURCE_SUBDIR cmake
            ${FM_FETCH_EXCLUDE_FROM_ALL}
        )
        FetchContent_MakeAvailable(libosmium)
        set(OSMIUM_INCLUDE_DIR "${libosmium_SOURCE_DIR}/include")
    endif()
endif()
if(NOT OSMIUM_INCLUDE_DIR)
    message(STATUS "Osmium disabled: headers not found")
    return()
endif()

find_package(LibLZ4 QUIET)
find_package(Protozero QUIET)
find_package(EXPAT QUIET)
if(TARGET LibLZ4::LibLZ4)
    set(LZ4_LIBRARIES LibLZ4::LibLZ4)
endif()
if(TARGET Protozero::Protozero)
    set(PROTOZERO_INCLUDE_DIR "")
endif()

if(FETCH_DEPS AND NOT LZ4_LIBRARIES)
    FetchContent_Declare(
        lz4
        #URL https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
        #URL_HASH SHA256=537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b
        GIT_REPOSITORY https://github.com/lz4/lz4
        GIT_TAG v1.11.1
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR build/cmake
        SOURCE_DIR "${DEPS_SOURCE_ROOT}/lz4"
        BINARY_DIR "${DEPS_BINARY_ROOT}/lz4"
        ${FM_FETCH_SYSTEM}
        ${FM_FETCH_EXCLUDE_FROM_ALL}
    )
    FetchContent_MakeAvailable(lz4)
    set(LZ4_LIBRARIES lz4_static)
endif()

if(FETCH_DEPS AND NOT PROTOZERO_INCLUDE_DIR AND NOT TARGET Protozero::Protozero)
    FetchContent_Declare(
        protozero
        GIT_REPOSITORY https://github.com/mapbox/protozero
        GIT_TAG v1.8.2
        GIT_SHALLOW TRUE
        SOURCE_DIR "${DEPS_SOURCE_ROOT}/protozero"
        BINARY_DIR "${DEPS_BINARY_ROOT}/protozero"
        SOURCE_SUBDIR cmake
        ${FM_FETCH_EXCLUDE_FROM_ALL}
    )
    FetchContent_MakeAvailable(protozero)
    set(PROTOZERO_INCLUDE_DIR "${protozero_SOURCE_DIR}/include")
endif()

if(FETCH_DEPS AND NOT TARGET EXPAT::EXPAT)
    function(fm_fetch_expat)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(EXPAT_BUILD_TOOLS OFF)
        set(EXPAT_BUILD_EXAMPLES OFF)
        set(EXPAT_BUILD_TESTS OFF)
        FetchContent_Declare(
            expat
            GIT_REPOSITORY https://github.com/libexpat/libexpat/
            GIT_TAG R_2_10_0
            GIT_SHALLOW TRUE
            SOURCE_DIR "${DEPS_SOURCE_ROOT}/expat"
            BINARY_DIR "${DEPS_BINARY_ROOT}/expat"
            SOURCE_SUBDIR expat
            ${FM_FETCH_OVERRIDE_FIND_PACKAGE}
            ${FM_FETCH_EXCLUDE_FROM_ALL}
        )
        FetchContent_MakeAvailable(expat)
        if(NOT TARGET EXPAT::EXPAT)
            add_library(EXPAT::EXPAT ALIAS expat)
        endif()
    endfunction()
    if(NOT TARGET EXPAT::EXPAT)
        fm_fetch_expat()
    endif()
endif()

if(NOT TARGET Boost::headers)
    set(Boost_USE_STATIC_LIBS ${BUILD_STATIC_LIBS})
    find_package(Boost QUIET)
endif()
if(NOT PROTOZERO_INCLUDE_DIR)
    find_path(PROTOZERO_INCLUDE_DIR NAMES protozero/pbf_reader.hpp)
endif()
if(NOT TARGET BZip2::BZip2)
    find_package(BZip2 QUIET)
endif()
if(NOT TARGET EXPAT::EXPAT)
    find_package(EXPAT QUIET)
endif()
# Osmium's supported readers need these libraries, regardless of header origin.
if(NOT TARGET Boost::headers OR (NOT PROTOZERO_INCLUDE_DIR AND NOT TARGET Protozero::Protozero) OR NOT TARGET BZip2::BZip2
    OR NOT TARGET EXPAT::EXPAT
)
    message(STATUS "Osmium disabled: requires Boost headers, protozero, BZip2 and EXPAT")
    return()
endif()

include(fm_tinygltf)
if(NOT TARGET fm_tinygltf OR NOT TARGET nlohmann_json::nlohmann_json)
    message(STATUS "Osmium/Arnis disabled: requires TinyGLTF and nlohmann_json")
    return()
endif()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/arnis-cpp/src/data_processing.h)
    message(STATUS "arnis-cpp sources not found, update submodules:  git submodule sync; git submodule update --init --recursive --jobs 4;")
    return()
endif()

add_library(fm_osmium INTERFACE)
target_include_directories(
    fm_osmium SYSTEM INTERFACE ${OSMIUM_INCLUDE_DIR} ${PROTOZERO_INCLUDE_DIR} ${Boost_INCLUDE_DIRS}
)
target_link_libraries(
    fm_osmium
    INTERFACE fm_tinygltf
    nlohmann_json::nlohmann_json
    Boost::headers
    BZip2::BZip2
    EXPAT::EXPAT
    ZLIB::ZLIB
    Threads::Threads
)
if(TARGET Boost::geometry)
    target_link_libraries(fm_osmium INTERFACE Boost::geometry)
endif()

# LASzip is optional: when available, Arnis can decode LAZ survey tiles and EPT nodes.
option(ENABLE_LASZIP "Enable LASzip decoding for Arnis survey data" ON)
if(ENABLE_LASZIP)
    find_path(FM_LASZIP_INCLUDE_DIR NAMES laszip_api.h PATH_SUFFIXES laszip)
    find_library(FM_LASZIP_LIBRARY NAMES laszip_api laszip)
    if(FM_LASZIP_INCLUDE_DIR AND FM_LASZIP_LIBRARY)
        target_include_directories(fm_osmium SYSTEM INTERFACE ${FM_LASZIP_INCLUDE_DIR})
        target_link_libraries(fm_osmium INTERFACE ${FM_LASZIP_LIBRARY})
        target_compile_definitions(fm_osmium INTERFACE ARNIS_WITH_LASZIP=1)
        message(STATUS "Arnis LAZ decoding enabled: ${FM_LASZIP_LIBRARY}")
    else()
        message(STATUS "Arnis LAZ decoding disabled: LASzip headers/library not found")
    endif()
endif()

if(TARGET Protozero::Protozero)
    target_link_libraries(fm_osmium INTERFACE Protozero::Protozero)
endif()
# The native voxelizer provides TinyGLTF's implementation when enabled.
if(NOT USE_VOXEL_EARTH)
    target_link_libraries(fm_osmium INTERFACE tinygltf)
endif()
list(APPEND FREEMINER_COMMON_LIBRARIES fm_osmium)
set(USE_OSMIUM 1)
message(STATUS "Using Osmium: ${OSMIUM_INCLUDE_DIR}")

if(ENABLE_OSMIUM_TOOL)
    if(NOT TARGET Boost::program_options)
        find_package(Boost QUIET COMPONENTS program_options)
    endif()
    if(TARGET Boost::program_options AND TARGET nlohmann_json::nlohmann_json)
        include(fm_osmium_tool)
    else()
        message(STATUS "Osmium tool disabled: requires Boost program_options and nlohmann_json")
    endif()
endif()
