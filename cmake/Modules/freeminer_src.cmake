
# == freeminer:
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
	OPTION(ENABLE_SCTP "Enable SCTP networking (EXPERIMENTAL)" 0)
	OPTION(USE_MULTI "Enable MT+ENET+WSS networking" 1)
endif()

if(USE_MULTI)
	#set(ENABLE_SCTP 1 CACHE BOOL "") # Maybe bugs
	set(ENABLE_ENET 1 CACHE BOOL "")
	#set(ENABLE_WEBSOCKET_SCTP 1 CACHE BOOL "") # NOT FINISHED
        if (NOT ANDROID)
	    	set(ENABLE_WEBSOCKET 1 CACHE BOOL "")
        endif()
endif()


if(ENABLE_WEBSOCKET OR ENABLE_WEBSOCKET_SCTP)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp/CMakeLists.txt)
		find_package(Boost)
		if(Boost_FOUND)
			include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp)
			# add_subdirectory(external/websocketpp)
			# set(WEBSOCKETPP_LIBRARY websocketpp::websocketpp)
			message(STATUS "Using websocket: ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp")
			find_package(OpenSSL)
			set(WEBSOCKETPP_LIBRARY ${WEBSOCKETPP_LIBRARY} OpenSSL::SSL)
			set(USE_WEBSOCKET 1 CACHE BOOL "")
			#TODO:
			# set(USE_WEBSOCKET_SCTP 1 CACHE BOOL "")
		endif()
	else()
		#set(USE_WEBSOCKET 0)
		#set(USE_WEBSOCKET_SCTP 0)
    endif()
endif()

if(ENABLE_SCTP AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp/usrsctplib)
    message(WARNING "Please Clone usrsctp:  git clone --depth 1 https://github.com/sctplab/usrsctp ${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp")
    set(ENABLE_SCTP 0)
endif()

if(ENABLE_SCTP)
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
#else()
	#set(USE_SCTP 0)
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
		message(STATUS "Using enet: ${ENET_INCLUDE_DIR} ${ENET_LIBRARY}")
		set(USE_ENET 1)
	endif()
endif()

#set(TinyTIFF_BUILD_TESTS 0 CACHE INTERNAL "")
#add_subdirectory(external/TinyTIFF/src)
#set(TINYTIFF_LIRARY TinyTIFF)

option(ENABLE_TIFF "Enable tiff (feotiff for mapgen earth)" 1)
if(ENABLE_TIFF AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/CMakeLists.txt)
	set(tiff-tools 0 CACHE INTERNAL "")
	set(tiff-tests 0 CACHE INTERNAL "")
	set(tiff-docs 0 CACHE INTERNAL "")
	add_subdirectory(external/libtiff)
	set(TIFF_LIRARY TIFF::tiff)
	set(TIFF_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/libtiff/libtiff ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/libtiff)
	include_directories(BEFORE SYSTEM ${TIFF_INCLUDE_DIR})
	message(STATUS "Using tiff: ${TIFF_INCLUDE_DIR} ${TIFF_LIRARY}")
	set(USE_TIFF 1)
endif()

option(ENABLE_OSMIUM "Enable Osmium" 1)

if (ENABLE_OSMIUM)
	find_path(OSMIUM_INCLUDE_DIR osmium/osm.hpp)
endif()

if(ENABLE_OSMIUM AND (OSMIUM_INCLUDE_DIR OR EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/libosmium/CMakeLists.txt))
	find_package(Boost)
	if(Boost_FOUND)
		set(BUILD_TESTING 0 CACHE INTERNAL "")
		set(BUILD_DATA_TESTS 0 CACHE INTERNAL "")
		set(BUILD_EXAMPLES 0 CACHE INTERNAL "")
		set(BUILD_BENCHMARKS 0 CACHE INTERNAL "")

		if (NOT OSMIUM_INCLUDE_DIR)
			add_subdirectory(external/libosmium)
			set(OSMIUM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/libosmium/include)
			include_directories(BEFORE SYSTEM ${OSMIUM_INCLUDE_DIR})
		endif()
		find_package(BZip2)
		if(BZIP2_FOUND)
			set (OSMIUM_LIRARY ${OSMIUM_LIRARY} BZip2::BZip2)
		endif()
		find_package(EXPAT)
		if(EXPAT_FOUND)
			set (OSMIUM_LIRARY ${OSMIUM_LIRARY} EXPAT::EXPAT)
		endif()
		set(USE_OSMIUM 1)
		message(STATUS "Using osmium: ${OSMIUM_INCLUDE_DIR} : ${OSMIUM_LIRARY}")
	endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND ${CMAKE_VERSION} VERSION_GREATER "3.11.0")
    set(USE_DEBUG_DUMP ON CACHE BOOL "")
endif()

if (USE_DEBUG_DUMP)
    #get_target_property(MAGIC_ENUM_INCLUDE_DIR ch_contrib::magic_enum INTERFACE_INCLUDE_DIRECTORIES)
    # CMake generator expression will do insane quoting when it encounters special character like quotes, spaces, etc.
    # Prefixing "SHELL:" will force it to use the original text.
    #set (INCLUDE_DEBUG_HELPERS "SHELL:-I\"${MAGIC_ENUM_INCLUDE_DIR}\" -include \"${ClickHouse_SOURCE_DIR}/base/base/dump.h\"")
    set (INCLUDE_DEBUG_HELPERS "SHELL:-I\"${CMAKE_CURRENT_SOURCE_DIR}/debug/\" -include \"${CMAKE_CURRENT_SOURCE_DIR}/debug/dump.h\"")
    #set (INCLUDE_DEBUG_HELPERS "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/util/dump.h\"")
    # Use generator expression as we don't want to pollute CMAKE_CXX_FLAGS, which will interfere with CMake check system.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${INCLUDE_DEBUG_HELPERS}>)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_definitions(-DDUMP_STREAM=actionstream)
	else()
		#add_definitions(-DDUMP_STREAM=verbosestream)
		add_definitions(-DDUMP_STREAM=actionstream)
	endif()
endif ()

set(FMcommon_SRCS ${FMcommon_SRCS}
	circuit_element_virtual.cpp
	circuit_element.cpp
	circuit.cpp
	fm_abm_world.cpp
	fm_bitset.cpp
	fm_liquid.cpp
	fm_map.cpp
	fm_server.cpp
	fm_world_merge.cpp
	fm_far_calc.cpp
	key_value_storage.cpp
	log_types.cpp
	stat.cpp
	content_abm_grow_tree.cpp
	content_abm.cpp
	fm_abm.cpp
	fm_clientiface.cpp
	fm_serverenvironment.cpp
	${DEBUG_SRCS}
	)
# == end freeminer:
