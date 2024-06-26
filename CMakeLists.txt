cmake_minimum_required(VERSION 3.5)

# Set policies up to 3.9 since we want to enable the IPO option
if(${CMAKE_VERSION} VERSION_LESS 3.9)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
	cmake_policy(VERSION 3.9)
endif()

# This can be read from ${PROJECT_NAME} after project() is called
project(freeminer)
set(PROJECT_NAME_CAPITALIZED "freeminer")
set(VERSION_TWEAK 0) # freeminer

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(GCC_MINIMUM_VERSION "7.5")
set(CLANG_MINIMUM_VERSION "7.0.1")

# You should not need to edit these manually, use util/bump_version.sh
set(VERSION_MAJOR 5)
set(VERSION_MINOR 8)
set(VERSION_PATCH 0)
set(VERSION_EXTRA "" CACHE STRING "Stuff to append to version string")

# Change to false for releases
set(DEVELOPMENT_BUILD TRUE)

set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}")
if(VERSION_EXTRA)
	set(VERSION_STRING "${VERSION_STRING}-${VERSION_EXTRA}")
elseif(DEVELOPMENT_BUILD)
	set(VERSION_STRING "${VERSION_STRING}-dev")
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Append "-debug" to version string
	set(VERSION_STRING "${VERSION_STRING}-debug")
endif()

# Configuration options
set(BUILD_CLIENT TRUE CACHE BOOL "Build client")
set(BUILD_SERVER FALSE CACHE BOOL "Build server")
set(BUILD_UNITTESTS TRUE CACHE BOOL "Build unittests")
set(BUILD_BENCHMARKS FALSE CACHE BOOL "Build benchmarks")
set(BUILD_DOCUMENTATION TRUE CACHE BOOL "Build documentation")

set(DEFAULT_RUN_IN_PLACE FALSE)
set(DEFAULT_RUN_IN_PLACE TRUE)
if(WIN32)
	set(DEFAULT_RUN_IN_PLACE TRUE)
endif()
set(RUN_IN_PLACE ${DEFAULT_RUN_IN_PLACE} CACHE BOOL
	"Run directly in source directory structure")



# == freeminer:

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

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

#============

message(STATUS "*** Will build version ${VERSION_STRING} ***")
message(STATUS "BUILD_CLIENT: " ${BUILD_CLIENT})
message(STATUS "BUILD_SERVER: " ${BUILD_SERVER})
message(STATUS "BUILD_UNITTESTS: " ${BUILD_UNITTESTS})
message(STATUS "BUILD_BENCHMARKS: " ${BUILD_BENCHMARKS})
message(STATUS "BUILD_DOCUMENTATION: " ${BUILD_DOCUMENTATION})
message(STATUS "RUN_IN_PLACE: " ${RUN_IN_PLACE})

set(WARN_ALL TRUE CACHE BOOL "Enable -Wall for Release build")
if(NOT CMAKE_BUILD_TYPE)
	# Default to release
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type: Debug or Release" FORCE)
endif()


MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(NOT NO_LTO AND NOT DEVELOPMENT_BUILD AND(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
	option(USE_LTO "Use LTO" ON)
else()
	option(USE_LTO "Use LTO" OFF)
endif()

if (USE_LTO)
    # include(CheckIPOSupported)
	# check_ipo_supported(RESULT supported OUTPUT error)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	message(STATUS "Using lto")
endif()


set(ENABLE_UPDATE_CHECKER (NOT ${DEVELOPMENT_BUILD}) CACHE BOOL
	"Whether to enable update checks by default")

# Included stuff
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Load default options for Android
if(ANDROID)
	cmake_minimum_required(VERSION 3.20)
	include(MinetestAndroidLibs)
endif()


set(IRRLICHTMT_BUILD_DIR "" CACHE PATH "Path to IrrlichtMt build directory.")
if(NOT NO_BUILD)
if(ANDROID)
	# currently manually provided
elseif(NOT "${IRRLICHTMT_BUILD_DIR}" STREQUAL "")
	find_package(IrrlichtMt QUIET
		PATHS "${IRRLICHTMT_BUILD_DIR}"
		NO_DEFAULT_PATH
	)

	if(NOT TARGET IrrlichtMt::IrrlichtMt)
		# find_package() searches certain subdirectories. ${PATH}/cmake is not
		# the only one, but it is the one where IrrlichtMt is supposed to export
		# IrrlichtMtConfig.cmake
		message(FATAL_ERROR "Could not find IrrlichtMtConfig.cmake in ${IRRLICHTMT_BUILD_DIR}/cmake.")
	endif()
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/irrlichtmt")
	message(STATUS "Using user-provided IrrlichtMt at subdirectory 'lib/irrlichtmt'")
	if(BUILD_CLIENT)
		# tell IrrlichtMt to create a static library
		#set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared library" FORCE)
		add_subdirectory(lib/irrlichtmt EXCLUDE_FROM_ALL)
		#unset(BUILD_SHARED_LIBS CACHE)

		if(NOT TARGET IrrlichtMt)
			message(FATAL_ERROR "IrrlichtMt project is missing a CMake target?!")
		endif()
	else()
		add_library(IrrlichtMt::IrrlichtMt INTERFACE IMPORTED)
		set_target_properties(IrrlichtMt::IrrlichtMt PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/irrlichtmt/include")
	endif()
else()
	find_package(IrrlichtMt QUIET)
	if(NOT TARGET IrrlichtMt::IrrlichtMt)
		string(CONCAT explanation_msg
			"The Minetest team has forked Irrlicht to make their own customizations. "
			"It can be found here: https://github.com/minetest/irrlicht\n"
			"For example use: git clone --depth=1 https://github.com/minetest/irrlicht lib/irrlichtmt\n")
		if(BUILD_CLIENT)
			message(FATAL_ERROR "IrrlichtMt is required to build the client, but it was not found.\n${explanation_msg}")
		endif()

		include(MinetestFindIrrlichtHeaders)
		if(NOT IRRLICHT_INCLUDE_DIR)
			message(FATAL_ERROR "IrrlichtMt headers are required to build the server, but none found.\n${explanation_msg}")
		endif()
		message(STATUS "Found IrrlichtMt headers: ${IRRLICHT_INCLUDE_DIR}")
		add_library(IrrlichtMt::IrrlichtMt INTERFACE IMPORTED)
		# Note that we can't use target_include_directories() since that doesn't work for IMPORTED targets before CMake 3.11
		set_target_properties(IrrlichtMt::IrrlichtMt PROPERTIES
			INTERFACE_INCLUDE_DIRECTORIES "${IRRLICHT_INCLUDE_DIR}")
	endif()
endif()

if(ANDROID)
	# skipped for now
elseif(BUILD_CLIENT AND TARGET IrrlichtMt::IrrlichtMt)
	# retrieve version somehow
	if(NOT IrrlichtMt_VERSION)
		get_target_property(IrrlichtMt_VERSION IrrlichtMt VERSION)
	endif()
	message(STATUS "Found IrrlichtMt ${IrrlichtMt_VERSION}")

	set(TARGET_VER_S 1.9.0mt13)
	string(REPLACE "mt" "." TARGET_VER ${TARGET_VER_S})
	if(IrrlichtMt_VERSION VERSION_LESS ${TARGET_VER})
		message(FATAL_ERROR "At least IrrlichtMt ${TARGET_VER_S} is required to build")
	elseif(NOT DEVELOPMENT_BUILD AND IrrlichtMt_VERSION VERSION_GREATER ${TARGET_VER})
		message(FATAL_ERROR "IrrlichtMt ${TARGET_VER_S} is required to build")
	endif()
endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${GCC_MINIMUM_VERSION}")
		message(FATAL_ERROR "Insufficient gcc version, found ${CMAKE_CXX_COMPILER_VERSION}. "
			"Version ${GCC_MINIMUM_VERSION} or higher is required.")
	endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${CLANG_MINIMUM_VERSION}")
		message(FATAL_ERROR "Insufficient clang version, found ${CMAKE_CXX_COMPILER_VERSION}. "
			"Version ${CLANG_MINIMUM_VERSION} or higher is required.")
	endif()
endif()

# Installation

if(WIN32)
	set(SHAREDIR ".")
	set(BINDIR "bin")
	set(DOCDIR "doc")
	set(EXAMPLE_CONF_DIR ".")
	set(LOCALEDIR "locale")
elseif(APPLE)
	set(BUNDLE_NAME ${PROJECT_NAME}.app)
	set(BUNDLE_PATH "${BUNDLE_NAME}")
	set(BINDIR ${BUNDLE_NAME}/Contents/MacOS)
	set(SHAREDIR ${BUNDLE_NAME}/Contents/Resources)
	set(DOCDIR "${SHAREDIR}/${PROJECT_NAME}")
	set(EXAMPLE_CONF_DIR ${DOCDIR})
	set(LOCALEDIR "${SHAREDIR}/locale")
elseif(UNIX) # Linux, BSD etc
	if(RUN_IN_PLACE)
		set(SHAREDIR ".")
		set(BINDIR "bin")
		set(DOCDIR "doc")
		set(EXAMPLE_CONF_DIR ".")
		set(MANDIR "unix/man")
		set(XDG_APPS_DIR "unix/applications")
		set(APPDATADIR "unix/metainfo")
		set(ICONDIR "unix/icons")
		set(LOCALEDIR "locale")
	else()
		include(GNUInstallDirs)
		set(SHAREDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
		set(BINDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
		set(DOCDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DOCDIR}")
		set(MANDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}")
		set(EXAMPLE_CONF_DIR ${DOCDIR})
		set(XDG_APPS_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/applications")
		set(APPDATADIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/metainfo")
		set(ICONDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/icons")
		set(LOCALEDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALEDIR}")
	endif()
endif()

set(CUSTOM_SHAREDIR "" CACHE STRING "Directory to install data files into")
if(NOT CUSTOM_SHAREDIR STREQUAL "")
	set(SHAREDIR "${CUSTOM_SHAREDIR}")
	message(STATUS "Using SHAREDIR=${SHAREDIR}")
endif()

set(CUSTOM_BINDIR "" CACHE STRING "Directory to install binaries into")
if(NOT CUSTOM_BINDIR STREQUAL "")
	set(BINDIR "${CUSTOM_BINDIR}")
	message(STATUS "Using BINDIR=${BINDIR}")
endif()

set(CUSTOM_DOCDIR "" CACHE STRING "Directory to install documentation into")
if(NOT CUSTOM_DOCDIR STREQUAL "")
	set(DOCDIR "${CUSTOM_DOCDIR}")
	if(NOT RUN_IN_PLACE)
		set(EXAMPLE_CONF_DIR ${DOCDIR})
	endif()
	message(STATUS "Using DOCDIR=${DOCDIR}")
endif()

set(CUSTOM_MANDIR "" CACHE STRING "Directory to install manpages into")
if(NOT CUSTOM_MANDIR STREQUAL "")
	set(MANDIR "${CUSTOM_MANDIR}")
	message(STATUS "Using MANDIR=${MANDIR}")
endif()

set(CUSTOM_EXAMPLE_CONF_DIR "" CACHE STRING "Directory to install example config file into")
if(NOT CUSTOM_EXAMPLE_CONF_DIR STREQUAL "")
	set(EXAMPLE_CONF_DIR "${CUSTOM_EXAMPLE_CONF_DIR}")
	message(STATUS "Using EXAMPLE_CONF_DIR=${EXAMPLE_CONF_DIR}")
endif()

set(CUSTOM_XDG_APPS_DIR "" CACHE STRING "Directory to install .desktop files into")
if(NOT CUSTOM_XDG_APPS_DIR STREQUAL "")
	set(XDG_APPS_DIR "${CUSTOM_XDG_APPS_DIR}")
	message(STATUS "Using XDG_APPS_DIR=${XDG_APPS_DIR}")
endif()

set(CUSTOM_ICONDIR "" CACHE STRING "Directory to install icons into")
if(NOT CUSTOM_ICONDIR STREQUAL "")
	set(ICONDIR "${CUSTOM_ICONDIR}")
	message(STATUS "Using ICONDIR=${ICONDIR}")
endif()

set(CUSTOM_LOCALEDIR "" CACHE STRING "Directory to install l10n files into")
if(NOT CUSTOM_LOCALEDIR STREQUAL "")
	set(LOCALEDIR "${CUSTOM_LOCALEDIR}")
	message(STATUS "Using LOCALEDIR=${LOCALEDIR}")
endif()


install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/builtin" DESTINATION "${SHAREDIR}")
if(RUN_IN_PLACE)
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/mods/mods_here.txt" DESTINATION "${SHAREDIR}/mods")
	install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/textures/texture_packs_here.txt" DESTINATION "${SHAREDIR}/textures")
endif()

set(INSTALL_DEVTEST FALSE CACHE BOOL "Install Development Test")

if(INSTALL_DEVTEST)
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/games/devtest" DESTINATION "${SHAREDIR}/games/"
		PATTERN ".git*" EXCLUDE )
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/games/default" DESTINATION "${SHAREDIR}/games/"
	COMPONENT "SUBGAME_DEFAULT" OPTIONAL PATTERN ".git*" EXCLUDE )


if(BUILD_CLIENT)
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/client/shaders" DESTINATION "${SHAREDIR}/client")
	install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/textures/base/pack" DESTINATION "${SHAREDIR}/textures/base")
	if(RUN_IN_PLACE)
		install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/clientmods" DESTINATION "${SHAREDIR}")
		#install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/client/serverlist" DESTINATION "${SHAREDIR}/client")
	endif()
endif()

install(FILES "README.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/lua_api.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/client_lua_api.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/menu_lua_api.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/texture_packs.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "doc/world_format.md" DESTINATION "${DOCDIR}" COMPONENT "Docs")
install(FILES "freeminer.conf.example" DESTINATION "${EXAMPLE_CONF_DIR}")

if(UNIX AND NOT APPLE)
	install(FILES "doc/freeminer.6" "doc/freeminerserver.6" DESTINATION "${MANDIR}/man6")
	install(FILES "misc/org.freeminer.freeminer.desktop" DESTINATION "${XDG_APPS_DIR}")
	install(FILES "misc/org.freeminer.freeminer.appdata.xml" DESTINATION "${APPDATADIR}")
	install(FILES "misc/freeminer.svg" DESTINATION "${ICONDIR}/hicolor/scalable/apps")
	install(FILES "misc/freeminer-xorg-icon-128.png"
		DESTINATION "${ICONDIR}/hicolor/128x128/apps"
		RENAME "freeminer.png")
endif()

if(APPLE)
	install(FILES "misc/freeminer-icon.icns" DESTINATION "${SHAREDIR}")
	install(FILES "misc/Info.plist" DESTINATION "${BUNDLE_PATH}/Contents")
endif()

# Library pack
find_package(GMP REQUIRED)
find_package(Json REQUIRED)
find_package(Lua REQUIRED)
if(NOT USE_LUAJIT)
	add_subdirectory(lib/bitop)
endif()

if(BUILD_BENCHMARKS)
	add_subdirectory(lib/catch2)
endif()

# Subdirectories
# Be sure to add all relevant definitions above this
add_subdirectory(src)

# CPack

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A free open-source voxel game engine with easy modding and game creation.")
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_VERSION_TWEAK ${VERSION_TWEAK})
set(CPACK_PACKAGE_VENDOR "celeron55")
set(CPACK_PACKAGE_CONTACT "Perttu Ahola <celeron55@gmail.com>")

include(CPackComponent)

cpack_add_component(Docs
	DISPLAY_NAME "Documentation"
	DESCRIPTION "Documentation about freeminer and freeminer modding"
)

if(WIN32)
	# For some reason these aren't copied otherwise
	# NOTE: For some reason now it seems to work without these
	#if(BUILD_CLIENT)
	#	install(FILES bin/freeminer.exe DESTINATION bin)
	#endif()
	#if(BUILD_SERVER)
	#	install(FILES bin/freeminerserver.exe DESTINATION bin)
	#endif()

	execute_process(COMMAND git describe --always --tag --dirty
		WORKING_DIRECTORY "${GENERATE_VERSION_SOURCE_DIR}"
		OUTPUT_VARIABLE VERSION_GITHASH OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)

	if(VERSION_GITHASH)
		set(PROJECT_VERSION ${VERSION_GITHASH})
	else()
		set(PROJECT_VERSION ${VERSION_STRING})
	endif()

	# Include all dynamically linked runtime libraries such as MSVCRxxx.dll
	include(InstallRequiredSystemLibraries)

	if(RUN_IN_PLACE)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
				set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win64")
			else()
				set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win32")
			endif()

			set(CPACK_GENERATOR ZIP)

		# This might be needed for some installer
		#set(CPACK_PACKAGE_EXECUTABLES bin/freeminer.exe "Freeminer" bin/freeminerserver.exe "Freeminer Server")
	else()
		set(CPACK_GENERATOR WIX)
		set(CPACK_PACKAGE_NAME "${PROJECT_NAME_CAPITALIZED}")
		set(CPACK_PACKAGE_INSTALL_DIRECTORY ".")
		set(CPACK_PACKAGE_EXECUTABLES ${PROJECT_NAME} "${PROJECT_NAME_CAPITALIZED}")
		set(CPACK_CREATE_DESKTOP_LINKS ${PROJECT_NAME})
		set(CPACK_PACKAGING_INSTALL_PREFIX "/${PROJECT_NAME_CAPITALIZED}")

		set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/misc/freeminer-icon.ico")
		# Supported languages can be found at
		# http://wixtoolset.org/documentation/manual/v3/wixui/wixui_localization.html
		#set(CPACK_WIX_CULTURES "ar-SA,bg-BG,ca-ES,hr-HR,cs-CZ,da-DK,nl-NL,en-US,et-EE,fi-FI,fr-FR,de-DE")
		set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/misc/CPACK_WIX_UI_BANNER.BMP")
		set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/misc/CPACK_WIX_UI_DIALOG.BMP")

		set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/doc/lgpl-2.1.txt")

		# The correct way would be to include both x32 and x64 into one installer
		# and install the appropriate one.
		# CMake does not support that, so there are two separate GUID's
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(CPACK_WIX_UPGRADE_GUID "745A0FB3-5552-44CA-A587-A91C397CCC56")
		else()
			set(CPACK_WIX_UPGRADE_GUID "814A2E2D-2779-4BBD-9ACD-FC3BD51FBBA2")
		endif()
	endif()
elseif(APPLE)
	set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-osx")
	set(CPACK_GENERATOR ZIP)
else()
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-linux")
	set(CPACK_GENERATOR TGZ)
	set(CPACK_SOURCE_GENERATOR TGZ)
endif()

include(CPack)


# Add a target to generate API documentation with Doxygen
if(BUILD_DOCUMENTATION)
	find_package(Doxygen)
	if(DOXYGEN_FOUND)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
				${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile @ONLY)
		add_custom_target(doc
			${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc
			COMMENT "Generating API documentation with Doxygen" VERBATIM
		)
	endif()
endif()
