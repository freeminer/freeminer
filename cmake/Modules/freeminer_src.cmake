find_package(MsgPack REQUIRED)
include_directories(${MSGPACK_INCLUDE_DIR})

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    option(ENABLE_SCTP "Enable SCTP networking (EXPERIMENTAL)" 0)
    option(USE_MULTI "Enable MT+ENET+WSS networking" 1)
endif()

if(USE_MULTI)
    #set(ENABLE_SCTP 1 CACHE BOOL "") # Maybe bugs
    set(ENABLE_ENET 1 CACHE BOOL "")
    #set(ENABLE_WEBSOCKET_SCTP 1 CACHE BOOL "") # NOT FINISHED
    if(NOT ANDROID)
        set(ENABLE_WEBSOCKET 0 CACHE BOOL "")
    endif()
endif()

if(ANDROID OR WIN32 OR EMSCRIPTEN OR USE_LIBCXX)
    option(FETCH_DEPS "Compile deps (boost,...) in place" 1)
else()
    option(FETCH_DEPS "Compile deps (boost,...) in place" 0)
endif()

set(FETCHCONTENT_UPDATES_DISCONNECTED 1)
set(FETCHCONTENT_QUIET 0) # Needed to print downloading progress

if(FETCH_DEPS)
    include(FetchContent)
    set(ENABLE_LIB_ONLY ON CACHE BOOL "")
    set(ENABLE_TESTS OFF CACHE BOOL "")
    set(ENABLE_STATIC_LIB ON CACHE BOOL "")
    FetchContent_Declare(
        BZip2
        GIT_REPOSITORY "https://gitlab.com/bzip2/bzip2.git"
        GIT_TAG "master"
        # GIT_TAG "bzip2-1.0.8" # CMake support not available
        GIT_SHALLOW TRUE

        # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/bzip2
        OVERRIDE_FIND_PACKAGE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
        GIT_PROGRESS TRUE
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(BZip2)
    set(BZIP2_FOUND 1 CACHE BOOL "")
    add_library(BZip2::BZip2 ALIAS bz2_static)
    set(BZIP2_INCLUDE_DIR "${bzip2_SOURCE_DIR}" CACHE INTERNAL "")
    target_include_directories(bz2_static PUBLIC ${BZIP2_INCLUDE_DIR})
endif()

if(FETCH_OPENSSL AND FETCH_DEPS AND NOT TARGET OpenSSL::SSL)
    FetchContent_Declare(
        openssl-cmake
        # URL https://github.com/jimmy-park/openssl-cmake/archive/main.tar.gz
        GIT_REPOSITORY https://github.com/jimmy-park/openssl-cmake
        GIT_TAG main
        SOURCE_SUBDIR cmake
        GIT_SUBMODULES_RECURSE OFF
        # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/openssl-cmake
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
        GIT_PROGRESS TRUE
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(openssl-cmake)
endif()

if(FETCH_DEPS)
    set(BOOST_ENABLE_CMAKE ON)
    set(BOOST_INCLUDE_LIBRARIES
        program_options
        asio
        thread
        geometry
    )

    include(FetchContent)
    FetchContent_Declare(
        Boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.90.0
        GIT_SHALLOW TRUE

        # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/boost
        OVERRIDE_FIND_PACKAGE TRUE # needed to find correct Boost
        USES_TERMINAL_DOWNLOAD TRUE
        GIT_PROGRESS TRUE
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(Boost)
    set(Boost_FOUND 1 CACHE INTERNAL "")
    set(Boost_INCLUDE_DIRS "${BOOST_LIBRARY_INCLUDES} ${boost_SOURCE_DIR}/libs/numeric/conversion/include" CACHE INTERNAL "")
endif()

if(ENABLE_WEBSOCKET OR ENABLE_WEBSOCKET_SCTP)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp/CMakeLists.txt)
        find_package(Boost)
        if(Boost_FOUND)
            if(boost_SOURCE_DIR)
                include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
            endif()

            include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp)
            #add_subdirectory(external/websocketpp)
            #set(WEBSOCKETPP_LIBRARY websocketpp::websocketpp)
            find_package(OpenSSL)

            if(OPENSSL_FOUND)
                set(WEBSOCKETPP_LIBRARY ${WEBSOCKETPP_LIBRARY} OpenSSL::SSL)
            endif()

            set(WEBSOCKETPP_LIBRARY ${WEBSOCKETPP_LIBRARY} Boost::headers)
            if(ENABLE_WEBSOCKET)
                set(USE_WEBSOCKET 1 CACHE BOOL "")
            endif()

            if(ENABLE_WEBSOCKET_SCTP)
                set(USE_WEBSOCKET_SCTP 1 CACHE BOOL "")
            endif()
            message(STATUS "Using websocket ${USE_WEBSOCKET},${USE_WEBSOCKET_SCTP}: ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp : ${WEBSOCKETPP_LIBRARY}")
            set(FREEMINER_COMMON_LIBRARIES ${FREEMINER_COMMON_LIBRARIES} ${WEBSOCKETPP_LIBRARY})
        endif()
    else()
        #set(USE_WEBSOCKET 0)
        #set(USE_WEBSOCKET_SCTP 0)
    endif()
endif()

set(USE_CLIENT_MCP "${USE_WEBSOCKET}")

if((ENABLE_SCTP OR ENABLE_WEBSOCKET_SCTP) AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp/usrsctplib)
    message(WARNING "Please Clone usrsctp:  git clone --depth 1 https://github.com/sctplab/usrsctp ${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp")
    set(ENABLE_SCTP 0)
endif()

if(ENABLE_SCTP OR ENABLE_WEBSOCKET_SCTP)
    # from external/usrsctp/usrsctplib/CMakeLists.txt :
    if(SCTP_DEBUG)
        set(sctp_debug 1 CACHE INTERNAL "")
        add_definitions(-DSCTP_DEBUG=1)
    endif()
    set(sctp_build_programs 0 CACHE INTERNAL "")
    set(sctp_werror 0 CACHE INTERNAL "")
    set(WERROR 0 CACHE INTERNAL "") #old

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp)

    #include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp/usrsctplib)
    set(SCTP_LIBRARY usrsctp)

    set(USE_SCTP 1)

    message(STATUS "Using sctp: ${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp ${SCTP_LIBRARY} SCTP_DEBUG=${SCTP_DEBUG}")
    set(FREEMINER_COMMON_LIBRARIES ${FREEMINER_COMMON_LIBRARIES} ${SCTP_LIBRARY})
endif()

if(ENABLE_ENET)
    if(NOT ENABLE_SYSTEM_ENET AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/enet/include/enet/enet.h)
        add_subdirectory(external/enet)
        set(ENET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/enet/include)
        set(ENET_LIBRARY enet)
    endif()
    if(NOT ENET_LIBRARY)
        find_library(ENET_LIBRARY NAMES enet)
        find_path(ENET_INCLUDE_DIR enet/enet.h)
    endif()
    if(ENET_LIBRARY AND ENET_INCLUDE_DIR)
        include_directories(${ENET_INCLUDE_DIR})
        set(USE_ENET 1)
        list(APPEND FREEMINER_COMMON_LIBRARIES ${ENET_LIBRARY})
        message(STATUS "Using enet ${USE_ENET}: ${ENET_INCLUDE_DIR} ${ENET_LIBRARY}")
    endif()
endif()

#set(TinyTIFF_BUILD_TESTS 0 CACHE INTERNAL "")
#add_subdirectory(external/TinyTIFF/src)
#set(TINYTIFF_LIRARY TinyTIFF)

option(ENABLE_TIFF "Enable tiff (geotiff for mapgen earth)" 1)
if(ENABLE_TIFF AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/CMakeLists.txt)
    set(tiff-tools 0 CACHE INTERNAL "")
    set(tiff-tests 0 CACHE INTERNAL "")
    set(tiff-docs 0 CACHE INTERNAL "")
    set(jbig OFF CACHE BOOL "Disable optional JBIG support in bundled libtiff" FORCE)
    set(lerc OFF CACHE BOOL "Disable optional LERC support in bundled libtiff" FORCE)
    set(webp OFF CACHE BOOL "Disable optional WebP support in bundled libtiff" FORCE)
    add_subdirectory(external/libtiff)
    set(TIFF_LIRARY TIFF::tiff)
    set(TIFF_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/libtiff/libtiff ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/libtiff)
    include_directories(BEFORE SYSTEM ${TIFF_INCLUDE_DIR})
    message(STATUS "Using tiff: ${TIFF_INCLUDE_DIR} ${TIFF_LIRARY}")
    set(USE_TIFF 1)
    set(FREEMINER_COMMON_LIBRARIES ${FREEMINER_COMMON_LIBRARIES} ${TIFF_LIRARY})
endif()

option(ENABLE_ONNXRUNTIME "Enable ONNX Runtime for Terrain Diffusion mapgen" 1)
if(ENABLE_ONNXRUNTIME)
    set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/onnxruntime" CACHE PATH "ONNX Runtime source, build, or install root")
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            "${ONNXRUNTIME_ROOT}/include"
            "${ONNXRUNTIME_ROOT}/include/onnxruntime/core/session"
            "${ONNXRUNTIME_ROOT}"
        PATH_SUFFIXES onnxruntime/core/session)
    find_library(ONNXRUNTIME_LIBRARY
        NAMES onnxruntime
        PATHS
            "${ONNXRUNTIME_ROOT}/lib"
            "${ONNXRUNTIME_ROOT}/lib64"
            "${ONNXRUNTIME_ROOT}/build"
            "${ONNXRUNTIME_ROOT}/build/Linux/Debug"
            "${ONNXRUNTIME_ROOT}/build/Linux/Release"
            "${ONNXRUNTIME_ROOT}/build/Linux/RelWithDebInfo"
            "${ONNXRUNTIME_ROOT}/build/Linux/MinSizeRel")
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        include_directories(BEFORE SYSTEM ${ONNXRUNTIME_INCLUDE_DIR})
        set(USE_ONNXRUNTIME 1)
        list(APPEND FREEMINER_COMMON_LIBRARIES ${ONNXRUNTIME_LIBRARY})
        message(STATUS "Using ONNX Runtime: ${ONNXRUNTIME_INCLUDE_DIR} ${ONNXRUNTIME_LIBRARY}")
    else()
        message(STATUS "ONNX Runtime not found; Terrain Diffusion mapgen ONNX path disabled")
        if(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime/core/session/onnxruntime_cxx_api.h")
            message(STATUS "ONNX Runtime headers found in ${ONNXRUNTIME_ROOT}, but libonnxruntime was not found. Build ONNX Runtime first or set ONNXRUNTIME_LIBRARY.")
        endif()
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/json/include/nlohmann/json.hpp")
    add_subdirectory(mapgen/earth/json)
    set(NLOHMANN_INCLUDE_DIR mapgen/earth/json/include)
    include_directories(BEFORE SYSTEM ${NLOHMANN_INCLUDE_DIR})
    message(STATUS "Using nlohmann json : ${NLOHMANN_INCLUDE_DIR}")
endif()

option(ENABLE_OSMIUM "Enable Osmium" 1)

# if(ENABLE_OSMIUM)
#     find_path(OSMIUM_INCLUDE_DIR osmium/osm.hpp)
# endif()

if(1)
    include(FetchContent)
    FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG v2.9.7
        GIT_SUBMODULES_RECURSE OFF
        # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/tinygltf
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE TRUE
        USES_TERMINAL_DOWNLOAD TRUE
        GIT_PROGRESS TRUE
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(tinygltf)

    set(TINYGLTF_INCLUDE_DIR ${tinygltf_SOURCE_DIR})
    include_directories(${TINYGLTF_INCLUDE_DIR})

endif()


if(ENABLE_OSMIUM AND (OSMIUM_INCLUDE_DIR OR EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/libosmium/CMakeLists.txt))

    if(FETCH_DEPS)
        include(FetchContent)

        FetchContent_Declare(lz4
            URL https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
            URL_HASH SHA256=537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b
            SOURCE_SUBDIR build/cmake
            SYSTEM TRUE

            # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/lz4
            GIT_SHALLOW TRUE
            OVERRIDE_FIND_PACKAGE TRUE
            USES_TERMINAL_DOWNLOAD TRUE
            GIT_PROGRESS TRUE
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            EXCLUDE_FROM_ALL

        )
        FetchContent_MakeAvailable(lz4)
        set(LZ4_LIBRARIES lz4_static CACHE INTERNAL "")

        FetchContent_Declare(protozero
            GIT_REPOSITORY https://github.com/mapbox/protozero
            GIT_TAG v1.8.1
            SOURCE_SUBDIR cmake

            GIT_SUBMODULES_RECURSE OFF
            # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/protozero
            GIT_SHALLOW TRUE
            OVERRIDE_FIND_PACKAGE TRUE
            USES_TERMINAL_DOWNLOAD TRUE
            GIT_PROGRESS TRUE
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(protozero)
        set(PROTOZERO_INCLUDE_DIR "${protozero_SOURCE_DIR}/include")
        set(PROTOZERO_FOUND 1 CACHE INTERNAL "")

        FetchContent_Declare(
            expat
            GIT_REPOSITORY https://github.com/libexpat/libexpat/
            GIT_TAG R_2_7_3
            SOURCE_SUBDIR expat/

            # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/expat
            GIT_SHALLOW TRUE
            OVERRIDE_FIND_PACKAGE TRUE
            USES_TERMINAL_DOWNLOAD TRUE
            GIT_PROGRESS TRUE
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(expat)
        set(EXPAT_FOUND 1 CACHE INTERNAL "")
        add_library(EXPAT::EXPAT ALIAS expat)
    endif()
    set(Boost_USE_STATIC_LIBS ${BUILD_STATIC_LIBS})
    find_package(Boost COMPONENTS program_options)
    if(Boost_FOUND)
        set(BUILD_TESTING 0 CACHE INTERNAL "")
        set(BUILD_DATA_TESTS 0 CACHE INTERNAL "")
        set(BUILD_EXAMPLES 0 CACHE INTERNAL "")
        set(BUILD_BENCHMARKS 0 CACHE INTERNAL "")
        set(Osmium_USE_GEOS 0 CACHE INTERNAL "")
        set(Osmium_USE_GDAL 0 CACHE INTERNAL "")
        set(CPPCHECK NOTFOUND CACHE INTERNAL "")
        set(Osmium_USE_GEOS 0 CACHE INTERNAL "")
        set(Osmium_USE_GDAL 0 CACHE INTERNAL "")

        if(NOT OSMIUM_INCLUDE_DIR)
            if(boost_SOURCE_DIR)
                include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
                include_directories(BEFORE SYSTEM
                    ${boost_SOURCE_DIR}/libs/numeric/conversion/include
                )
            endif()
            if(FETCH_DEPS)
                set(FETCH_OSMIUM 1 CACHE INTERNAL "")
            endif()

            set(Osmium_DEBUG 1 CACHE INTERNAL "")
            # TODO: support system installed libosmium
            if(NOT FETCH_OSMIUM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/libosmium/CMakeLists.txt)
                list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/libosmium/cmake")
                set(Osmium_USE_LZ4 1 CACHE INTERNAL "")
                add_subdirectory(mapgen/earth/libosmium)
                set(OSMIUM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/libosmium/include ${Boost_INCLUDE_DIRS})
                find_package(BZip2)
                find_package(EXPAT)
            else()
                FetchContent_Declare(libosmium
                    GIT_REPOSITORY https://github.com/osmcode/libosmium
                    GIT_TAG v2.22.0
                    # SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/libosmium
                    SOURCE_SUBDIR cmake
                    GIT_SUBMODULES_RECURSE OFF

                    GIT_SHALLOW TRUE
                    OVERRIDE_FIND_PACKAGE TRUE
                    USES_TERMINAL_DOWNLOAD TRUE
                    GIT_PROGRESS TRUE
                    DOWNLOAD_EXTRACT_TIMESTAMP ON
                    EXCLUDE_FROM_ALL

                )
                FetchContent_MakeAvailable(libosmium)
                list(APPEND OSMIUM_INCLUDE_DIR ${libosmium_SOURCE_DIR}/include)
            endif()

            list(APPEND OSMIUM_INCLUDE_DIR ${PROTOZERO_INCLUDE_DIR})
            list(APPEND OSMIUM_LIRARY Boost::headers)
            if(BZIP2_FOUND)
                list(APPEND OSMIUM_LIRARY BZip2::BZip2)
            endif()
            if(EXPAT_FOUND)
                list(APPEND OSMIUM_LIRARY EXPAT::EXPAT)
            endif()

            include_directories(BEFORE SYSTEM ${OSMIUM_INCLUDE_DIR})
        endif()
        set(USE_OSMIUM 1)
        message(STATUS "Using osmium ${USE_OSMIUM}: ${OSMIUM_INCLUDE_DIR} : ${OSMIUM_LIRARY}")
        list(APPEND FREEMINER_COMMON_LIBRARIES ${OSMIUM_LIRARY})

        option(ENABLE_OSMIUM_TOOL "Enable Osmium tool" 1)
        if(ENABLE_OSMIUM_TOOL AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/osmium-tool/CMakeLists.txt)
            set(USE_OSMIUM_TOOL 1)
        endif()

        if(USE_OSMIUM_TOOL)
            #add_subdirectory(mapgen/earth/json)
            #set(NLOHMANN_INCLUDE_DIR mapgen/earth/json/include)
            #include_directories(BEFORE SYSTEM ${NLOHMANN_INCLUDE_DIR})
            list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/osmium-tool/cmake/Modules/")
            # add_subdirectory(mapgen/earth/osmium-tool)
            set(OSMIUM_TOOL_LIBRARY osmium-tool-lib)
            set(OSMIUM_TOOL_SRC mapgen/earth/osmium-tool/src/)

            configure_file(${OSMIUM_TOOL_SRC}/version.cpp.in ${PROJECT_BINARY_DIR}/${OSMIUM_TOOL_SRC}/version.cpp)

            add_library(${OSMIUM_TOOL_LIBRARY}
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
            target_link_libraries(${OSMIUM_TOOL_LIBRARY}
                PRIVATE ${OSMIUM_LIRARY}
                PUBLIC Boost::program_options)
            target_include_directories(${OSMIUM_TOOL_LIBRARY} PRIVATE ${OSMIUM_INCLUDE_DIR})

            list(APPEND FREEMINER_COMMON_LIBRARIES ${OSMIUM_TOOL_LIBRARY})

        endif()
        message(STATUS "Using osmiumtool ${USE_OSMIUM_TOOL} : ${OSMIUM_TOOL_LIBRARY}")
    endif()
endif()

option(ENABLE_ICONV "Enable utf8 convert via iconv " FALSE)

if(ENABLE_ICONV)
    find_package(Iconv)
    if(ICONV_INCLUDE_DIR)
        set(USE_ICONV 1)
        message(STATUS "iconv.h found: ${ICONV_INCLUDE_DIR}")
    else()
        message(STATUS "iconv.h NOT found")
    endif()
endif()

if(NOT USE_ICONV)
    set(USE_ICONV 0)
endif()

#option(ENABLE_MANDELBULBER "Use Mandelbulber source to generate more fractals in math mapgen" OFF)
set(USE_MANDELBULBER 1)
#find_package(Mandelbulber)

option(ENABLE_IPV4_DEFAULT "Do not use ipv6 dual socket " FALSE)
if(ENABLE_IPV4_DEFAULT)
    set(USE_IPV4_DEFAULT 1)
else()
    set(USE_IPV4_DEFAULT 0)
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ${CMAKE_VERSION} VERSION_GREATER "3.11.0")
    # set(USE_DEBUG_DUMP ON CACHE BOOL "")
endif()

if(USE_DEBUG_DUMP)
    #get_target_property(MAGIC_ENUM_INCLUDE_DIR ch_contrib::magic_enum INTERFACE_INCLUDE_DIRECTORIES)
    # CMake generator expression will do insane quoting when it encounters special character like quotes, spaces, etc.
    # Prefixing "SHELL:" will force it to use the original text.
    #set (INCLUDE_DEBUG_HELPERS "SHELL:-I\"${MAGIC_ENUM_INCLUDE_DIR}\" -include \"${ClickHouse_SOURCE_DIR}/base/base/dump.h\"")
    set(INCLUDE_DEBUG_HELPERS "SHELL:-I\"${CMAKE_CURRENT_SOURCE_DIR}/debug/\" -include \"${CMAKE_CURRENT_SOURCE_DIR}/debug/dump.h\"")
    #set (INCLUDE_DEBUG_HELPERS "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/util/dump.h\"")
    # Use generator expression as we don't want to pollute CMAKE_CXX_FLAGS, which will interfere with CMake check system.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${INCLUDE_DEBUG_HELPERS}>)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DDUMP_STREAM=actionstream)
    else()
        #add_definitions(-DDUMP_STREAM=verbosestream)
        add_definitions(-DDUMP_STREAM=actionstream)
    endif()
endif()

set(FMcommon_SRCS ${FMcommon_SRCS}
    circuit_element_virtual.cpp
    circuit_element.cpp
    circuit.cpp
    content_abm_grow_tree.cpp
    content_abm.cpp
    content_abm_core.cpp
    content_abm_erosion.cpp
    content_abm_evaporation.cpp
    content_abm_growth.cpp
    content_abm_precipitation.cpp
    fm_abm_world.cpp
    fm_abm.cpp
    fm_bitset.cpp
    fm_clientiface.cpp
    fm_far_calc.cpp
    fm_liquid.cpp
    fm_map.cpp
    fm_server.cpp
    fm_serverenvironment.cpp
    fm_util.cpp
    fm_world_merge.cpp
    key_value_storage.cpp
    log_types.cpp
    stat.cpp
)

list(APPEND FREEMINER_COMMON_LIBRARIES
    ${MSGPACK_LIBRARY}
)

list(APPEND FREEMINER_CLIENT_LIBRARIES
    ${FREEMINER_COMMON_LIBRARIES}
)

if(NOT PNG_LIBRARY)
    find_package(PNG REQUIRED)
endif()

message(STATUS "Using server PNG: ${PNG_LIBRARY}  : ${PNG_INCLUDE_DIR} : ? ${PNG_PNG_INCLUDE_DIR}")
if(NOT PNG_INCLUDE_DIR AND PNG_PNG_INCLUDE_DIR)
    set(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE INTERNAL "")
endif()
include_directories(${PNG_INCLUDE_DIR})

list(APPEND FREEMINER_SERVER_LIBRARIES
    ${FREEMINER_COMMON_LIBRARIES}
    ${PNG_LIBRARY}
)
