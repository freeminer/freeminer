# == freeminer:
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
	set(ENV{CCACHE_SLOPPINESS} pch_defines,time_macros)
endif(CCACHE_FOUND)

set(BUILD_CLIENT TRUE CACHE BOOL "Build client")
if(WIN32 OR APPLE)
	# win32 broken! http://forum.freeminer.org/threads/building-errors.182/#post-1852
	#if( ${CMAKE_SYSTEM_VERSION} VERSION_LESS 6.0 )
	#	MESSAGE(FATAL_ERROR "Building is not supported for Windows ${CMAKE_SYSTEM_VERSION}")
	#endif()
	set(BUILD_SERVER FALSE CACHE BOOL "Build server")
else()
	set(BUILD_SERVER TRUE CACHE BOOL "Build server")
endif()

if(${CMAKE_VERSION} VERSION_LESS 3.13)
	function(add_link_options)
		foreach(arg IN LISTS ARGN)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${arg}")
			set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${arg}")
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${arg}")
			set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${arg}")
		endforeach()
	endfunction()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	include (CheckCXXSourceCompiles)
	include (CMakePushCheckState)

	set (TEST_FLAG "-fuse-ld=lld")
	set (CMAKE_REQUIRED_FLAGS ${TEST_FLAG})
	check_cxx_source_compiles(" int main() { return 0; } " HAVE_LLD)
	set (CMAKE_REQUIRED_FLAGS "")
	if (HAVE_LLD)
		add_link_options(-fuse-ld=lld)
	endif ()
endif()

if(USE_LIBCXX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	find_library (LIBCXX_LIBRARY c++)
	find_library (LIBCXXABI_LIBRARY c++abi)
	if (LIBCXX_LIBRARY AND LIBCXXABI_LIBRARY)
		add_link_options(-stdlib=libc++)
		add_compile_options(-stdlib=libc++)
		message(STATUS "Using libc++ and lld linker: ${LIBCXX_LIBRARY}, ${LIBCXXABI_LIBRARY}")
	endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_MEMORY OR SANITIZE_UNDEFINED)
		set(ENABLE_SYSTEM_JSONCPP 0 CACHE BOOL "")
endif()

option (USE_STATIC_LIBRARIES "Set to FALSE to use shared libraries" ON)
if (USE_STATIC_LIBRARIES)
    set(BUILD_SHARED_LIBS OFF)
endif()
# ===






# === fm ===

INCLUDE(CheckCXXSourceRuns)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#set(CMAKE_CXX_STANDARD 20)

set(HAVE_SHARED_MUTEX 1)
set(HAVE_THREAD_LOCAL 1)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
	set(HAVE_FUTURE 1)

set(CMAKE_REQUIRED_FLAGS "")

option(ENABLE_THREADS "Use more threads (might be slower on 1-2 core machines)" 1)

if(ENABLE_THREADS)
	set(ENABLE_THREADS 1)
else()
	set(ENABLE_THREADS 0)
endif()

option(MINETEST_PROTO "Use minetest protocol (Slow and buggy)" 1)
if(MINETEST_PROTO)
	set(MINETEST_TRANSPORT 1 CACHE BOOL "")
	message(STATUS "Using minetest compatible protocol (some features missing)")
endif()
if (MINETEST_TRANSPORT)
	message(STATUS "Using minetest compatible transport (slow)")
endif()

#
# Set some optimizations and tweaks
#

include(CheckCXXCompilerFlag)

if(STATIC_BUILD)
	set(STATIC_BUILD 1)
else()
	set(STATIC_BUILD 0)
endif()

if(NOT MSVC)
	# set(WARNING_FLAGS "${WARNING_FLAGS} -Wno-inconsistent-missing-override")

	if("${CMAKE_GENERATOR}" STREQUAL "Ninja" )
		if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
			set(OTHER_FLAGS "${OTHER_FLAGS}  -fdiagnostics-color")
	        elseif ( CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.8 )
			set(OTHER_FLAGS "${OTHER_FLAGS}  -fdiagnostics-color=always")
		endif ()
	endif ()

	if(SANITIZE_ADDRESS)
                message(STATUS "Build with sanitize=address")
		set(OTHER_FLAGS "${OTHER_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(ENABLE_SYSTEM_JSONCPP 0)
	endif()
	if(SANITIZE_THREAD)
                message(STATUS "Build with sanitize=thread")
		set(OTHER_FLAGS "${OTHER_FLAGS} -fsanitize=thread -fPIE -fno-omit-frame-pointer -fno-optimize-sibling-calls")
		if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
			set(OTHER_FLAGS "${OTHER_FLAGS} -pie")
		endif()
        set(ENABLE_SYSTEM_JSONCPP 0)
	endif()
	if(SANITIZE_MEMORY)
                message(STATUS "Build with sanitize=memory")
		set(OTHER_FLAGS "${OTHER_FLAGS} -fsanitize=memory -fPIE -fno-omit-frame-pointer -fno-optimize-sibling-calls")
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
        set(ENABLE_SYSTEM_JSONCPP 0)
	endif()
	if(SANITIZE_UNDEFINED)
                message(STATUS "Build with sanitize=undefined ")
		set(OTHER_FLAGS "${OTHER_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(ENABLE_SYSTEM_JSONCPP 0)
	endif()

	if (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_MEMORY OR SANITIZE_UNDEFINED))
		option(ENABLE_TCMALLOC "Enable tcmalloc" 1)
	endif()

	if(ENABLE_GPERF OR ENABLE_TCMALLOC)
		#set(CMAKE_POSITION_INDEPENDENT_CODE ON)

		if (USE_STATIC_LIBRARIES AND TCMALLOC_STATIC)
			list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
		endif()

		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			find_library(TCMALLOC_LIBRARY NAMES tcmalloc_debug tcmalloc)
		else()
			find_library(TCMALLOC_LIBRARY NAMES tcmalloc tcmalloc_and_profiler)
		endif()

		if(ENABLE_GPERF)
			find_library(PROFILER_LIBRARY NAMES profiler tcmalloc_and_profiler)
		endif()

		if (USE_STATIC_LIBRARIES AND TCMALLOC_STATIC)
			list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
		endif()

		if (PROFILER_LIBRARY)
			# set(PLATFORM_LIBS ${PLATFORM_LIBS} -Wl,--no-as-needed ${PROFILER_LIBRARY} -Wl,--as-needed)
			set(PLATFORM_LIBS ${PLATFORM_LIBS} ${PROFILER_LIBRARY})
		endif()

		if (TCMALLOC_LIBRARY)
			set(PLATFORM_LIBS ${PLATFORM_LIBS} ${TCMALLOC_LIBRARY})
			set(OTHER_FLAGS "${OTHER_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
		endif()

		message(STATUS "Build with tcmalloc ${TCMALLOC_LIBRARY} ${PROFILER_LIBRARY}")
	endif()

    # too noisy
	option(ENABLE_UNWIND "Enable unwind" 1)
	if(ENABLE_UNWIND)
	#if(USE_STATIC_LIBRARIES AND UNWIND_STATIC)
	#	list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
	#endif()
	#find_path(
	#    UNWIND_INCLUDE_DIR
	#    NAMES unwind.h libunwind.h
	#    DOC "unwind include directory"
	#)
	#find_library(UNWIND_LIBRARY NAMES unwind)
    #
	#if(USE_STATIC_LIBRARIES AND UNWIND_STATIC)
	#	list(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
	#endif()
    ##set(UNWIND_LIBRARY unwind::unwind )
	#find_package_handle_standard_args(
	#    UNWIND
	#    REQUIRED_VARS UNWIND_INCLUDE_DIR UNWIND_LIBRARY UNWIND_PLATFORM_LIBRARY
	#    VERSION_VAR UNWIND_VERSION
	#)
    #if(UNWIND_FOUND)
    #    set(UNWIND_INCLUDE_DIRS ${UNWIND_INCLUDE_DIR})
    #    if(NOT TARGET Unwind::unwind)
    #        add_library(Unwind::unwind UNKNOWN IMPORTED)
    #        set_target_properties(Unwind::unwind PROPERTIES
    #                IMPORTED_LOCATION "${_UNWIND_library_generic}"
    #                INTERFACE_LINK_LIBRARIES "${_UNWIND_library_target}"
    #                INTERFACE_INCLUDE_DIRECTORIES "${UNWIND_INCLUDE_DIR}"
    #        )
    #    endif()
	#	message(STATUS "Build with unwind ${Backtrace_LIBRARIES} : ${Backtrace_INCLUDE_DIRS} : ${Backtrace_HEADER}")
    #endif()
    #
	#if(UNWIND_FOUND)
    #    CHECK_CXX_SOURCE_RUNS("
    #   #include <${Backtrace_HEADER}> // for backtrace
    #   int main(int argc, char *argv[]) {
    #        void *callstack[128];
    #        const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    #        char buf[1024];
    #        int nFrames = backtrace(callstack, nMaxFrames);
    #        return 0;
    #   }
    #   "
    #    USE_UNWIND)
    #    if(NOT USE_UNWIND)
    #        set(USE_UNWIND 0)
    #    endif()
    #    # set(USE_UNWIND 1)
	#	#SET(PLATFORM_LIBS ${PLATFORM_LIBS} ${Backtrace_LIBRARIES})
	#	#message(STATUS "Build with unwind ${Backtrace_LIBRARIES} : ${Backtrace_INCLUDE_DIRS} : ${Backtrace_HEADER}")
	#	include_directories(SYSTEM ${Backtrace_INCLUDE_DIRS})
	#endif()
    find_package(Unwind)
		if (Unwind_FOUND)
			SET(PLATFORM_LIBS ${PLATFORM_LIBS} Unwind::unwind)
		endif()
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "(BSD|DragonFly)") # Darwin|
		SET(PLATFORM_LIBS ${PLATFORM_LIBS} execinfo)
	endif()

	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OTHER_FLAGS}")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${OTHER_FLAGS}")
endif()

# === fm === ^^^
