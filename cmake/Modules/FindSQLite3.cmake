mark_as_advanced(SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

if (NOT SQLITE3_INCLUDE_DIR AND NOT SQLITE3_LIBRARY)

find_path(SQLITE3_INCLUDE_DIR sqlite3.h)

find_library(SQLITE3_LIBRARY NAMES sqlite3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

endif()

if (NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIBRARY)
	find_path(SQLITE3_INCLUDE_DIR sqlite3.h ${PROJECT_SOURCE_DIR}/external/sqlite3 NO_DEFAULT_PATH)
	if (SQLITE3_INCLUDE_DIR)
		add_subdirectory(${PROJECT_SOURCE_DIR}/external/sqlite3)
		set(SQLITE3_LIBRARY sqlite3)
		message(STATUS "Using bundled sqlite3 ${SQLITE3_INCLUDE_DIR} ${SQLITE3_LIBRARY}")
	endif()
endif()
