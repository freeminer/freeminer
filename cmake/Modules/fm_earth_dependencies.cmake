# Recompute detected features; never reuse results from an earlier configure.
foreach(feature TIFF ONNXRUNTIME)
    unset(USE_${feature} CACHE)
    set(USE_${feature} 0)
endforeach()

option(ENABLE_TIFF "Enable tiff (geotiff for mapgen earth)" 1)
if(ENABLE_TIFF AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/CMakeLists.txt)
    block(SCOPE_FOR VARIABLES)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(tiff-tools OFF)
        set(tiff-tests OFF)
        set(tiff-docs OFF)
        set(jbig OFF)
        set(lerc OFF)
        set(webp OFF)
        add_subdirectory(external/libtiff)
    endblock()
    set(TIFF_LIBRARY TIFF::tiff)
    set(TIFF_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/libtiff/libtiff
        ${CMAKE_CURRENT_SOURCE_DIR}/external/libtiff/libtiff
    )
    target_include_directories(fm_dependencies SYSTEM INTERFACE ${TIFF_INCLUDE_DIR})
    message(STATUS "Using tiff: ${TIFF_INCLUDE_DIR} ${TIFF_LIBRARY}")
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
