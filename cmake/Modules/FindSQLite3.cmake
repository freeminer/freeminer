mark_as_advanced(SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR) 

find_path(SQLITE3_INCLUDE_DIR sqlite3.h)

find_library(SQLITE3_LIBRARY NAMES sqlite3)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

