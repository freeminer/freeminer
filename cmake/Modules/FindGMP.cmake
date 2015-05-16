
option(ENABLE_SYSTEM_GMP "Use GMP from system" TRUE)
mark_as_advanced(GMP_LIBRARY GMP_INCLUDE_DIR)
set(USE_SYSTEM_GMP FALSE)

if(ENABLE_SYSTEM_GMP)
	find_library(GMP_LIBRARY NAMES libgmp.so)
	find_path(GMP_INCLUDE_DIR NAMES gmp.h)

	if(GMP_LIBRARY AND GMP_INCLUDE_DIR)
		message (STATUS "Using GMP provided by system.")
		set(USE_SYSTEM_GMP TRUE)
	else()
		message (STATUS "Detecting GMP from system failed.")
	endif()
else()
	message (STATUS "Detecting GMP from system disabled! (ENABLE_SYSTEM_GMP=0)")
endif()

if(NOT USE_SYSTEM_GMP)
	message(STATUS "Using bundled mini-gmp library.")
	set(GMP_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/gmp)
	set(GMP_LIBRARY gmp)
	add_subdirectory(gmp)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_LIBRARY GMP_INCLUDE_DIR)
