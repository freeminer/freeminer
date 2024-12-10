/*
noise.h
 * Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>
 * Copyright (C) 2010-2014 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
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

#include <atomic>

#include "constants.h"
#include "irr_v3d.h"
#include "exceptions.h"
#include "util/string.h"

#if defined(RANDOM_MIN)
#undef RANDOM_MIN
#endif
#if defined(RANDOM_MAX)
#undef RANDOM_MAX
#endif

extern FlagDesc flagdesc_noiseparams[];

template <typename type_scale, typename type_coord>
inline type_scale farscale(type_scale scale, type_coord z) {
	return ( 1 + ( 1 - (FARSCALE_LIMIT * 1 - (fmod(fabs(z), FARSCALE_LIMIT))                     ) / (FARSCALE_LIMIT * 1) ) * (scale - 1) );
}

template <typename type_scale, typename type_coord>
inline type_scale farscale(type_scale scale, type_coord x, type_coord z) {
	return ( 1 + ( 1 - (FARSCALE_LIMIT * 2 - fmod(fabs(x) + fabs(z), FARSCALE_LIMIT*2)           ) / (FARSCALE_LIMIT * 2) ) * (scale - 1) );
}

template <typename type_scale, typename type_coord>
inline type_scale farscale(type_scale scale, type_coord x, type_coord y, type_coord z) {
	return ( 1 + ( 1 - (FARSCALE_LIMIT * 3 - fmod(fabs(x) + fabs(y) + fabs(z), FARSCALE_LIMIT*3) ) / (FARSCALE_LIMIT * 3) ) * (scale - 1) );
}

// Note: this class is not polymorphic so that its high level of
// optimizability may be preserved in the common use case
class PseudoRandom {
public:
	const static u32 RANDOM_RANGE = 32767;

	inline PseudoRandom(s32 seed_=0)
	{
		seed(seed_);
	}

	inline void seed(s32 seed)
	{
		m_next = seed;
	}

	inline u32 next()
	{
		m_next = static_cast<u32>(m_next) * 1103515245U + 12345U;
		// Signed division is required due to backwards compatibility
		return static_cast<u32>(m_next / 65536) % (RANDOM_RANGE + 1U);
	}

	inline s32 range(s32 min, s32 max)
	{
		if (max < min)
			throw PrngException("Invalid range (max < min)");
		/*
		Here, we ensure the range is not too large relative to RANDOM_MAX,
		as otherwise the effects of bias would become noticeable.  Unlike
		PcgRandom, we cannot modify this RNG's range as it would change the
		output of this RNG for reverse compatibility.
		*/
		if (static_cast<u32>(max - min) > (RANDOM_RANGE + 1) / 5)
			throw PrngException("Range too large");

		return (next() % (max - min + 1)) + min;
	}

	// Allow save and restore of state
	inline s32 getState() const
	{
		return m_next;
	}
private:
	s32 m_next;
};

class PcgRandom {
public:
	const static s32 RANDOM_MIN   = -0x7fffffff - 1;
	const static s32 RANDOM_MAX   = 0x7fffffff;
	const static u32 RANDOM_RANGE = 0xffffffff;

	PcgRandom(u64 state=0x853c49e6748fea9bULL, u64 seq=0xda3e39cb94b95bdbULL);
	void seed(u64 state, u64 seq=0xda3e39cb94b95bdbULL);
	u32 next();
	u32 range(u32 bound);
	s32 range(s32 min, s32 max);
	void bytes(void *out, size_t len);
	s32 randNormalDist(s32 min, s32 max, int num_trials=6);

	// Allow save and restore of state
	void getState(u64 state[2]) const;
	void setState(const u64 state[2]);
private:
	std::atomic_ullong m_state;
	//u64 m_state;
	u64 m_inc;
};

#define NOISE_FLAG_DEFAULTS    0x01
#define NOISE_FLAG_EASED       0x02
#define NOISE_FLAG_ABSVALUE    0x04

//// TODO(hmmmm): implement these!
#define NOISE_FLAG_POINTBUFFER 0x08
#define NOISE_FLAG_SIMPLEX     0x10

struct NoiseParams {
	float offset = 0.0f;
	float scale = 1.0f;
	v3f spread = v3f(250, 250, 250);
	s32 seed = 12345;
	u16 octaves = 3;
	float persist = 0.6f;
	float lacunarity = 2.0f;
	u32 flags = NOISE_FLAG_DEFAULTS;

// fm:
	opos_t far_scale = 1;
	opos_t far_spread = 1;
	opos_t far_persist = 1;
	opos_t far_lacunarity = 1;

	NoiseParams() = default;

	NoiseParams(float offset_, float scale_, const v3f &spread_, s32 seed_,
		u16 octaves_, float persist_, float lacunarity_,
		u32 flags_=NOISE_FLAG_DEFAULTS,
		float far_scale_ = 1, float far_spread_ = 1, float far_persist_ = 1, float far_lacunarity_ = 1
		)
	{
		offset     = offset_;
		scale      = scale_;
		spread     = spread_;
		seed       = seed_;
		octaves    = octaves_;
		persist    = persist_;
		lacunarity = lacunarity_;
		flags      = flags_;

		far_scale  = far_scale_;
		far_spread = far_spread_;
		far_persist = far_persist_;
		far_lacunarity = far_lacunarity_;
	}

	friend std::ostream & operator<<(std::ostream & os, NoiseParams & np);

};

class Noise {
public:
	NoiseParams np;
	s32 seed;
	u32 sx;
	u32 sy;
	u32 sz;
	float *noise_buf = nullptr;
	float *gradient_buf = nullptr;
	float *persist_buf = nullptr;
	float *result = nullptr;

	Noise(const NoiseParams *np, s32 seed, u32 sx, u32 sy, u32 sz=1);
	~Noise();

	void setSize(u32 sx, u32 sy, u32 sz=1);
	void setSpreadFactor(v3f spread);
	void setOctaves(int octaves);

	void gradientMap2D(
		float x, float y,
		float step_x, float step_y,
		s32 seed);
	void gradientMap3D(
		float x, float y, float z,
		float step_x, float step_y, float step_z,
		s32 seed);

	float *perlinMap2D(float x, float y, float *persistence_map=NULL);
	float *perlinMap3D(float x, float y, float z, float *persistence_map=NULL);

	inline float *perlinMap2D_PO(float x, float xoff, float y, float yoff,
		float *persistence_map=NULL)
	{
		return perlinMap2D(
			x + xoff * np.spread.X * farscale(np.far_spread, x, y),
			y + yoff * np.spread.Y * farscale(np.far_spread, x, y),
			persistence_map);
	}

	inline float *perlinMap3D_PO(float x, float xoff, float y, float yoff,
		float z, float zoff, float *persistence_map=NULL)
	{
		return perlinMap3D(
			x + xoff * np.spread.X * farscale(np.far_spread, x, y, z),
			y + yoff * np.spread.Y * farscale(np.far_spread, x, y, z),
			z + zoff * np.spread.Z * farscale(np.far_spread, x, y, z),
			persistence_map);
	}

private:
	void allocBuffers();
	void resizeNoiseBuf(bool is3d);
	void updateResults(float g, float *gmap, const float *persistence_map,
			size_t bufsize);

};

float NoisePerlin2D(const NoiseParams *np, float x, float y, s32 seed);
float NoisePerlin3D(const NoiseParams *np, float x, float y, float z, s32 seed);

inline float NoisePerlin2D_PO(NoiseParams *np, float x, float xoff,
	float y, float yoff, s32 seed)
{
	return NoisePerlin2D(np,
		x + xoff * np->spread.X,
		y + yoff * np->spread.Y,
		seed);
}

inline float NoisePerlin3D_PO(NoiseParams *np, float x, float xoff,
	float y, float yoff, float z, float zoff, s32 seed)
{
	return NoisePerlin3D(np,
		x + xoff * np->spread.X,
		y + yoff * np->spread.Y,
		z + zoff * np->spread.Z,
		seed);
}

// Return value: -1 ... 1
float noise2d(int x, int y, s32 seed);
float noise3d(int x, int y, int z, s32 seed);

float noise2d_gradient(float x, float y, s32 seed, bool eased=true);
float noise3d_gradient(float x, float y, float z, s32 seed, bool eased=false);

float noise2d_perlin(float x, float y, s32 seed,
		int octaves, float persistence, bool eased=true);

inline float easeCurve(float t)
{
	return t * t * t * (t * (6.f * t - 15.f) + 10.f);
}

float contour(float v);
