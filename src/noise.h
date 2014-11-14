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

#include "debug.h"
#include "irr_v3d.h"

class PseudoRandom
{
public:
	PseudoRandom(): m_next(0)
	{
	}
	PseudoRandom(int seed): m_next(seed)
	{
	}
	void seed(int seed)
	{
		m_next = seed;
	}
	// Returns 0...32767
	int next()
	{
		m_next = m_next * 1103515245 + 12345;
		return((unsigned)(m_next/65536) % 32768);
	}
	int range(int min, int max)
	{
		if(max-min > 32768/10)
		{
			//dstream<<"WARNING: PseudoRandom::range: max > 32767"<<std::endl;
			assert(0);
		}
		if(min > max)
		{
			assert(0);
			return max;
		}
		return (next()%(max-min+1))+min;
	}
private:
	int m_next;
};

struct NoiseParams {
	float offset;
	float scale;
	v3f spread;
	int seed;
	int octaves;
	float persist;

	float farscale;
	float farspread;
	float farpersist;

	bool eased;

	NoiseParams() {}

	NoiseParams(float offset_, float scale_, v3f spread_,
		int seed_, int octaves_, float persist_, bool eased_=false,
		float farscale_ = 1, float farspread_ = 1, float farpersist_ = 1)
	{
		offset  = offset_;
		scale   = scale_;
		spread  = spread_;
		seed    = seed_;
		octaves = octaves_;
		persist = persist_;
		eased   = eased_;

		farscale  = farscale_;
		farspread = farspread_;
		farpersist = farpersist_;

	}
};


// Convenience macros for getting/setting NoiseParams in Settings

#define NOISEPARAMS_FMT_STR "f,f,v3,s32,s32,f"

#define getNoiseParams(x, y) getStruct((x), NOISEPARAMS_FMT_STR, &(y), sizeof(y))
#define setNoiseParams(x, y) setStruct((x), NOISEPARAMS_FMT_STR, &(y))

class Noise {
public:
	NoiseParams *np;
	int seed;
	int sx;
	int sy;
	int sz;
	float *noisebuf;
	float *buf;
	float *result;

	Noise(NoiseParams *np, int seed, int sx, int sy, int sz=1);
	~Noise();

	void setSize(int sx, int sy, int sz=1);
	void setSpreadFactor(v3f spread);
	void setOctaves(int octaves);
	void resizeNoiseBuf(bool is3d);

	void gradientMap2D(
		float x, float y,
		float step_x, float step_y,
		int seed);
	void gradientMap3D(
		float x, float y, float z,
		float step_x, float step_y, float step_z,
		int seed, bool eased=false);
	float *perlinMap2D(float x, float y);
	float *perlinMap2DModulated(float x, float y, float *persist_map);
	float *perlinMap3D(float x, float y, float z, bool eased=false);
	void transformNoiseMap(float xx = 0, float yy = 0, float zz = 0);
};

// Return value: -1 ... 1
float noise2d(int x, int y, int seed);
float noise3d(int x, int y, int z, int seed);

float noise2d_gradient(float x, float y, int seed);
float noise3d_gradient(float x, float y, float z, int seed, bool eased=false);

float noise2d_perlin(float x, float y, int seed,
		int octaves, float persistence);

float noise2d_perlin_abs(float x, float y, int seed,
		int octaves, float persistence);

float noise3d_perlin(float x, float y, float z, int seed,
		int octaves, float persistence, bool eased=false);

float noise3d_perlin_abs(float x, float y, float z, int seed,
		int octaves, float persistence, bool eased=false);

inline float easeCurve(float t) {
	return t * t * t * (t * (6.f * t - 15.f) + 10.f);
}

float contour(float v);

#define NoisePerlin2D(np, x, y, s) \
		((np)->offset + (np)->scale * noise2d_perlin( \
		(float)(x) / (np)->spread.X, \
		(float)(y) / (np)->spread.Y, \
		(s) + (np)->seed, (np)->octaves, (np)->persist))

#define NoisePerlin2DNoTxfm(np, x, y, s) \
		(noise2d_perlin( \
		(float)(x) / (np)->spread.X, \
		(float)(y) / (np)->spread.Y, \
		(s) + (np)->seed, (np)->octaves, (np)->persist))

#define NoisePerlin2DPosOffset(np, x, xoff, y, yoff, s) \
		((np)->offset + (np)->scale * noise2d_perlin( \
		(float)(xoff) + (float)(x) / (np)->spread.X, \
		(float)(yoff) + (float)(y) / (np)->spread.Y, \
		(s) + (np)->seed, (np)->octaves, (np)->persist))

#define NoisePerlin2DNoTxfmPosOffset(np, x, xoff, y, yoff, s) \
		(noise2d_perlin( \
		(float)(xoff) + (float)(x) / (np)->spread.X, \
		(float)(yoff) + (float)(y) / (np)->spread.Y, \
		(s) + (np)->seed, (np)->octaves, (np)->persist))

#define NoisePerlin3D(np, x, y, z, s) ((np)->offset + (np)->scale * \
		noise3d_perlin((float)(x) / (np)->spread.X, (float)(y) / (np)->spread.Y, \
		(float)(z) / (np)->spread.Z, (s) + (np)->seed, (np)->octaves, (np)->persist))

inline float linearInterpolation(float v0, float v1, float t);
/* {
    return v0 + (v1 - v0) * t;
} */

float biLinearInterpolation(float v00, float v10,
							float v01, float v11,
							float x, float y);

float biLinearInterpolationNoEase(float x0y0, float x1y0,
								  float x0y1, float x1y1,
								  float x, float y);

float triLinearInterpolation(
		float v000, float v100, float v010, float v110,
		float v001, float v101, float v011, float v111,
		float x, float y, float z);


float farscale(float scale, float z);
float farscale(float scale, float x, float z);
float farscale(float scale, float x, float y, float z);


#define NoisePerlin3DEased(np, x, y, z, s) ((np)->offset + (np)->scale * \
		noise3d_perlin((float)(x) / (np)->spread.X, (float)(y) / (np)->spread.Y, \
		(float)(z) / (np)->spread.Z, (s) + (np)->seed, (np)->octaves, \
		(np)->persist), true)

#endif

