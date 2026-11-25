option(ENABLE_MANDELBULBER2 "Enable Mandelbulber2 math mapgen formulas" ON)
if(NOT ENABLE_MANDELBULBER2)
    set(MANDELBULBER2_DIR "" PARENT_SCOPE)
    message(STATUS "Mandelbulber2 formulas disabled")
    return()
endif()

# Prefer an explicit FetchContent source override, then the local checkout.
# FETCH_DEPS controls downloads when no source is available.
include(FetchContent)
FetchContent_Declare(mandelbulber2
    GIT_REPOSITORY https://github.com/buddhi1980/mandelbulber2
    GIT_TAG 18e749d04e09d3b12df8efc3d808a507b1e616be
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR build/cmake
    SOURCE_DIR "${DEPS_SOURCE_ROOT}/mandelbulber2"
    BINARY_DIR "${DEPS_BINARY_ROOT}/mandelbulber2"
    ${FM_FETCH_SYSTEM}
    ${FM_FETCH_EXCLUDE_FROM_ALL}
)
set(_mandelbulber2_local "${CMAKE_CURRENT_SOURCE_DIR}/../external/mandelbulber2")
if(FETCHCONTENT_SOURCE_DIR_MANDELBULBER2)
    set(_mandelbulber2_root "${FETCHCONTENT_SOURCE_DIR_MANDELBULBER2}")
elseif(EXISTS "${_mandelbulber2_local}/mandelbulber2/src/fractal.h")
    set(_mandelbulber2_root "${_mandelbulber2_local}")
elseif(FETCH_DEPS)
    FetchContent_GetProperties(mandelbulber2)
    if(NOT mandelbulber2_POPULATED)
        FetchContent_Populate(mandelbulber2)
    endif()
    set(_mandelbulber2_root "${mandelbulber2_SOURCE_DIR}")
endif()
set(MANDELBULBER2_DIR "${_mandelbulber2_root}/mandelbulber2")
if(_mandelbulber2_root AND NOT EXISTS "${MANDELBULBER2_DIR}/src/fractal.h")
    message(FATAL_ERROR "Invalid Mandelbulber2 source directory: ${_mandelbulber2_root}")
endif()
if(IS_DIRECTORY "${MANDELBULBER2_DIR}/formula/definition" AND
    EXISTS "${MANDELBULBER2_DIR}/src/fractal.h")
    set(MANDELBULBER2_DIR "${MANDELBULBER2_DIR}" PARENT_SCOPE)
    message(STATUS "Mandelbulber2 formulas enabled")
else()
    message(STATUS "Mandelbulber2 source not found; formulas disabled")
endif()
