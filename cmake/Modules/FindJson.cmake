# Look for JSONCPP if asked to.
# We use a bundled version by default because some distros ship versions of
# JSONCPP that cause segfaults and other memory errors when we link with them.
# See https://github.com/minetest/minetest/issues/1793

mark_as_advanced(JSON_LIBRARY JSON_INCLUDE_DIR)
option(ENABLE_SYSTEM_JSONCPP "Enable using a system-wide JSONCPP.  May cause segfaults and other memory errors!" FALSE)

if(ENABLE_SYSTEM_JSONCPP)
	find_library(JSON_LIBRARY NAMES jsoncpp)
	find_path(JSON_INCLUDE_DIR json/allocator.h PATH_SUFFIXES jsoncpp)

	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(JSONCPP DEFAULT_MSG JSON_LIBRARY JSON_INCLUDE_DIR)

	if(JSONCPP_FOUND)
		message(STATUS "Using system JSONCPP library ${JSON_LIBRARY} : ${JSON_INCLUDE_DIR}")
	endif()
endif()

if(NOT JSONCPP_FOUND)
	option(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF)
	option(JSONCPP_WITH_TESTS OFF)
	option(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
	option(JSONCPP_WITH_WARNING_AS_ERROR OFF)
	add_subdirectory(external/jsoncpp)
	message(STATUS "Using bundled JSONCPP library. ${jsoncpp_BINARY_DIR}")
	set(JSON_INCLUDE_DIR BEFORE SYSTEM ${PROJECT_SOURCE_DIR}/external/jsoncpp/include)
	set(JSON_LIBRARY jsoncpp_static)
endif()

