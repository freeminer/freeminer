// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <cstdint>
#include <irrTypes.h>
#include "config.h"

using namespace irr;

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
#define POS_MIN INT32_MIN
#define POS_MAX INT32_MAX

// Node position
using pos_t = irr::s32;

// Block position
using bpos_t = irr::s32;

#else
#define POS_MIN INT16_MIN
#define POS_MAX INT16_MAX
using pos_t = irr::s16;
using bpos_t = irr::s16;
#endif

#if USE_OPOS64
// Object position
using opos_t = double;
#else
using opos_t = float;
#endif
