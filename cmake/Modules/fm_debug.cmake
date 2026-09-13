option(USE_DEBUG_DUMP "Enable Freeminer debug dump helpers" OFF)
if(USE_DEBUG_DUMP)
    target_include_directories(fm_dependencies INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/debug")
    target_compile_options(
        fm_dependencies
        INTERFACE
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/debug/dump.h\">"
    )
    target_compile_definitions(fm_dependencies INTERFACE DUMP_STREAM=actionstream)
endif()
