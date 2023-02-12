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

#include <irrTypes.h>
#include "config.h"

using namespace irr;

#define IRRLICHT_VERSION_10000 IRRLICHT_VERSION_MAJOR*10000 + IRRLICHT_VERSION_MINOR * 100 + IRRLICHT_VERSION_REVISION

namespace irr {

#if (IRRLICHT_VERSION_MAJOR == 1 && IRRLICHT_VERSION_MINOR >= 9)
namespace core {
	template <typename T>
	inline T roundingError();

	template <>
	inline s16 roundingError()
	{
		return 0;
	}
}
#endif

}

#define S8_MIN  (-0x7F - 1)
#define S16_MIN (-0x7FFF - 1)
#define S32_MIN (-0x7FFFFFFF - 1)
#define S64_MIN (-0x7FFFFFFFFFFFFFFF - 1)

#define S8_MAX  0x7F
#define S16_MAX 0x7FFF
#define S32_MAX 0x7FFFFFFF
#define S64_MAX 0x7FFFFFFFFFFFFFFF

#define U8_MAX  0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFF

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
