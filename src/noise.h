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

#ifndef NOISE_HEADER
#define NOISE_HEADER

#include <atomic>

#include "irr_v3d.h"
#include "exceptions.h"
#include "util/string.h"

extern FlagDesc flagdesc_noiseparams[];

float farscale(float scale, float z);
float farscale(float scale, float x, float z);
float farscale(float scale, float x, float y, float z);

// Note: this class is not polymorphic so that its high level of
// optimizability may be preserved in the common use case
class PseudoRandom {
public:
	const static u32 RANDOM_RANGE = 32767;

	inline PseudoRandom(int seed=0):
		m_next(seed)
	{
	}

	inline void seed(int seed)
	{
		m_next = seed;
	}

	inline int next()
	{
		m_next = m_next * 1103515245 + 12345;
		return (unsigned)(m_next / 65536) % (RANDOM_RANGE + 1);
	}

	inline int range(int min, int max)
	{
		if (max < min)
			throw PrngException("Invalid range (max < min)");
		/*
		Here, we ensure the range is not too large relative to RANDOM_MAX,
		as otherwise the effects of bias would become noticable.  Unlike
		PcgRandom, we cannot modify this RNG's range as it would change the
		output of this RNG for reverse compatibility.
		*/
		if ((u32)(max - min) > (RANDOM_RANGE + 1) / 10)
			throw PrngException("Range too large");

		return (next() % (max - min + 1)) + min;
	}

private:
	int m_next;
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

private:
	std::atomic_ullong m_state;
	u64 m_inc;
};

#define NOISE_FLAG_DEFAULTS    0x01
#define NOISE_FLAG_EASED       0x02
#define NOISE_FLAG_ABSVALUE    0x04

//// TODO(hmmmm): implement these!
#define NOISE_FLAG_POINTBUFFER 0x08
#define NOISE_FLAG_SIMPLEX     0x10

struct NoiseParams {
	float offset;
	float scale;
	v3f spread;
	s32 seed;
	u16 octaves;
	float persist;
	float lacunarity;
	u32 flags;

	float farscale;
	float farspread;
	float farpersist;

	NoiseParams()
	{
		offset     = 0.0f;
		scale      = 1.0f;
		spread     = v3f(250, 250, 250);
		seed       = 12345;
		octaves    = 3;
		persist    = 0.6f;
		lacunarity = 2.0f;
		flags      = NOISE_FLAG_DEFAULTS;

		farscale  = 1;
		farspread = 1;
		farpersist = 1;
	}

	NoiseParams(float offset_, float scale_, v3f spread_, s32 seed_,
		u16 octaves_, float persist_, float lacunarity_,
		u32 flags_=NOISE_FLAG_DEFAULTS,
		float farscale_ = 1, float farspread_ = 1, float farpersist_ = 1
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

		farscale  = farscale_;
		farspread = farspread_;
		farpersist = farpersist_;
	}
};


// Convenience macros for getting/setting NoiseParams in Settings as a string
// WARNING:  Deprecated, use Settings::getNoiseParamsFromValue() instead
#define NOISEPARAMS_FMT_STR "f,f,v3,s32,u16,f"
//#define getNoiseParams(x, y) getStruct((x), NOISEPARAMS_FMT_STR, &(y), sizeof(y))
//#define setNoiseParams(x, y) setStruct((x), NOISEPARAMS_FMT_STR, &(y))

class Noise {
public:
	NoiseParams np;
	int seed;
	u32 sx;
	u32 sy;
	u32 sz;
	float *noise_buf;
	float *gradient_buf;
	float *persist_buf;
	float *result;

	Noise(NoiseParams *np, int seed, u32 sx, u32 sy, u32 sz=1);
	~Noise();

	void setSize(u32 sx, u32 sy, u32 sz=1);
	void setSpreadFactor(v3f spread);
	void setOctaves(int octaves);

	void gradientMap2D(
		float x, float y,
		float step_x, float step_y,
		int seed);
	void gradientMap3D(
		float x, float y, float z,
		float step_x, float step_y, float step_z,
		int seed);

	float *perlinMap2D(float x, float y, float *persistence_map=NULL);
	float *perlinMap3D(float x, float y, float z, float *persistence_map=NULL);

	inline float *perlinMap2D_PO(float x, float xoff, float y, float yoff,
		float *persistence_map=NULL)
	{
		return perlinMap2D(
			x + xoff * np.spread.X * farscale(np.farspread, x, y),
			y + yoff * np.spread.Y * farscale(np.farspread, x, y),
			persistence_map);
	}

	inline float *perlinMap3D_PO(float x, float xoff, float y, float yoff,
		float z, float zoff, float *persistence_map=NULL)
	{
		return perlinMap3D(
			x + xoff * np.spread.X * farscale(np.farspread, x, y, z),
			y + yoff * np.spread.Y * farscale(np.farspread, x, y, z),
			z + zoff * np.spread.Z * farscale(np.farspread, x, y, z),
			persistence_map);
	}

private:
	void allocBuffers();
	void resizeNoiseBuf(bool is3d);
	void updateResults(float g, float *gmap, float *persistence_map, size_t bufsize);

};

float NoisePerlin2D(NoiseParams *np, float x, float y, int seed);
float NoisePerlin3D(NoiseParams *np, float x, float y, float z, int seed);

inline float NoisePerlin2D_PO(NoiseParams *np, float x, float xoff,
	float y, float yoff, int seed)
{
	return NoisePerlin2D(np,
		x + xoff * np->spread.X,
		y + yoff * np->spread.Y,
		seed);
}

inline float NoisePerlin3D_PO(NoiseParams *np, float x, float xoff,
	float y, float yoff, float z, float zoff, int seed)
{
	return NoisePerlin3D(np,
		x + xoff * np->spread.X,
		y + yoff * np->spread.Y,
		z + zoff * np->spread.Z,
		seed);
}

// Return value: -1 ... 1
float noise2d(int x, int y, int seed);
float noise3d(int x, int y, int z, int seed);

float noise2d_gradient(float x, float y, int seed, bool eased=true);
float noise3d_gradient(float x, float y, float z, int seed, bool eased=false);

float noise2d_perlin(float x, float y, int seed,
		int octaves, float persistence, bool eased=true);

float noise2d_perlin_abs(float x, float y, int seed,
		int octaves, float persistence, bool eased=true);

float noise3d_perlin(float x, float y, float z, int seed,
		int octaves, float persistence, bool eased=false);

float noise3d_perlin_abs(float x, float y, float z, int seed,
		int octaves, float persistence, bool eased=false);

inline float easeCurve(float t)
{
	return t * t * t * (t * (6.f * t - 15.f) + 10.f);
}

float contour(float v);

#endif

