include(fm_cmake_compat)

add_library(fm_dependencies INTERFACE)

include(fm_dependencies)
include(fm_network)
include(fm_earth_dependencies)
include(fm_osmium)
include(fm_debug)
include(fm_sources)
include(fm_arrow)

# Collect common dependencies only after all feature modules have run.
find_package(PNG REQUIRED)
target_link_libraries(fm_dependencies INTERFACE ${FREEMINER_COMMON_LIBRARIES} PNG::PNG)
# fm: GeoParquet support is used by the Osmium map-generation path only.
if(USE_ARROW AND USE_OSMIUM)
    target_link_libraries(fm_dependencies INTERFACE Parquet::parquet_shared)
    # target_compile_definitions(fm_dependencies INTERFACE USE_ARROW=1)
endif()
# ===
list(APPEND FREEMINER_CLIENT_LIBRARIES fm_dependencies)

list(APPEND FREEMINER_SERVER_LIBRARIES fm_dependencies)

# Mandelbulber's implementation is built into Freeminer.
set(USE_MANDELBULBER 1)
