/*
irrlichttypes.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

/*
 * IrrlichtMt already includes stdint.h in irrTypes.h. This works everywhere
 * we need it to (including recent MSVC), so should be fine here too.
 */
#include <cstdint>

#include "irrTypes.h"
#include "config.h"

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
