#pragma once

#if defined USE_CMAKE_CONFIG_H
	#include "cmake_config.h"
#else
	#warning Missing configuration
#endif

// fm:
#ifndef GAMES_VERSION
	#define GAMES_VERSION ""
#endif
