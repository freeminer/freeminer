// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <cstdint>

namespace irr
{

//! 8 bit unsigned variable.
typedef uint8_t u8;

//! 8 bit signed variable.
typedef int8_t s8;

//! 8 bit character variable.
/** This is a typedef for char, it ensures portability of the engine. */
typedef char c8;

//! 16 bit unsigned variable.
typedef uint16_t u16;

//! 16 bit signed variable.
typedef int16_t s16;

//! 32 bit unsigned variable.
typedef uint32_t u32;

//! 32 bit signed variable.
typedef int32_t s32;

//! 64 bit unsigned variable.
typedef uint64_t u64;

//! 64 bit signed variable.
typedef int64_t s64;

//! 32 bit floating point variable.
/** This is a typedef for float, it ensures portability of the engine. */
typedef float f32;

//! 64 bit floating point variable.
/** This is a typedef for double, it ensures portability of the engine. */
typedef double f64;

} // end namespace irr

//! Defines for snprintf_irr because snprintf method does not match the ISO C
//! standard on Windows platforms.
//! We want int snprintf_irr(char *str, size_t size, const char *format, ...);
#if defined(_MSC_VER)
#define snprintf_irr sprintf_s
#else
#define snprintf_irr snprintf
#endif // _MSC_VER

namespace irr
{

//! Type name for character type used by the file system (legacy).
typedef char fschar_t;
#define _IRR_TEXT(X) X

} // end namespace irr

//! define a break macro for debugging.
#if defined(_DEBUG)
#if defined(_IRR_WINDOWS_API_) && defined(_MSC_VER)
#include <crtdbg.h>
#define _IRR_DEBUG_BREAK_IF(_CONDITION_) \
	if (_CONDITION_) {                   \
		_CrtDbgBreak();                  \
	}
#else
#include <assert.h>
#define _IRR_DEBUG_BREAK_IF(_CONDITION_) assert(!(_CONDITION_));
#endif
#else
#define _IRR_DEBUG_BREAK_IF(_CONDITION_)
#endif

//! deprecated macro for virtual function override
/** prefer to use the override keyword for new code */
#define _IRR_OVERRIDE_ override

//! creates four CC codes used in Irrlicht for simple ids
/** some compilers can create those by directly writing the
code like 'code', but some generate warnings so we use this macro here */
#define MAKE_IRR_ID(c0, c1, c2, c3)                             \
	((irr::u32)(irr::u8)(c0) | ((irr::u32)(irr::u8)(c1) << 8) | \
			((irr::u32)(irr::u8)(c2) << 16) | ((irr::u32)(irr::u8)(c3) << 24))
