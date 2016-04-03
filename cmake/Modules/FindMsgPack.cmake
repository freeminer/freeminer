# - Find MsgPack includes and library
#
# This module defines
#  MSGPACK_INCLUDE_DIR
#  MSGPACK_LIBRARY, the libraries to link against to use MSGPACK.
#  MSGPACK_FOUND, If false, do not try to use MSGPACK
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# Minimal supported version: 1.4.0
if(CMAKE_VERSION VERSION_LESS 2.8.9)
  message(WARNING "Your cmake is bad and you should feel bad! (at least 2.8.9 is recommended)")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
  message(WARNING "Using system msgpack (insufficient gcc version (${CMAKE_CXX_COMPILER_VERSION})). Minimal supported msgpack version is 1.4.0")
  set(ENABLE_SYSTEM_MSGPACK 1)
endif()


# msgpack 1.2.0 recompiles all .h every cmake run - it cause recompile all freeminer src.
if(NOT ENABLE_SYSTEM_MSGPACK AND NOT MSGPACK_LIBRARY)
	FIND_PATH(MSGPACK_INCLUDE_DIR msgpack.hpp PATHS ${CMAKE_HOME_DIRECTORY}/src/msgpack-c/include NO_DEFAULT_PATH)
	FIND_LIBRARY(MSGPACK_LIBRARY NAMES libmsgpackc.a msgpackc msgpack PATHS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/src/msgpack-c NO_DEFAULT_PATH)
	IF (MSGPACK_LIBRARY)
		message(STATUS "Using already compiled bundled msgpack ${MSGPACK_INCLUDE_DIR} ${MSGPACK_LIBRARY}")
	else()
		set(MSGPACK_INCLUDE_DIR NOTFOUND)
	endif()
endif()


if(ENABLE_SYSTEM_MSGPACK OR MSGPACK_LIBRARY OR MSGPACK_INCLUDE_DIR)

IF (MSGPACK_LIBRARY AND MSGPACK_INCLUDE_DIR)
    SET(MSGPACK_FIND_QUIETLY TRUE) # Already in cache, be silent
ENDIF ()

FIND_PATH(MSGPACK_INCLUDE_DIR msgpack.hpp)

FIND_LIBRARY(MSGPACK_LIBRARY NAMES msgpack msgpackc PATHS)

MARK_AS_ADVANCED(MSGPACK_INCLUDE_DIR MSGPACK_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(msgpack DEFAULT_MSG MSGPACK_LIBRARY MSGPACK_INCLUDE_DIR)

elseif(NOT MSGPACK_LIBRARY)
	if(NOT MSVC)
		set(MSGPACK_CXX11 ON)
	endif()
	set(MSGPACK_BUILD_EXAMPLES OFF)
	set(MSGPACK_BUILD_TESTS OFF)
	set(MSGPACK_ENABLE_SHARED OFF)
	add_subdirectory(msgpack-c)
	#include_directories(${PROJECT_SOURCE_DIR}/msgpack-c/include)
	set(MSGPACK_LIBRARY msgpackc-static) # before 1.4.0 was msgpack-static
	set(MSGPACK_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/msgpack-c/include)
	message(STATUS "Using bundled msgpack ${MSGPACK_INCLUDE_DIR} ${MSGPACK_LIBRARY}")
endif()
