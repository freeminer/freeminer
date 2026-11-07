option(ENABLE_LEVELDB "Enable LevelDB backend" TRUE)
set(USE_LEVELDB FALSE)

if(ENABLE_LEVELDB)

    if(LIBCXX_LIBRARY)
        set(SYSTEM_LEVELDB 0 CACHE BOOL "")
    endif()

    if(SYSTEM_LEVELDB)
        if(USE_STATIC_LIBRARIES AND LEVELDB_STATIC)
            list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
        endif()

        find_library(LEVELDB_LIBRARY NAMES leveldb libleveldb)
        find_path(LEVELDB_INCLUDE_DIR db.h PATH_SUFFIXES leveldb)
        find_library(SNAPPY_LIBRARY snappy)
        find_path(SNAPPY_INCLUDE_DIR snappy.h PATH_SUFFIXES snappy)

        if(USE_STATIC_LIBRARIES AND LEVELDB_STATIC)
            list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
        endif()

    endif()

    message(STATUS "Snappy library: ${SNAPPY_LIBRARY} : ${SNAPPY_INCLUDE_DIR}")
    message(STATUS "Leveldb library: ${LEVELDB_LIBRARY} : ${LEVELDB_INCLUDE_DIR}")

    if((NOT SNAPPY_INCLUDE_DIR OR NOT SNAPPY_LIBRARY) AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/snappy/snappy.h)
        set(SNAPPY_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/snappy)
        # Snappy have no cmake by default,
        # git clone --recursive --depth 1 https://github.com/google/snappy src/external/snappy
        # But we can collect something from pulls
        # wget -O src/external/snappy/CMakeLists.txt https://raw.githubusercontent.com/adasworks/snappy-cmake/master/CMakeLists.txt
        set(SNAPPY_BUILD_TESTS 0 CACHE INTERNAL "")
        set(SNAPPY_BUILD_BENCHMARKS 0 CACHE INTERNAL "")
        set(SNAPPY_INSTALL 0 CACHE INTERNAL "")
        set(HAVE_TCMALLOC 0 CACHE INTERNAL "")

        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/snappy)
        set(SNAPPY_LIBRARY snappy)
        set(HAVE_SNAPPY 1 CACHE INTERNAL "")
        message(STATUS "Using bundled snappy ${SNAPPY_LIBRARY} : ${SNAPPY_INCLUDE_DIR}")
    endif()

    if((NOT LEVELDB_INCLUDE_DIR OR NOT LEVELDB_LIBRARY) AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/leveldb/include/leveldb/db.h)
        set(LEVELDB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/leveldb/include)
        # bad try direct make build 
        #find_path(LEVELDB_MAKEFILE_DIR Makefile ${PROJECT_SOURCE_DIR}/external/leveldb NO_DEFAULT_PATH)
        #if (LEVELDB_MAKEFILE_DIR)
        #	execute_process(COMMAND "make -f ${LEVELDB_MAKEFILE_DIR}/Makefile" WORKING_DIRECTORY LEVELDB_MAKEFILE_DIR OUTPUT_VARIABLE LMKE ERROR_VARIABLE LMKE)
        #message(STATUS "leveldb mk=${LMKE}")
        #endif()
        #
        # good cmake try
        # Leveldb have no cmake by default,
        # git clone --recursive --depth 1 https://github.com/google/leveldb src/external/leveldb
        # But we can collect something from pulls
        # wget -O src/external/leveldb/CMakeLists.txt https://raw.githubusercontent.com/proller/leveldb/patch-2/CMakeLists.txt
        # wget -O src/external/leveldb/leveldbConfig.cmake.in https://raw.githubusercontent.com/tamaskenez/leveldb-cmake-win/native_windows_port_1_18/leveldbConfig.cmake.in
        set(LEVELDB_BUILD_TESTS 0 CACHE INTERNAL "")
        set(LEVELDB_BUILD_BENCHMARKS 0 CACHE INTERNAL "")
        set(LEVELDB_INSTALL 0 CACHE INTERNAL "")
        set(HAVE_CLANG_THREAD_SAFETY 0 CACHE INTERNAL "") # -Werror remove
        if(EMSCRIPTEN) # BUG?
            include_directories(${ZSTD_INCLUDE_DIR})
        endif()

        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/leveldb)
        set(LEVELDB_LIBRARY leveldb)
        message(STATUS "Using bundled leveldb ${LEVELDB_INCLUDE_DIR} ${LEVELDB_LIBRARY}")
    endif()

    if(SNAPPY_LIBRARY AND SNAPPY_INCLUDE_DIR)
        include_directories(SYSTEM ${SNAPPY_INCLUDE_DIR})
    endif()

    if(LEVELDB_LIBRARY AND LEVELDB_INCLUDE_DIR)
        set(USE_LEVELDB TRUE)
        message(STATUS "LevelDB backend enabled. ${LEVELDB_LIBRARY} : ${LEVELDB_INCLUDE_DIR}")
        include_directories(SYSTEM ${LEVELDB_INCLUDE_DIR})
    elseif(NOT FORCE_LEVELDB)
        message(WARNING "LevelDB not found! Player data cannot be saved in singleplayer or server")
    endif()
endif(ENABLE_LEVELDB)

# this is needed because VS builds install LevelDB via nuget
if(FORCE_LEVELDB)
    set(USE_LEVELDB 1)
endif()
