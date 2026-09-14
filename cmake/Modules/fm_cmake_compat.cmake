# SOURCE_SUBDIR in FetchContent_MakeAvailable requires CMake 3.18.
if(CMAKE_VERSION VERSION_LESS 3.18)
    message(FATAL_ERROR "Freeminer requires CMake 3.18 or newer")
endif()

# Older FetchContent versions do not recognize these optional keywords.
set(FM_FETCH_OVERRIDE_FIND_PACKAGE)
if(NOT CMAKE_VERSION VERSION_LESS 3.24)
    set(FM_FETCH_OVERRIDE_FIND_PACKAGE OVERRIDE_FIND_PACKAGE)
endif()
set(FM_FETCH_SYSTEM)
if(NOT CMAKE_VERSION VERSION_LESS 3.25)
    set(FM_FETCH_SYSTEM SYSTEM)
endif()
set(FM_FETCH_EXCLUDE_FROM_ALL)
if(NOT CMAKE_VERSION VERSION_LESS 3.28)
    set(FM_FETCH_EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
endif()
