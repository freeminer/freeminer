/*
	If CMake is used, includes the cmake-generated cmake_config.h.
	Otherwise use default values
*/

#pragma once


#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)


#if defined USE_CMAKE_CONFIG_H
	#include "cmake_config.h"
<<<<<<< HEAD
#elif defined (__ANDROID__) || defined (ANDROID)
	#define PROJECT_NAME "freeminer"
//	#define PROJECT_NAME_C "freeminer"
	#ifndef STATIC_SHAREDIR
		#define STATIC_SHAREDIR ""
	#endif
	#include "android_version.h"
	#ifdef NDEBUG
=======
#elif defined (__ANDROID__)
	#define PROJECT_NAME "minetest"
	#define PROJECT_NAME_C "Minetest"
	#define STATIC_SHAREDIR ""
	#define VERSION_STRING STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH) STR(VERSION_EXTRA)
#ifdef NDEBUG
>>>>>>> 5.5.0
		#define BUILD_TYPE "Release"
	#else
		#define BUILD_TYPE "Debug"
	#endif
#else
	#ifdef NDEBUG
		#define BUILD_TYPE "Release"
	#else
		#define BUILD_TYPE "Debug"
	#endif
#endif
<<<<<<< HEAD

#define BUILD_INFO "BUILD_TYPE=" BUILD_TYPE \
		" RUN_IN_PLACE=" STR(RUN_IN_PLACE) \
		" USE_GETTEXT=" STR(USE_GETTEXT) \
		" USE_SOUND=" STR(USE_SOUND) \
		" USE_CURL=" STR(USE_CURL) \
		" USE_FREETYPE=" STR(USE_FREETYPE) \
		" USE_LUAJIT=" STR(USE_LUAJIT) \
		" STATIC_SHAREDIR=" STR(STATIC_SHAREDIR)

#ifndef GAMES_VERSION
	#define GAMES_VERSION ""
#endif

#endif
=======
>>>>>>> 5.5.0
