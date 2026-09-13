if(USE_OSMIUM)
     file(GLOB_RECURSE mapgen_earth_SRCS CONFIGURE_DEPENDS
          "${CMAKE_CURRENT_SOURCE_DIR}/arnis-cpp/src/*.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/arnis-cpp/src/*.cpp"
     )
     list(APPEND mapgen_earth_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/arnis_adapter.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/blocks.cpp
     )
     message(STATUS "Using osmium + arnis")
endif()

list(APPEND mapgen_earth_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/hgt.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/http.cpp ${CMAKE_CURRENT_SOURCE_DIR}/world_elevation.cpp
)

if(USE_VOXEL_EARTH)
     include_directories(${TINYGLTF_INCLUDE_DIR})

     add_subdirectory(luanti-earth/native)
     set(FREEMINER_CLIENT_LIBRARIES ${FREEMINER_CLIENT_LIBRARIES} earth_native PARENT_SCOPE)
     set(FREEMINER_SERVER_LIBRARIES ${FREEMINER_SERVER_LIBRARIES} earth_native PARENT_SCOPE)

     message(STATUS "Earth: Using voxel-earth")
endif()

set(freeminer_mapgen_SRCS ${freeminer_mapgen_SRCS} ${mapgen_earth_SRCS} PARENT_SCOPE)
