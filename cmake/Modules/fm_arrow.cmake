if(NOT (ANDROID OR WIN32 OR EMSCRIPTEN))
    option(ENABLE_ARROW "Enable bundled Apache Arrow" ON)
endif()

set(USE_ARROW 0)

if(ENABLE_ARROW AND EXISTS "${CMAKE_SOURCE_DIR}/src/external/arrow/cpp/CMakeLists.txt")
    include(ExternalProject)
    set(ARROW_INSTALL_DIR "${CMAKE_BINARY_DIR}/src/external/arrow/install")
    set(ARROW_INCLUDE_DIR "${ARROW_INSTALL_DIR}/include")
    file(MAKE_DIRECTORY "${ARROW_INCLUDE_DIR}")
    set(ARROW_BYPRODUCTS)
    foreach(ARROW_COMPONENT arrow parquet)
        add_library(${ARROW_COMPONENT}_shared SHARED IMPORTED GLOBAL)
        if(WIN32)
            set(ARROW_LOCATION
                "${ARROW_INSTALL_DIR}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${ARROW_COMPONENT}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
            set(ARROW_IMPLIB
                "${ARROW_INSTALL_DIR}/lib/${CMAKE_IMPORT_LIBRARY_PREFIX}${ARROW_COMPONENT}${CMAKE_IMPORT_LIBRARY_SUFFIX}"
            )
            set_target_properties(
                ${ARROW_COMPONENT}_shared PROPERTIES IMPORTED_IMPLIB "${ARROW_IMPLIB}"
            )
            list(APPEND ARROW_BYPRODUCTS "${ARROW_IMPLIB}")
        else()
            set(ARROW_LOCATION
                "${ARROW_INSTALL_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${ARROW_COMPONENT}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
        endif()
        set_target_properties(
            ${ARROW_COMPONENT}_shared
            PROPERTIES IMPORTED_LOCATION "${ARROW_LOCATION}" INTERFACE_INCLUDE_DIRECTORIES
            "${ARROW_INCLUDE_DIR}"
        )
        list(APPEND ARROW_BYPRODUCTS "${ARROW_LOCATION}")
    endforeach()
    add_library(Arrow::arrow_shared ALIAS arrow_shared)
    add_library(Parquet::parquet_shared ALIAS parquet_shared)
    set_target_properties(parquet_shared PROPERTIES INTERFACE_LINK_LIBRARIES Arrow::arrow_shared)
    set(ARROW_CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
        "-DCMAKE_INSTALL_LIBDIR:STRING=lib"
        "-DCMAKE_INSTALL_BINDIR:STRING=bin"
        "-DCMAKE_INSTALL_INCLUDEDIR:STRING=include"
        "-DCMAKE_INSTALL_RPATH:STRING=${ARROW_INSTALL_DIR}/lib"
        "-DARROW_BUILD_SHARED:BOOL=ON"
        "-DARROW_BUILD_STATIC:BOOL=OFF"
        "-DARROW_PARQUET:BOOL=ON"
    )
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND ARROW_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}")
    endif()
    ExternalProject_Add(
        fm_arrow
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/external/arrow/cpp"
        PREFIX "${CMAKE_BINARY_DIR}/src/external/arrow"
        BINARY_DIR "${CMAKE_BINARY_DIR}/src/external/arrow/build"
        INSTALL_DIR "${ARROW_INSTALL_DIR}"
        CMAKE_ARGS ${ARROW_CMAKE_ARGS}
        BUILD_ALWAYS TRUE
    )
    ExternalProject_Add_Step(
        fm_arrow installed_libraries DEPENDEES install BYPRODUCTS ${ARROW_BYPRODUCTS}
    )
    add_dependencies(arrow_shared fm_arrow)
    add_dependencies(parquet_shared fm_arrow)
    set(USE_ARROW 1)
    message(STATUS "Using Arrow ${USE_ARROW}")
endif()
