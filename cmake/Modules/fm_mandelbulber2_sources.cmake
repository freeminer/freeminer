# Configure Mandelbulber2 sources in the directory that creates the target.
if(NOT ENABLE_MANDELBULBER2 OR NOT MANDELBULBER2_DIR)
    return()
endif()

file(GLOB mb_sources CONFIGURE_DEPENDS
    "${MANDELBULBER2_DIR}/formula/definition/fractal_*.cpp")
list(REMOVE_ITEM mb_sources "${MANDELBULBER2_DIR}/formula/definition/fractal_custom.cpp")
set_source_files_properties(${mb_sources} PROPERTIES INCLUDE_DIRECTORIES "${MANDELBULBER2_DIR}")
list(APPEND mapgen_SRCS ${mb_sources})

# The upstream algebra implementation uses QString only for diagnostic output.
set(mb_algebra "${MANDELBULBER2_DIR}/src/algebra.cpp")
set_source_files_properties(${mb_algebra} PROPERTIES
    COMPILE_DEFINITIONS "QString=MBQString"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2_compat")
list(APPEND mapgen_SRCS "${mb_algebra}")

# Supply the Qt-free parameter container before upstream's quoted includes.
set(mb_fractal "${MANDELBULBER2_DIR}/src/fractal.cpp")
set(mb_parameter_compat "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2_compat/parameters.h")
if(MSVC)
    set(mb_force_include "/FI${mb_parameter_compat}")
else()
    set(mb_force_include "-include${mb_parameter_compat}")
endif()
set_source_files_properties(${mb_fractal} PROPERTIES
    COMPILE_OPTIONS "${mb_force_include}"
    INCLUDE_DIRECTORIES "${MANDELBULBER2_DIR};${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2_compat")
list(APPEND mapgen_SRCS "${mb_fractal}")

# Compile the original parameter initializer with private compatibility types.
set(mb_initializers "${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2_compat/initializers.cpp")
set_source_files_properties(${mb_initializers} PROPERTIES
    INCLUDE_DIRECTORIES "${MANDELBULBER2_DIR};${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2_compat")
list(APPEND mapgen_SRCS "${mb_initializers}")

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2.cpp PROPERTIES
    COMPILE_DEFINITIONS USE_MANDELBULBER2
    INCLUDE_DIRECTORIES "${MANDELBULBER2_DIR};${CMAKE_CURRENT_BINARY_DIR}/mandelbulber2;${CMAKE_CURRENT_SOURCE_DIR}/mapgen/mandelbulber2_compat")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/unittest/test_fm_mandelbulber2.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${MANDELBULBER2_DIR}")
