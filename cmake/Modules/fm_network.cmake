# Recompute detected features; never reuse results from an earlier configure.
foreach(feature ENET SCTP WEBSOCKET WEBSOCKET_SCTP CLIENT_MCP)
    unset(USE_${feature} CACHE)
    set(USE_${feature} 0)
endforeach()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    option(USE_MULTI "Enable MT+ENET+WSS networking" 1)
endif()

if(USE_MULTI)
    set(ENABLE_ENET 1 CACHE BOOL "")
    if(NOT ANDROID)
        set(ENABLE_WEBSOCKET 0 CACHE BOOL "")
    endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR USE_MULTI)
    option(ENABLE_SCTP "Enable SCTP networking (EXPERIMENTAL)" 1)
endif()

set(SCTP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/usrsctp")
set(FM_HAVE_SCTP_SOURCE FALSE)
if(EXISTS "${SCTP_SOURCE_DIR}/CMakeLists.txt" AND EXISTS "${SCTP_SOURCE_DIR}/usrsctplib")
    set(FM_HAVE_SCTP_SOURCE TRUE)
elseif(ENABLE_SCTP OR ENABLE_WEBSOCKET_SCTP)
    message(WARNING "SCTP transports disabled: source missing at ${SCTP_SOURCE_DIR}")
endif()

if(ENABLE_WEBSOCKET OR (ENABLE_WEBSOCKET_SCTP AND FM_HAVE_SCTP_SOURCE))
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp/websocketpp/config/asio.hpp")
        if(NOT TARGET Boost::headers)
            find_package(Boost QUIET)
        endif()
        if(TARGET Boost::headers)
            if(boost_SOURCE_DIR)
                target_include_directories(fm_dependencies SYSTEM INTERFACE ${Boost_INCLUDE_DIRS})
            endif()

            target_include_directories(
                fm_dependencies SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp"
            )
            if(NOT TARGET OpenSSL::SSL)
                find_package(OpenSSL QUIET)
            endif()

            set(WEBSOCKETPP_LIBRARY Boost::headers)
            if(TARGET OpenSSL::SSL)
                set(WEBSOCKETPP_LIBRARY ${WEBSOCKETPP_LIBRARY} OpenSSL::SSL)
            endif()

            if(ENABLE_WEBSOCKET)
                set(USE_WEBSOCKET 1)
            endif()

            if(ENABLE_WEBSOCKET_SCTP AND FM_HAVE_SCTP_SOURCE)
                set(USE_WEBSOCKET_SCTP 1)
            endif()
            message(
                STATUS
                "Using websocket ${USE_WEBSOCKET},${USE_WEBSOCKET_SCTP}: ${CMAKE_CURRENT_SOURCE_DIR}/external/websocketpp : ${WEBSOCKETPP_LIBRARY}"
            )
            list(APPEND FREEMINER_COMMON_LIBRARIES ${WEBSOCKETPP_LIBRARY})
        endif()
    endif()
endif()

set(USE_CLIENT_MCP "${USE_WEBSOCKET}")

if(FM_HAVE_SCTP_SOURCE AND (ENABLE_SCTP OR USE_WEBSOCKET_SCTP))
    block(SCOPE_FOR VARIABLES)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        if(SCTP_DEBUG)
            set(sctp_debug ON)
            target_compile_definitions(fm_dependencies INTERFACE SCTP_DEBUG=1)
        else()
            set(sctp_debug OFF)
        endif()
        set(sctp_build_programs OFF)
        set(sctp_build_fuzzer OFF)
        set(sctp_werror OFF)

        add_subdirectory("${SCTP_SOURCE_DIR}" external/usrsctp EXCLUDE_FROM_ALL)
    endblock()

    set(SCTP_LIBRARY usrsctp)
    if(ANDROID)
        target_compile_definitions(${SCTP_LIBRARY} PRIVATE __Userspace_os_Linux)
    endif()

    set(USE_SCTP 1)

    message(STATUS "Using sctp: ${SCTP_SOURCE_DIR} ${SCTP_LIBRARY} SCTP_DEBUG=${SCTP_DEBUG}")
    list(APPEND FREEMINER_COMMON_LIBRARIES ${SCTP_LIBRARY})
endif()

if(ENABLE_ENET)
    if(NOT ENABLE_SYSTEM_ENET AND EXISTS
        ${CMAKE_CURRENT_SOURCE_DIR}/external/enet/include/enet/enet.h
    )
        add_subdirectory(external/enet)
        set(ENET_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/enet/include)
        set(ENET_LIBRARY enet)
    else()
        find_library(ENET_LIBRARY NAMES enet)
        find_path(ENET_INCLUDE_DIR enet/enet.h)
    endif()
    if(ENET_LIBRARY AND ENET_INCLUDE_DIR)
        target_include_directories(fm_dependencies SYSTEM INTERFACE ${ENET_INCLUDE_DIR})
        set(USE_ENET 1)
        list(APPEND FREEMINER_COMMON_LIBRARIES ${ENET_LIBRARY})
        message(STATUS "Using enet ${USE_ENET}: ${ENET_INCLUDE_DIR} ${ENET_LIBRARY}")
    endif()
endif()

option(ENABLE_IPV4_DEFAULT "Do not use ipv6 dual socket " FALSE)
if(ENABLE_IPV4_DEFAULT)
    set(USE_IPV4_DEFAULT 1)
else()
    set(USE_IPV4_DEFAULT 0)
endif()
