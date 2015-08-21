# - Find MsgPack includes and library
#
# This module defines
#  MSGPACK_INCLUDE_DIR
#  MSGPACK_LIBRARY, the libraries to link against to use MSGPACK.
#  MSGPACK_FOUND, If false, do not try to use MSGPACK
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# Minimal supported version: 1.1.0
if(NOT CMAKE_CXX_COMPILER_VERSION OR (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7))
	message(WARNING "Using system msgpack (compiler too old). it can be too old. recommended min version=1.1.0")
	set(ENABLE_SYSTEM_MSGPACK 1)
endif()

if(ENABLE_SYSTEM_MSGPACK OR MSGPACK_LIBRARY OR MSGPACK_INCLUDE_DIR)

IF (MSGPACK_LIBRARY AND MSGPACK_INCLUDE_DIR)
    SET(MSGPACK_FIND_QUIETLY TRUE) # Already in cache, be silent
ENDIF ()

FIND_PATH(MSGPACK_INCLUDE_DIR msgpack.hpp)

FIND_LIBRARY(MSGPACK_LIBRARY NAMES msgpack PATHS)

MARK_AS_ADVANCED(MSGPACK_INCLUDE_DIR MSGPACK_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(msgpack DEFAULT_MSG MSGPACK_LIBRARY MSGPACK_INCLUDE_DIR)

elseif(NOT MSGPACK_LIBRARY)

	add_subdirectory(msgpack-c)
	#include_directories(${PROJECT_SOURCE_DIR}/msgpack-c/include)
	set(MSGPACK_LIBRARY msgpack)
	set(MSGPACK_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/msgpack-c/include)
	message(STATUS "Using bundled msgpack ${MSGPACK_INCLUDE_DIR}")
endif()
