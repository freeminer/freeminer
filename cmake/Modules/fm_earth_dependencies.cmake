# Recompute detected features; never reuse results from an earlier configure.
foreach(feature TIFF ONNXRUNTIME)
    unset(USE_${feature} CACHE)
    set(USE_${feature} 0)
endforeach()

option(ENABLE_TIFF "Enable tiff (geotiff for mapgen earth)" 1)
set(ENABLE_SYSTEM_TIFF 1 CACHE INTERNAL "")

if(ENABLE_TIFF AND ENABLE_SYSTEM_TIFF)
    find_package(TIFF QUIET)
    find_package(LibLZMA QUIET)
endif()
if(ENABLE_TIFF AND TIFF_FOUND)
    if(TARGET TIFF::TIFF)
        set(TIFF_LIBRARY TIFF::TIFF)
    elseif(TARGET TIFF::tiff)
        set(TIFF_LIBRARY TIFF::tiff)
    else()
        set(TIFF_FOUND FALSE)
    endif()
endif()
if(ENABLE_TIFF AND TIFF_FOUND)
    target_include_directories(fm_dependencies SYSTEM INTERFACE ${TIFF_INCLUDE_DIR})
    message(STATUS "Using system tiff: ${TIFF_INCLUDE_DIR} ${TIFF_LIBRARY}")
    set(USE_TIFF 1)
    list(APPEND FREEMINER_COMMON_LIBRARIES ${TIFF_LIBRARY})
    if(TARGET LibLZMA::LibLZMA)
        list(APPEND FREEMINER_COMMON_LIBRARIES LibLZMA::LibLZMA)
    endif()
elseif(ENABLE_TIFF AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/CMakeLists.txt)
    function(fm_add_tiff)
        # Static glibc libm requires private symbols unavailable in shared libc.
        # Keep libm shared for Linux builds that prefer static dependencies.
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND STATIC_BUILD)
            set(_fm_saved_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
            set(CMAKE_FIND_LIBRARY_SUFFIXES .so)
            unset(CMath_LIBRARY CACHE)
            find_library(CMath_LIBRARY NAMES m REQUIRED)
            set(CMAKE_FIND_LIBRARY_SUFFIXES "${_fm_saved_suffixes}")
            unset(CMath_HAVE_LIBM_POW CACHE)
        endif()
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(tiff-tools OFF)
        set(tiff-tests OFF)
        set(tiff-docs OFF)
        set(jbig OFF)
        set(lerc OFF)
        set(webp OFF)
        add_subdirectory(external/libtiff)
    endfunction()
    fm_add_tiff()
    set(TIFF_LIBRARY TIFF::tiff)
    set(TIFF_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/libtiff/libtiff
                         ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/libtiff
    )
    target_include_directories(fm_dependencies SYSTEM INTERFACE ${TIFF_INCLUDE_DIR})
    message(STATUS "Using bundled tiff: ${TIFF_INCLUDE_DIR} ${TIFF_LIBRARY}")
    set(USE_TIFF 1)
    list(APPEND FREEMINER_COMMON_LIBRARIES ${TIFF_LIBRARY})
endif()

option(ENABLE_ONNXRUNTIME "Enable ONNX Runtime for Terrain Diffusion mapgen" 1)
if(ENABLE_ONNXRUNTIME)
    set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/onnxruntime"
        CACHE PATH "ONNX Runtime source, build, or install root"
    )
    find_path(
        ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h
        HINTS "${ONNXRUNTIME_ROOT}/include" "${ONNXRUNTIME_ROOT}/include/onnxruntime/core/session"
              "${ONNXRUNTIME_ROOT}" PATH_SUFFIXES onnxruntime/core/session
    )
    find_library(
        ONNXRUNTIME_LIBRARY
        NAMES onnxruntime
        HINTS "${ONNXRUNTIME_ROOT}/lib"
              "${ONNXRUNTIME_ROOT}/lib64"
              "${ONNXRUNTIME_ROOT}/build"
              "${ONNXRUNTIME_ROOT}/build/Linux/Debug"
              "${ONNXRUNTIME_ROOT}/build/Linux/Release"
              "${ONNXRUNTIME_ROOT}/build/Linux/RelWithDebInfo"
              "${ONNXRUNTIME_ROOT}/build/Linux/MinSizeRel"
    )
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        target_include_directories(fm_dependencies SYSTEM INTERFACE ${ONNXRUNTIME_INCLUDE_DIR})
        set(USE_ONNXRUNTIME 1)
        list(APPEND FREEMINER_COMMON_LIBRARIES ${ONNXRUNTIME_LIBRARY})
        message(STATUS "Using ONNX Runtime: ${ONNXRUNTIME_INCLUDE_DIR} ${ONNXRUNTIME_LIBRARY}")
    else()
        message(STATUS "ONNX Runtime not found; Terrain Diffusion mapgen ONNX path disabled")
        if(EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime/core/session/onnxruntime_cxx_api.h")
            message(
                STATUS
                    "ONNX Runtime headers found in ${ONNXRUNTIME_ROOT}, but libonnxruntime was not found. Build ONNX Runtime first or set ONNXRUNTIME_LIBRARY."
            )
        endif()
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/json/include/nlohmann/json.hpp")
    add_subdirectory(mapgen/earth/json)
    set(NLOHMANN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/earth/json/include")
    list(APPEND FREEMINER_COMMON_LIBRARIES nlohmann_json::nlohmann_json)
    message(STATUS "Using nlohmann json : ${NLOHMANN_INCLUDE_DIR}")
endif()

include(fm_voxel_earth)
