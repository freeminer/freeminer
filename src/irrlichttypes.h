// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "config.h"

#include <cstdint>
#include <irrTypes.h>

using namespace irr;

#define IRRLICHT_VERSION_10000 IRRLICHT_VERSION_MAJOR*10000 + IRRLICHT_VERSION_MINOR * 100 + IRRLICHT_VERSION_REVISION
#define S8_MIN  INT8_MIN
#define S16_MIN INT16_MIN
#define S32_MIN INT32_MIN
#define S64_MIN INT64_MIN

#define S8_MAX  INT8_MAX
#define S16_MAX INT16_MAX
#define S32_MAX INT32_MAX
#define S64_MAX INT64_MAX

#define U8_MAX  UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX



#if USE_POS32

// Node position
using pos_t = irr::s32;

// Block position
using bpos_t = irr::s32;

#else
using pos_t = irr::s16;
using bpos_t = irr::s16;
#endif

#if USE_OPOS64
// Object position
using opos_t = double;
#else
using opos_t = float;
#endif

using block_step_t = uint8_t;
