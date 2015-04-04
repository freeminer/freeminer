/*
util/numeric.h
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

#ifndef UTIL_NUMERIC_HEADER
#define UTIL_NUMERIC_HEADER

#include "../irrlichttypes.h"
#include "../irr_v2d.h"
#include "../irr_v3d.h"
#include "../irr_aabb3d.h"
#include <algorithm>
#include <list>
#include <cmath>
#include <map>
#include <vector>
#include <algorithm>


/*
 * This class permits to cache getFacePosition call results
 * This reduces CPU usage and vector calls
 */
class FacePositionCache
{
public:
	static std::vector<v3s16> getFacePositions(u16 d);
private:
	static void generateFacePosition(u16 d);
	static std::map<u16, std::vector<v3s16> > m_cache;
};

class IndentationRaiser
{
public:
	IndentationRaiser(u16 *indentation)
	{
		m_indentation = indentation;
		(*m_indentation)++;
	}
	~IndentationRaiser()
	{
		(*m_indentation)--;
	}
private:
	u16 *m_indentation;
};

inline s16 getContainerPos(s16 p, s16 d)
{
	return (p>=0 ? p : p-d+1) / d;
}

inline v2s16 getContainerPos(v2s16 p, s16 d)
{
	return v2s16(
		getContainerPos(p.X, d),
		getContainerPos(p.Y, d)
	);
}

inline v3s16 getContainerPos(v3s16 p, s16 d)
{
	return v3s16(
		getContainerPos(p.X, d),
		getContainerPos(p.Y, d),
		getContainerPos(p.Z, d)
	);
}

inline v2s16 getContainerPos(v2s16 p, v2s16 d)
{
	return v2s16(
		getContainerPos(p.X, d.X),
		getContainerPos(p.Y, d.Y)
	);
}

inline v3s16 getContainerPos(v3s16 p, v3s16 d)
{
	return v3s16(
		getContainerPos(p.X, d.X),
		getContainerPos(p.Y, d.Y),
		getContainerPos(p.Z, d.Z)
	);
}

inline void getContainerPosWithOffset(s16 p, s16 d, s16 &container, s16 &offset)
{
	container = (p >= 0 ? p : p - d + 1) / d;
	offset = p & (d - 1);
}

inline void getContainerPosWithOffset(const v2s16 &p, s16 d, v2s16 &container, v2s16 &offset)
{
	getContainerPosWithOffset(p.X, d, container.X, offset.X);
	getContainerPosWithOffset(p.Y, d, container.Y, offset.Y);
}

inline void getContainerPosWithOffset(const v3s16 &p, s16 d, v3s16 &container, v3s16 &offset)
{
	getContainerPosWithOffset(p.X, d, container.X, offset.X);
	getContainerPosWithOffset(p.Y, d, container.Y, offset.Y);
	getContainerPosWithOffset(p.Z, d, container.Z, offset.Z);
}


inline bool isInArea(v3s16 p, s16 d)
{
	return (
		p.X >= 0 && p.X < d &&
		p.Y >= 0 && p.Y < d &&
		p.Z >= 0 && p.Z < d
	);
}

inline bool isInArea(v2s16 p, s16 d)
{
	return (
		p.X >= 0 && p.X < d &&
		p.Y >= 0 && p.Y < d
	);
}

inline bool isInArea(v3s16 p, v3s16 d)
{
	return (
		p.X >= 0 && p.X < d.X &&
		p.Y >= 0 && p.Y < d.Y &&
		p.Z >= 0 && p.Z < d.Z
	);
}

#define rangelim(d, min, max) ((d) < (min) ? (min) : ((d)>(max)?(max):(d)))
#define myfloor(x) ((x) > 0.0 ? (int)(x) : (int)(x) - 1)

inline v3s16 arealim(v3s16 p, s16 d)
{
	if(p.X < 0)
		p.X = 0;
	if(p.Y < 0)
		p.Y = 0;
	if(p.Z < 0)
		p.Z = 0;
	if(p.X > d-1)
		p.X = d-1;
	if(p.Y > d-1)
		p.Y = d-1;
	if(p.Z > d-1)
		p.Z = d-1;
	return p;
}

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))
#define CONTAINS(c, v) (std::find((c).begin(), (c).end(), (v)) != (c).end())

// The naive swap performs better than the xor version
#define SWAP(t, x, y) do { \
	t temp = x;            \
	x = y;                 \
	y = temp;              \
} while (0)

inline void sortBoxVerticies(v3s16 &p1, v3s16 &p2) {
	if (p1.X > p2.X)
		SWAP(s16, p1.X, p2.X);
	if (p1.Y > p2.Y)
		SWAP(s16, p1.Y, p2.Y);
	if (p1.Z > p2.Z)
		SWAP(s16, p1.Z, p2.Z);
}


/** Returns \p f wrapped to the range [-360, 360]
 *
 *  See test.cpp for example cases.
 *
 *  \note This is also used in cases where degrees wrapped to the range [0, 360]
 *  is innapropriate (e.g. pitch needs negative values)
 *
 *  \internal functionally equivalent -- although precision may vary slightly --
 *  to fmodf((f), 360.0f) however empirical tests indicate that this approach is
 *  faster.
 */
inline float modulo360f(float f)
{
	int sign;
	int whole;
	float fraction;

	if (f < 0) {
		f = -f;
		sign = -1;
	} else {
		sign = 1;
	}

	whole = f;

	fraction = f - whole;
	whole %= 360;

	return sign * (whole + fraction);
}


/** Returns \p f wrapped to the range [0, 360]
  */
inline float wrapDegrees_0_360(float f)
{
	float value = modulo360f(f);
	return value < 0 ? value + 360 : value;
}


/** Returns \p f wrapped to the range [-180, 180]
  */
inline float wrapDegrees_180(float f)
{
	float value = modulo360f(f + 180);
	if (value < 0)
		value += 360;
	return value - 180;
}

/*
	Pseudo-random (VC++ rand() sucks)
*/
#define MYRAND_RANGE 0xffffffff
u32 myrand();
void mysrand(unsigned int seed);
void myrand_bytes(void *out, size_t len);
int myrand_range(int min, int max);

/*
	Miscellaneous functions
*/

inline u32 get_bits(u32 x, u32 pos, u32 len)
{
	u32 mask = (1 << len) - 1;
	return (x >> pos) & mask;
}

inline void set_bits(u32 *x, u32 pos, u32 len, u32 val)
{
	u32 mask = (1 << len) - 1;
	*x &= ~(mask << pos);
	*x |= (val & mask) << pos;
}

inline u32 calc_parity(u32 v)
{
	v ^= v >> 16;
	v ^= v >> 8;
	v ^= v >> 4;
	v &= 0xf;
	return (0x6996 >> v) & 1;
}

u64 murmur_hash_64_ua(const void *key, int len, unsigned int seed);

bool isBlockInSight(v3s16 blockpos_b, v3f camera_pos, v3f camera_dir,
		f32 camera_fov, f32 range, f32 *distance_ptr=NULL);

/*
	Some helper stuff
*/
#define MYMIN(a,b) ((a)<(b)?(a):(b))
#define MYMAX(a,b) ((a)>(b)?(a):(b))

/*
	Returns nearest 32-bit integer for given floating point number.
	<cmath> and <math.h> in VC++ don't provide round().
*/
inline s32 myround(f32 f)
{
	return floor(f + 0.5);
}

/*
	Returns integer position of node in given floating point position
*/
inline v3s16 floatToInt(v3f p, f32 d)
{
	v3s16 p2(
		(p.X + (p.X>0 ? d/2 : -d/2))/d,
		(p.Y + (p.Y>0 ? d/2 : -d/2))/d,
		(p.Z + (p.Z>0 ? d/2 : -d/2))/d);
	return p2;
}

/*
	Returns floating point position of node in given integer position
*/
inline v3f intToFloat(v3s16 p, f32 d)
{
	v3f p2(
		(f32)p.X * d,
		(f32)p.Y * d,
		(f32)p.Z * d
	);
	return p2;
}

// Random helper. Usually d=BS
inline core::aabbox3d<f32> getNodeBox(v3s16 p, float d)
{
	return core::aabbox3d<f32>(
		(float)p.X * d - 0.5*d,
		(float)p.Y * d - 0.5*d,
		(float)p.Z * d - 0.5*d,
		(float)p.X * d + 0.5*d,
		(float)p.Y * d + 0.5*d,
		(float)p.Z * d + 0.5*d
	);
}

class IntervalLimiter
{
public:
	IntervalLimiter():
		m_accumulator(0)
	{
	}
	/*
		dtime: time from last call to this method
		wanted_interval: interval wanted
		return value:
			true: action should be skipped
			false: action should be done
	*/
	bool step(float dtime, float wanted_interval)
	{
		m_accumulator += dtime;
		if(m_accumulator < wanted_interval)
			return false;
		m_accumulator -= wanted_interval;
		if (m_accumulator > wanted_interval*2)
			m_accumulator = 0;
		return true;
	}
	void run_next(float wanted_interval) {
		m_accumulator = wanted_interval;
	}
protected:
	float m_accumulator;
};

/*
	Splits a list into "pages". For example, the list [1,2,3,4,5] split
	into two pages would be [1,2,3],[4,5]. This function computes the
	minimum and maximum indices of a single page.

	length: Length of the list that should be split
	page: Page number, 1 <= page <= pagecount
	pagecount: The number of pages, >= 1
	minindex: Receives the minimum index (inclusive).
	maxindex: Receives the maximum index (exclusive).

	Ensures 0 <= minindex <= maxindex <= length.
*/
inline void paging(u32 length, u32 page, u32 pagecount, u32 &minindex, u32 &maxindex)
{
	if(length < 1 || pagecount < 1 || page < 1 || page > pagecount)
	{
		// Special cases or invalid parameters
		minindex = maxindex = 0;
	}
	else if(pagecount <= length)
	{
		// Less pages than entries in the list:
		// Each page contains at least one entry
		minindex = (length * (page-1) + (pagecount-1)) / pagecount;
		maxindex = (length * page + (pagecount-1)) / pagecount;
	}
	else
	{
		// More pages than entries in the list:
		// Make sure the empty pages are at the end
		if(page < length)
		{
			minindex = page-1;
			maxindex = page;
		}
		else
		{
			minindex = 0;
			maxindex = 0;
		}
	}
}

inline float cycle_shift(float value, float by = 0, float max = 1)
{
    if (value + by < 0) return max + by + value;
    if (value + by > max) return value + by - max;
    return value + by;
}

inline int radius_box(const v3s16 & a, const v3s16 & b) {
	return std::max(std::max(std::abs((float)a.X - b.X), std::abs((float)a.Y - b.Y)), std::abs((float)a.Z - b.Z));
}

inline int radius_box(const v3f & a, const v3f & b) {
	return std::max(std::max(std::fabs(a.X - b.X), std::fabs(a.Y - b.Y)), std::fabs(a.Z - b.Z));
}

inline bool is_power_of_two(u32 n)
{
	return n != 0 && (n & (n-1)) == 0;
}

// Compute next-higher power of 2 efficiently, e.g. for power-of-2 texture sizes.
// Public Domain: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
inline u32 npot2(u32 orig) {
	orig--;
	orig |= orig >> 1;
	orig |= orig >> 2;
	orig |= orig >> 4;
	orig |= orig >> 8;
	orig |= orig >> 16;
	return orig + 1;
}

#endif
