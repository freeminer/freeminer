# Look for JsonCpp, with fallback to bundeled version

mark_as_advanced(JSON_LIBRARY JSON_INCLUDE_DIR)
if (NOT USE_LIBCXX)
option(ENABLE_SYSTEM_JSONCPP "Enable using a system-wide JsonCpp" TRUE)
endif()
set(USE_SYSTEM_JSONCPP FALSE)

if(ENABLE_SYSTEM_JSONCPP)
	find_library(JSON_LIBRARY NAMES jsoncpp)
	find_path(JSON_INCLUDE_DIR json/allocator.h PATH_SUFFIXES jsoncpp)

	if(JSON_LIBRARY AND JSON_INCLUDE_DIR)
		message(STATUS "Using JsonCpp provided by system.")
		set(USE_SYSTEM_JSONCPP TRUE)
	endif()
endif()

if(NOT USE_SYSTEM_JSONCPP)
	option(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF)
	option(JSONCPP_WITH_TESTS OFF)
	option(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
	option(JSONCPP_WITH_WARNING_AS_ERROR OFF)
	add_subdirectory(src/external/jsoncpp)
	message(STATUS "Using bundled JSONCPP library. ${jsoncpp_BINARY_DIR}")
	# Cant use SYSTEM BEFORE because tiniergltf uses this
	#set(JSON_INCLUDE_DIR SYSTEM BEFORE ${PROJECT_SOURCE_DIR}/src/external/jsoncpp/include)
	set(JSON_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/external/jsoncpp/include)
	
	set(JSON_LIBRARY jsoncpp_static)
	#set(JSON_LIBRARY jsoncpp_lib)

	#set(JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/jsoncpp)
	#set(JSON_LIBRARY jsoncpp)
	#add_subdirectory(lib/jsoncpp)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Json DEFAULT_MSG JSON_LIBRARY JSON_INCLUDE_DIR)
