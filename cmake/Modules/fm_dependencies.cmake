include(fm_cmake_compat)

# Set the API baseline before configuring dependencies, including Clang/MinGW. Recent Boost.Atomic
# requires WaitOnAddress and uses Windows 10 for its builds.
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# Recompute detected features; never reuse results from an earlier configure.
foreach(feature ICONV)
    unset(USE_${feature} CACHE)
    set(USE_${feature} 0)
endforeach()

if(ANDROID OR WIN32 OR EMSCRIPTEN OR USE_LIBCXX)
    option(FETCH_DEPS "Compile deps (boost,...) in place" 1)
else()
    option(FETCH_DEPS "Compile deps (boost,...) in place" 1)
endif()

option(FETCHCONTENT_UPDATES_DISCONNECTED "Skip dependency updates" ON)
option(FETCHCONTENT_QUIET "Hide dependency download progress" OFF)
include(FetchContent)

find_package(BZip2 QUIET)
if(TARGET BZip2::BZip2)
    message(STATUS "Using system BZip2: ${BZIP2_INCLUDE_DIRS}")
elseif(FETCH_DEPS)
    message(STATUS "System BZip2 not found; using bundled BZip2")
    function(fm_fetch_bzip2)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(ENABLE_LIB_ONLY ON)
        set(ENABLE_TESTS OFF)
        set(ENABLE_STATIC_LIB ON)
        FetchContent_Declare(
            BZip2 GIT_REPOSITORY "https://gitlab.com/bzip2/bzip2.git"
            GIT_TAG 66c46b8c9436613fd81bc5d03f63a61933a4dcc3 ${FM_FETCH_OVERRIDE_FIND_PACKAGE}
            USES_TERMINAL_DOWNLOAD TRUE GIT_PROGRESS TRUE ${FM_FETCH_EXCLUDE_FROM_ALL}
        )
        FetchContent_MakeAvailable(BZip2)
        set(bzip2_SOURCE_DIR "${bzip2_SOURCE_DIR}" PARENT_SCOPE)
    endfunction()
    fm_fetch_bzip2()
    set(BZIP2_FOUND TRUE)
    if(NOT TARGET BZip2::BZip2)
        add_library(BZip2::BZip2 ALIAS bz2_static)
    endif()
    set(BZIP2_INCLUDE_DIR "${bzip2_SOURCE_DIR}")
    target_include_directories(bz2_static PUBLIC ${BZIP2_INCLUDE_DIR})
else()
    message(STATUS "BZip2 not found; set FETCH_DEPS=ON to build bundled BZip2")
endif()

if(FETCH_OPENSSL)
    find_package(OpenSSL 3.0 QUIET)
    if(TARGET OpenSSL::SSL)
        message(STATUS "Using system OpenSSL: ${OPENSSL_VERSION}")
    elseif(FETCH_DEPS)
        message(STATUS "System OpenSSL 3.0 not found; using bundled OpenSSL")
        FetchContent_Declare(
            openssl-cmake
            GIT_REPOSITORY https://github.com/jimmy-park/openssl-cmake
            GIT_TAG 48c3f910074784adab7fe422cb955d71eda8fc4b
            SOURCE_SUBDIR cmake
            GIT_SUBMODULES_RECURSE OFF ${FM_FETCH_OVERRIDE_FIND_PACKAGE}
            USES_TERMINAL_DOWNLOAD TRUE
            GIT_PROGRESS TRUE ${FM_FETCH_EXCLUDE_FROM_ALL}
        )
        FetchContent_MakeAvailable(openssl-cmake)
    else()
        message(STATUS "OpenSSL 3.0 not found; set FETCH_DEPS=ON to build bundled OpenSSL")
    endif()
endif()

find_package(Boost 1.69 QUIET COMPONENTS program_options geometry)
if(TARGET Boost::headers AND TARGET Boost::program_options)
    message(STATUS "Using system Boost: ${Boost_INCLUDE_DIRS}")
elseif(FETCH_DEPS)
    message(STATUS "System Boost with program_options not found; using bundled Boost")
    function(fm_fetch_boost)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(BOOST_ENABLE_CMAKE ON)
        set(BOOST_INCLUDE_LIBRARIES program_options asio thread geometry)

        FetchContent_Declare(
            Boost GIT_REPOSITORY https://github.com/boostorg/boost.git GIT_TAG boost-1.92.0
            GIT_SHALLOW TRUE ${FM_FETCH_OVERRIDE_FIND_PACKAGE} USES_TERMINAL_DOWNLOAD TRUE
            GIT_PROGRESS TRUE ${FM_FETCH_EXCLUDE_FROM_ALL}
        )
        FetchContent_MakeAvailable(Boost)
        set(Boost_FOUND TRUE)
        set(Boost_INCLUDE_DIRS
            "${BOOST_LIBRARY_INCLUDES};${boost_SOURCE_DIR}/libs/geometry/include;${boost_SOURCE_DIR}/libs/numeric/conversion/include"
        )
        message(STATUS "Using fetched Boost: ${Boost_INCLUDE_DIRS}")
        set(boost_SOURCE_DIR "${boost_SOURCE_DIR}" PARENT_SCOPE)
        set(Boost_INCLUDE_DIRS "${Boost_INCLUDE_DIRS}" PARENT_SCOPE)
        set(Boost_FOUND "${Boost_FOUND}" PARENT_SCOPE)
    endfunction()
    fm_fetch_boost()
else()
    message(STATUS "Boost not found; set FETCH_DEPS=ON to build bundled Boost")
endif()

find_package(MsgPack REQUIRED)
target_include_directories(fm_dependencies INTERFACE ${MSGPACK_INCLUDE_DIR})
list(APPEND FREEMINER_COMMON_LIBRARIES ${MSGPACK_LIBRARY})

option(ENABLE_ICONV "Enable UTF-8 conversion via iconv" OFF)
if(ENABLE_ICONV)
    # Use CMake's module, including its libc detection and imported target.
    include("${CMAKE_ROOT}/Modules/FindIconv.cmake")
    if(Iconv_FOUND)
        set(USE_ICONV 1)
        list(APPEND FREEMINER_COMMON_LIBRARIES Iconv::Iconv)
    endif()
endif()
