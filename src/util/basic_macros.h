/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef BASICMACROS_HEADER
#define BASICMACROS_HEADER

#include <algorithm>

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

#define MYMIN(a, b) ((a) < (b) ? (a) : (b))

#define MYMAX(a, b) ((a) > (b) ? (a) : (b))

#define CONTAINS(c, v) (std::find((c).begin(), (c).end(), (v)) != (c).end())

// To disable copy constructors and assignment operations for some class
// 'Foobar', add the macro DISABLE_CLASS_COPY(Foobar) as a private member.
// Note this also disables copying for any classes derived from 'Foobar' as well
// as classes having a 'Foobar' member.
#define DISABLE_CLASS_COPY(C) \
	C(const C &);             \
	C &operator=(const C &)

#ifndef _MSC_VER
	#define UNUSED_ATTRIBUTE __attribute__ ((unused))
#else
	#define UNUSED_ATTRIBUTE
#endif

// Fail compilation if condition expr is not met.
// Note that 'msg' must follow the format of a valid identifier, e.g.
// STATIC_ASSERT(sizeof(foobar_t) == 40), foobar_t_is_wrong_size);
#define STATIC_ASSERT(expr, msg) \
	UNUSED_ATTRIBUTE typedef char msg[!!(expr) * 2 - 1]

#endif
