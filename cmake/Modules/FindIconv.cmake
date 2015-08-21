mark_as_advanced(ICONV_INCLUDE_DIR ICONV_LIBRARY)
find_path(ICONV_INCLUDE_DIR iconv.h)

if(APPLE)
	find_library(ICONV_LIBRARY
		NAMES libiconv.dylib
		PATHS "/usr/lib"
		DOC "IConv library")
endif(APPLE)

if(${CMAKE_SYSTEM_NAME} MATCHES "BSD")
	FIND_LIBRARY(ICONV_LIBRARY NAMES iconv)
endif()

#include(FindPackageHandleStandardArgs)
#find_package_handle_standard_args(iconv DEFAULT_MSG ICONV_INCLUDE_DIR)
