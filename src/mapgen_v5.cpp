/*
Minetest
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

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


#include "mapgen.h"
#include "voxel.h"
#include "noise.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "content_sao.h"
#include "nodedef.h"
#include "voxelalgorithms.h"
#include "profiler.h"
#include "settings.h" // For g_settings
#include "emerge.h"
#include "dungeongen.h"
#include "cavegen.h"
#include "treegen.h"
#include "biome.h"
#include "mapgen_v5.h"
#include "util/directiontables.h"

// TODO: Remove
#define WATER_LEVEL 1

/*
	Boilerplate stuff
*/

MapgenV5::MapgenV5(int mapgenid, MapgenParams *params, EmergeManager *emerge_)
{
	generating  = false;
	id     = mapgenid;
	emerge = emerge_;

	seed        = (int)params->seed;
	water_level = params->water_level;
	flags       = params->flags;
	gennotify   = emerge->gennotify;

	csize   = v3s16(1, 1, 1) * params->chunksize * MAP_BLOCKSIZE;

	// amount of elements to skip for the next index
	// for noise/height/biome maps (not vmanip)
	ystride = csize.X;
	zstride = csize.X * csize.Y;
}

MapgenV5::~MapgenV5()
{
}

/*
	Actual interface
*/

int MapgenV5::getGroundLevelAtPoint(v2s16 p)
{
	// TODO
	return 0;
}

void MapgenV5::makeChunk(BlockMakeData *data)
{
	assert(data->vmanip);
	assert(data->nodedef);
	assert(data->blockpos_requested.X >= data->blockpos_min.X &&
		   data->blockpos_requested.Y >= data->blockpos_min.Y &&
		   data->blockpos_requested.Z >= data->blockpos_min.Z);
	assert(data->blockpos_requested.X <= data->blockpos_max.X &&
		   data->blockpos_requested.Y <= data->blockpos_max.Y &&
		   data->blockpos_requested.Z <= data->blockpos_max.Z);
			
	generating = true;
	vm   = data->vmanip;	
	ndef = data->nodedef;
	//TimeTaker t("makeChunk");
	
	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	blockseed = emerge->getBlockSeed(full_node_min);  //////use getBlockSeed2()!
	
	c_stone           = ndef->getId("mapgen_stone");
	c_dirt            = ndef->getId("mapgen_dirt");
	c_dirt_with_grass = ndef->getId("mapgen_dirt_with_grass");
	c_sand            = ndef->getId("mapgen_sand");
	c_water_source    = ndef->getId("mapgen_water_source");
	c_lava_source     = ndef->getId("mapgen_lava_source");
	c_gravel          = ndef->getId("mapgen_gravel");
	c_cobble          = ndef->getId("mapgen_cobble");
	c_ice             = ndef->getId("default:ice");
	c_mossycobble     = ndef->getId("mapgen_mossycobble");
	c_sandbrick       = ndef->getId("mapgen_sandstonebrick");
	c_stair_cobble    = ndef->getId("mapgen_stair_cobble");
	c_stair_sandstone = ndef->getId("mapgen_stair_sandstone");
	if (c_ice == CONTENT_IGNORE)
		c_ice = CONTENT_AIR;
	if (c_mossycobble == CONTENT_IGNORE)
		c_mossycobble = c_cobble;
	if (c_sandbrick == CONTENT_IGNORE)
		c_sandbrick = c_desert_stone;
	if (c_stair_cobble == CONTENT_IGNORE)
		c_stair_cobble = c_cobble;
	if (c_stair_sandstone == CONTENT_IGNORE)
		c_stair_sandstone = c_sandbrick;

	// Do stuff
	actuallyGenerate();

	// Generate the registered decorations
	for (unsigned int i = 0; i != emerge->decorations.size(); i++) {
		Decoration *deco = emerge->decorations[i];
		deco->placeDeco(this, blockseed + i, node_min, node_max);
	}

	// Generate the registered ores
	for (unsigned int i = 0; i != emerge->ores.size(); i++) {
		Ore *ore = emerge->ores[i];
		ore->placeOre(this, blockseed + i, node_min, node_max);
	}

	//printf("makeChunk: %dms\n", t.stop());
	
	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);
	
	if (flags & MG_LIGHT)
		calcLighting(node_min - v3s16(1, 0, 1) * MAP_BLOCKSIZE,
					 node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE);
	//setLighting(node_min - v3s16(1, 0, 1) * MAP_BLOCKSIZE,
	//			node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE, 0xFF);
	
	generating = false;
}

/*
	A copy of the old noise implementation - noise.h
*/

enum OldNoiseType
{
	OLDNOISE_CONSTANT_ONE,
	OLDNOISE_PERLIN,
	OLDNOISE_PERLIN_ABS,
	OLDNOISE_PERLIN_CONTOUR,
	OLDNOISE_PERLIN_CONTOUR_FLIP_YZ,
};

struct OldNoiseParams
{
	OldNoiseType type;
	int seed;
	int octaves;
	double persistence;
	double pos_scale;
	double noise_scale; // Useful for contour noises
	
	OldNoiseParams(OldNoiseType type_=OLDNOISE_PERLIN, int seed_=0,
			int octaves_=3, double persistence_=0.5,
			double pos_scale_=100.0, double noise_scale_=1.0):
		type(type_),
		seed(seed_),
		octaves(octaves_),
		persistence(persistence_),
		pos_scale(pos_scale_),
		noise_scale(noise_scale_)
	{
	}
};

double noise3d_param(const OldNoiseParams &param, double x, double y, double z);

class OldNoiseBuffer
{
public:
	OldNoiseBuffer();
	~OldNoiseBuffer();
	
	void clear();
	void create(const OldNoiseParams &param,
			double first_x, double first_y, double first_z,
			double last_x, double last_y, double last_z,
			double samplelength_x, double samplelength_y, double samplelength_z);
	void multiply(const OldNoiseParams &param);
	// Deprecated
	void create(int seed, int octaves, double persistence,
			bool abs,
			double first_x, double first_y, double first_z,
			double last_x, double last_y, double last_z,
			double samplelength_x, double samplelength_y, double samplelength_z);

	void intSet(int x, int y, int z, double d);
	void intMultiply(int x, int y, int z, double d);
	double intGet(int x, int y, int z);
	double get(double x, double y, double z);
	//bool contains(double x, double y, double z);

private:
	double *m_data;
	double m_start_x, m_start_y, m_start_z;
	double m_samplelength_x, m_samplelength_y, m_samplelength_z;
	int m_size_x, m_size_y, m_size_z;
};

/*
	A copy of the old noise implementation - noise.cpp
*/

// -1->0, 0->1, 1->0
double contour(double v)
{
	v = fabs(v);
	if(v >= 1.0)
		return 0.0;
	return (1.0-v);
}

double noise3d_param(const OldNoiseParams &param, double x, double y, double z)
{
	double s = param.pos_scale;
	x /= s;
	y /= s;
	z /= s;

	if(param.type == OLDNOISE_CONSTANT_ONE)
	{
		return 1.0;
	}
	else if(param.type == OLDNOISE_PERLIN)
	{
		return param.noise_scale*noise3d_perlin(x,y,z, param.seed,
				param.octaves,
				param.persistence);
	}
	else if(param.type == OLDNOISE_PERLIN_ABS)
	{
		return param.noise_scale*noise3d_perlin_abs(x,y,z, param.seed,
				param.octaves,
				param.persistence);
	}
	else if(param.type == OLDNOISE_PERLIN_CONTOUR)
	{
		return contour(param.noise_scale*noise3d_perlin(x,y,z,
				param.seed, param.octaves,
				param.persistence));
	}
	else if(param.type == OLDNOISE_PERLIN_CONTOUR_FLIP_YZ)
	{
		return contour(param.noise_scale*noise3d_perlin(x,z,y,
				param.seed, param.octaves,
				param.persistence));
	}
	else assert(0);
}

/* OldNoiseBuffer */

OldNoiseBuffer::OldNoiseBuffer():
	m_data(NULL)
{
}

OldNoiseBuffer::~OldNoiseBuffer()
{
	clear();
}

void OldNoiseBuffer::clear()
{
	if(m_data)
		delete[] m_data;
	m_data = NULL;
	m_size_x = 0;
	m_size_y = 0;
	m_size_z = 0;
}

void OldNoiseBuffer::create(const OldNoiseParams &param,
		double first_x, double first_y, double first_z,
		double last_x, double last_y, double last_z,
		double samplelength_x, double samplelength_y, double samplelength_z)
{
	clear();
	
	m_start_x = first_x - samplelength_x;
	m_start_y = first_y - samplelength_y;
	m_start_z = first_z - samplelength_z;
	m_samplelength_x = samplelength_x;
	m_samplelength_y = samplelength_y;
	m_samplelength_z = samplelength_z;

	m_size_x = (last_x - m_start_x)/samplelength_x + 2;
	m_size_y = (last_y - m_start_y)/samplelength_y + 2;
	m_size_z = (last_z - m_start_z)/samplelength_z + 2;

	m_data = new double[m_size_x*m_size_y*m_size_z];

	for(int x=0; x<m_size_x; x++)
	for(int y=0; y<m_size_y; y++)
	for(int z=0; z<m_size_z; z++)
	{
		double xd = (m_start_x + (double)x*m_samplelength_x);
		double yd = (m_start_y + (double)y*m_samplelength_y);
		double zd = (m_start_z + (double)z*m_samplelength_z);
		double a = noise3d_param(param, xd,yd,zd);
		intSet(x,y,z, a);
	}
}

void OldNoiseBuffer::multiply(const OldNoiseParams &param)
{
	assert(m_data != NULL);

	for(int x=0; x<m_size_x; x++)
	for(int y=0; y<m_size_y; y++)
	for(int z=0; z<m_size_z; z++)
	{
		double xd = (m_start_x + (double)x*m_samplelength_x);
		double yd = (m_start_y + (double)y*m_samplelength_y);
		double zd = (m_start_z + (double)z*m_samplelength_z);
		double a = noise3d_param(param, xd,yd,zd);
		intMultiply(x,y,z, a);
	}
}

// Deprecated
void OldNoiseBuffer::create(int seed, int octaves, double persistence,
		bool abs,
		double first_x, double first_y, double first_z,
		double last_x, double last_y, double last_z,
		double samplelength_x, double samplelength_y, double samplelength_z)
{
	OldNoiseParams param;
	param.type = abs ? OLDNOISE_PERLIN_ABS : OLDNOISE_PERLIN;
	param.seed = seed;
	param.octaves = octaves;
	param.persistence = persistence;

	create(param, first_x, first_y, first_z,
			last_x, last_y, last_z,
			samplelength_x, samplelength_y, samplelength_z);
}

void OldNoiseBuffer::intSet(int x, int y, int z, double d)
{
	int i = m_size_x*m_size_y*z + m_size_x*y + x;
	assert(i >= 0);
	assert(i < m_size_x*m_size_y*m_size_z);
	m_data[i] = d;
}

void OldNoiseBuffer::intMultiply(int x, int y, int z, double d)
{
	int i = m_size_x*m_size_y*z + m_size_x*y + x;
	assert(i >= 0);
	assert(i < m_size_x*m_size_y*m_size_z);
	m_data[i] = m_data[i] * d;
}

double OldNoiseBuffer::intGet(int x, int y, int z)
{
	int i = m_size_x*m_size_y*z + m_size_x*y + x;
	assert(i >= 0);
	assert(i < m_size_x*m_size_y*m_size_z);
	return m_data[i];
}

double OldNoiseBuffer::get(double x, double y, double z)
{
	x -= m_start_x;
	y -= m_start_y;
	z -= m_start_z;
	x /= m_samplelength_x;
	y /= m_samplelength_y;
	z /= m_samplelength_z;
	// Calculate the integer coordinates
	int x0 = (x > 0.0 ? (int)x : (int)x - 1);
	int y0 = (y > 0.0 ? (int)y : (int)y - 1);
	int z0 = (z > 0.0 ? (int)z : (int)z - 1);
	// Calculate the remaining part of the coordinates
	double xl = x - (double)x0;
	double yl = y - (double)y0;
	double zl = z - (double)z0;
	// Get values for corners of cube
	double v000 = intGet(x0,   y0,   z0);
	double v100 = intGet(x0+1, y0,   z0);
	double v010 = intGet(x0,   y0+1, z0);
	double v110 = intGet(x0+1, y0+1, z0);
	double v001 = intGet(x0,   y0,   z0+1);
	double v101 = intGet(x0+1, y0,   z0+1);
	double v011 = intGet(x0,   y0+1, z0+1);
	double v111 = intGet(x0+1, y0+1, z0+1);
	// Interpolate
	return triLinearInterpolation(v000,v100,v010,v110,v001,v101,v011,v111,xl,yl,zl);
}

/*bool OldNoiseBuffer::contains(double x, double y, double z)
{
	x -= m_start_x;
	y -= m_start_y;
	z -= m_start_z;
	x /= m_samplelength_x;
	y /= m_samplelength_y;
	z /= m_samplelength_z;
	if(x <= 0.0 || x >= m_size_x)
}*/


/*
	OldNoise functions. Make sure seed is mangled differently in each one.
*/

/*
	Scaling the output of the noise function affects the overdrive of the
	contour function, which affects the shape of the output considerably.
*/
#define CAVE_NOISE_SCALE 12.0
//#define CAVE_NOISE_SCALE 10.0
//#define CAVE_NOISE_SCALE 7.5
//#define CAVE_NOISE_SCALE 5.0
//#define CAVE_NOISE_SCALE 1.0

//#define CAVE_NOISE_THRESHOLD (2.5/CAVE_NOISE_SCALE)
#define CAVE_NOISE_THRESHOLD (1.5/CAVE_NOISE_SCALE)

OldNoiseParams get_cave_noise1_params(u64 seed)
{
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR, seed+52534, 5, 0.7,
			200, CAVE_NOISE_SCALE);*/
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR, seed+52534, 4, 0.7,
			100, CAVE_NOISE_SCALE);*/
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR, seed+52534, 5, 0.6,
			100, CAVE_NOISE_SCALE);*/
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR, seed+52534, 5, 0.3,
			100, CAVE_NOISE_SCALE);*/
	return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR, seed+52534, 4, 0.5,
			50, CAVE_NOISE_SCALE);
	//return OldNoiseParams(OLDNOISE_CONSTANT_ONE);
}

OldNoiseParams get_cave_noise2_params(u64 seed)
{
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR_FLIP_YZ, seed+10325, 5, 0.7,
			200, CAVE_NOISE_SCALE);*/
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR_FLIP_YZ, seed+10325, 4, 0.7,
			100, CAVE_NOISE_SCALE);*/
	/*return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR_FLIP_YZ, seed+10325, 5, 0.3,
			100, CAVE_NOISE_SCALE);*/
	return OldNoiseParams(OLDNOISE_PERLIN_CONTOUR_FLIP_YZ, seed+10325, 4, 0.5,
			50, CAVE_NOISE_SCALE);
	//return OldNoiseParams(OLDNOISE_CONSTANT_ONE);
}

OldNoiseParams get_ground_noise1_params(u64 seed)
{
	return OldNoiseParams(OLDNOISE_PERLIN, seed+983240, 4,
			0.55, 80.0, 40.0);
}

OldNoiseParams get_ground_crumbleness_params(u64 seed)
{
	return OldNoiseParams(OLDNOISE_PERLIN, seed+34413, 3,
			1.3, 20.0, 1.0);
}

OldNoiseParams get_ground_wetness_params(u64 seed)
{
	return OldNoiseParams(OLDNOISE_PERLIN, seed+32474, 4,
			1.1, 40.0, 1.0);
}

bool is_cave(u64 seed, v3s16 p)
{
	double d1 = noise3d_param(get_cave_noise1_params(seed), p.X,p.Y,p.Z);
	double d2 = noise3d_param(get_cave_noise2_params(seed), p.X,p.Y,p.Z);
	return d1*d2 > CAVE_NOISE_THRESHOLD;
}

/*
	Ground density noise shall be interpreted by using this.

	TODO: No perlin noises here, they should be outsourced
	      and buffered
		  NOTE: The speed of these actually isn't terrible
*/
bool val_is_ground(double ground_noise1_val, v3s16 p, u64 seed)
{
	//return ((double)p.Y < ground_noise1_val);

	double f = 0.55 + noise2d_perlin(
			0.5+(float)p.X/250, 0.5+(float)p.Z/250,
			seed+920381, 3, 0.45);
	if(f < 0.01)
		f = 0.01;
	else if(f >= 1.0)
		f *= 1.6;
	double h = WATER_LEVEL + 10 * noise2d_perlin(
			0.5+(float)p.X/250, 0.5+(float)p.Z/250,
			seed+84174, 4, 0.5);
	/*double f = 1;
	double h = 0;*/
	return ((double)p.Y - h < ground_noise1_val * f);
}

/*
	Queries whether a position is ground or not.
*/
bool is_ground(u64 seed, v3s16 p)
{
	double val1 = noise3d_param(get_ground_noise1_params(seed), p.X,p.Y,p.Z);
	return val_is_ground(val1, p, seed);
}

/*
	Incrementally find ground level from 3d noise
*/
s16 find_ground_level_from_noise(u64 seed, v2s16 p2d, s16 precision)
{
	// Start a bit fuzzy to make averaging lower precision values
	// more useful
	s16 level = myrand_range(-precision/2, precision/2);
	s16 dec[] = {31000, 100, 20, 4, 1, 0};
	s16 i;
	for(i = 1; dec[i] != 0 && precision <= dec[i]; i++)
	{
		// First find non-ground by going upwards
		// Don't stop in caves.
		{
			s16 max = level+dec[i-1]*2;
			v3s16 p(p2d.X, level, p2d.Y);
			for(; p.Y < max; p.Y += dec[i])
			{
				if(!is_ground(seed, p))
				{
					level = p.Y;
					break;
				}
			}
		}
		// Then find ground by going downwards from there.
		// Go in caves, too, when precision is 1.
		{
			s16 min = level-dec[i-1]*2;
			v3s16 p(p2d.X, level, p2d.Y);
			for(; p.Y>min; p.Y-=dec[i])
			{
				bool ground = is_ground(seed, p);
				/*if(dec[i] == 1 && is_cave(seed, p))
					ground = false;*/
				if(ground)
				{
					level = p.Y;
					break;
				}
			}
		}
	}
	
	// This is more like the actual ground level
	level += dec[i-1]/2;

	return level;
}

double get_sector_average_ground_level(u64 seed, v3s16 node_min, v3s16 node_max, double p=4)
{
	double a = 0;
	a += find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_min.Z), p);
	a += find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_max.Z), p);
	a += find_ground_level_from_noise(seed,
			v2s16(node_max.X, node_max.Z), p);
	a += find_ground_level_from_noise(seed,
			v2s16(node_max.X, node_min.Z), p);
	a += find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, (node_min.Z+node_max.Z)/2), p);
	a /= 5;
	return a;
}

double get_sector_maximum_ground_level(u64 seed, v3s16 node_min, v3s16 node_max, double p=4)
{
	double a = -31000;
	// Corners
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_min.Z), p));
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_max.Z), p));
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16(node_max.X, node_max.Z), p));
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_min.Z), p));
	// Center
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, (node_min.Z+node_max.Z)/2), p));
	// Side middle points
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, node_min.Z), p));
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, node_max.Z), p));
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, (node_min.Z+node_max.Z)/2), p));
	a = MYMAX(a, find_ground_level_from_noise(seed,
			v2s16(node_max.X, (node_min.Z+node_max.Z)/2), p));
	return a;
}

double get_sector_minimum_ground_level(u64 seed, v3s16 node_min, v3s16 node_max, double p=4)
{
	double a = 31000;
	// Corners
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_min.Z), p));
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_max.Z), p));
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16(node_max.X, node_max.Z), p));
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, node_min.Z), p));
	// Center
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, (node_min.Z+node_max.Z)/2), p));
	// Side middle points
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, node_min.Z), p));
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16((node_min.X+node_max.X)/2, node_max.Z), p));
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16(node_min.X, (node_min.Z+node_max.Z)/2), p));
	a = MYMIN(a, find_ground_level_from_noise(seed,
			v2s16(node_max.X, (node_min.Z+node_max.Z)/2), p));
	return a;
}

bool block_is_underground(u64 seed, v3s16 node_min, v3s16 node_max)
{
	s16 minimum_groundlevel = (s16)get_sector_minimum_ground_level(
			seed, node_min, node_max);
	
	if(node_max.Y < minimum_groundlevel)
		return true;
	else
		return false;
}

#if 0
#define AVERAGE_MUD_AMOUNT 4

double base_rock_level_2d(u64 seed, v2s16 p)
{
	// The base ground level
	double base = (double)WATER_LEVEL - (double)AVERAGE_MUD_AMOUNT
			+ 20. * noise2d_perlin(
			0.5+(float)p.X/500., 0.5+(float)p.Y/500.,
			(seed>>32)+654879876, 6, 0.6);

	/*// A bit hillier one
	double base2 = WATER_LEVEL - 4.0 + 40. * noise2d_perlin(
			0.5+(float)p.X/250., 0.5+(float)p.Y/250.,
			(seed>>27)+90340, 6, 0.69);
	if(base2 > base)
		base = base2;*/
#if 1
	// Higher ground level
	double higher = (double)WATER_LEVEL + 25. + 35. * noise2d_perlin(
			0.5+(float)p.X/250., 0.5+(float)p.Y/250.,
			seed+85039, 5, 0.69);
	//higher = 30; // For debugging

	// Limit higher to at least base
	if(higher < base)
		higher = base;

	// Steepness factor of cliffs
	double b = 1.0 + 1.0 * noise2d_perlin(
			0.5+(float)p.X/250., 0.5+(float)p.Y/250.,
			seed-932, 7, 0.7);
	b = rangelim(b, 0.0, 1000.0);
	b = pow(b, 5);
	b *= 7;
	b = rangelim(b, 3.0, 1000.0);
	//dstream<<"b="<<b<<std::endl;
	//double b = 20;

	// Offset to more low
	double a_off = -0.2;
	// High/low selector
	/*double a = 0.5 + b * (a_off + noise2d_perlin(
			0.5+(float)p.X/500., 0.5+(float)p.Y/500.,
			seed-359, 6, 0.7));*/
	double a = (double)0.5 + b * (a_off + noise2d_perlin(
			0.5+(float)p.X/250., 0.5+(float)p.Y/250.,
			seed-359, 5, 0.60));
	// Limit
	a = rangelim(a, 0.0, 1.0);

	//dstream<<"a="<<a<<std::endl;

	double h = base*(1.0-a) + higher*a;
#else
	double h = base;
#endif
	return h;
}

double get_mud_add_amount(u64 seed, v2s16 p)
{
	return ((float)AVERAGE_MUD_AMOUNT + 3.0 * noise2d_perlin(
			0.5+(float)p.X/200, 0.5+(float)p.Y/200,
			seed+91013, 3, 0.55));
}
#endif

bool get_have_sand(u64 seed, v2s16 p2d)
{
	// Determine whether to have sand here
	double sandnoise = noise2d_perlin(
			0.5+(float)p2d.X/500, 0.5+(float)p2d.Y/500,
			seed+59420, 3, 0.50);

	return (sandnoise > -0.15);
}

/*
	Adds random objects to block, depending on the content of the block
*/
void add_random_objects(MapBlock *block)
{
#if 0
	for(s16 z0=0; z0<MAP_BLOCKSIZE; z0++)
	for(s16 x0=0; x0<MAP_BLOCKSIZE; x0++)
	{
		bool last_node_walkable = false;
		for(s16 y0=0; y0<MAP_BLOCKSIZE; y0++)
		{
			v3s16 p(x0,y0,z0);
			MapNode n = block->getNodeNoEx(p);
			if(n.getContent() == CONTENT_IGNORE)
				continue;
			if(content_features(n).liquid_type != LIQUID_NONE)
				continue;
			if(content_features(n).walkable)
			{
				last_node_walkable = true;
				continue;
			}
			if(last_node_walkable)
			{
				// If block contains light information
				if(content_features(n).param_type == CPT_LIGHT)
				{
					if(n.getLight(LIGHTBANK_DAY) <= 3)
					{
						if(myrand() % 300 == 0)
						{
							v3f pos_f = intToFloat(p+block->getPosRelative(), BS);
							pos_f.Y -= BS*0.4;
							ServerActiveObject *obj = new RatSAO(NULL, 0, pos_f);
							std::string data = obj->getStaticData();
							StaticObject s_obj(obj->getType(),
									obj->getBasePosition(), data);
							// Add some
							block->m_static_objects.insert(0, s_obj);
							block->m_static_objects.insert(0, s_obj);
							block->m_static_objects.insert(0, s_obj);
							block->m_static_objects.insert(0, s_obj);
							block->m_static_objects.insert(0, s_obj);
							block->m_static_objects.insert(0, s_obj);
							delete obj;
						}
						if(myrand() % 1000 == 0)
						{
							v3f pos_f = intToFloat(p+block->getPosRelative(), BS);
							pos_f.Y -= BS*0.4;
							ServerActiveObject *obj = new Oerkki1SAO(NULL,0,pos_f);
							std::string data = obj->getStaticData();
							StaticObject s_obj(obj->getType(),
									obj->getBasePosition(), data);
							// Add one
							block->m_static_objects.insert(0, s_obj);
							delete obj;
						}
					}
				}
			}
			last_node_walkable = false;
		}
	}
	block->setChangedFlag();
#endif
}

/*
	This is where things actually happen
*/

void MapgenV5::actuallyGenerate()
{
	ManualMapVoxelManipulator &vmanip = *vm;

	v2s16 p2d_center(node_min.X+MAP_BLOCKSIZE/2, node_min.Z+MAP_BLOCKSIZE/2);

	/*
		Get average ground level from noise
	*/
	
	s16 minimum_groundlevel = (s16)get_sector_minimum_ground_level(
			seed, node_min, node_max);
	// Minimum amount of ground above the top of the central block
	s16 minimum_ground_depth = minimum_groundlevel - node_max.Y;

	s16 maximum_groundlevel = (s16)get_sector_maximum_ground_level(
			seed, node_min, node_max, 1);
	// Maximum amount of ground above the bottom of the central block
	s16 maximum_ground_depth = maximum_groundlevel - node_min.Y;

	#if 0
	/*
		Special case for high air or water: Just fill with air and water.
	*/
	if(maximum_ground_depth < -20)
	{
		for(s16 x=node_min.X; x<=node_max.X; x++)
		for(s16 z=node_min.Z; z<=node_max.Z; z++)
		{
			// Node position
			v2s16 p2d(x,z);
			{
				// Use fast index incrementing
				v3s16 em = vmanip.m_area.getExtent();
				u32 i = vmanip.m_area.index(v3s16(p2d.X, node_min.Y, p2d.Y));
				for(s16 y=node_min.Y; y<=node_max.Y; y++)
				{
					// Only modify places that have no content
					if(vmanip.m_data[i].getContent() == CONTENT_IGNORE)
					{
						if(y <= WATER_LEVEL)
							vmanip.m_data[i] = MapNode(c_water_source);
						else
							vmanip.m_data[i] = MapNode(CONTENT_AIR);
					}
				
					vmanip.m_area.add_y(em, i, 1);
				}
			}
		}
		
		// We're done
		return;
	}
	#endif

	/*
		If block is deep underground, this is set to true and ground
		density noise is not generated, for speed optimization.
	*/
	bool all_is_ground_except_caves = (minimum_ground_depth > 40);
	
	/*
		Create a block-specific seed
	*/
	u32 blockseed = (u32)(seed%0x100000000ULL) + full_node_min.Z*38134234
			+ full_node_min.Y*42123 + full_node_min.X*23;
	
	/*
		Make some 3D noise
	*/
	
	//OldNoiseBuffer noisebuf1;
	//OldNoiseBuffer noisebuf2;
	OldNoiseBuffer noisebuf_cave;
	OldNoiseBuffer noisebuf_ground;
	OldNoiseBuffer noisebuf_ground_crumbleness;
	OldNoiseBuffer noisebuf_ground_wetness;
	{
		v3f minpos_f(node_min.X, node_min.Y, node_min.Z);
		v3f maxpos_f(node_max.X, node_max.Y, node_max.Z);

		//TimeTaker timer("noisebuf.create");

		/*
			Cave noise
		*/
#if 1
		noisebuf_cave.create(get_cave_noise1_params(seed),
				minpos_f.X, minpos_f.Y, minpos_f.Z,
				maxpos_f.X, maxpos_f.Y, maxpos_f.Z,
				2, 2, 2);
		noisebuf_cave.multiply(get_cave_noise2_params(seed));
#endif

		/*
			Ground noise
		*/
		
		// Sample length
		v3f sl = v3f(4.0, 4.0, 4.0);
		
		/*
			Density noise
		*/
		if(all_is_ground_except_caves == false)
			//noisebuf_ground.create(seed+983240, 6, 0.60, false,
			noisebuf_ground.create(get_ground_noise1_params(seed),
					minpos_f.X, minpos_f.Y, minpos_f.Z,
					maxpos_f.X, maxpos_f.Y, maxpos_f.Z,
					sl.X, sl.Y, sl.Z);
		
		/*
			Ground property noise
		*/
		sl = v3f(2.5, 2.5, 2.5);
		noisebuf_ground_crumbleness.create(
				get_ground_crumbleness_params(seed),
				minpos_f.X, minpos_f.Y, minpos_f.Z,
				maxpos_f.X, maxpos_f.Y+5, maxpos_f.Z,
				sl.X, sl.Y, sl.Z);
		noisebuf_ground_wetness.create(
				get_ground_wetness_params(seed),
				minpos_f.X, minpos_f.Y, minpos_f.Z,
				maxpos_f.X, maxpos_f.Y+5, maxpos_f.Z,
				sl.X, sl.Y, sl.Z);
	}
	
	/*
		Make base ground level
	*/

	for(s16 x=node_min.X; x<=node_max.X; x++)
	for(s16 z=node_min.Z; z<=node_max.Z; z++)
	{
		// Node position
		v2s16 p2d(x,z);
		{
			// Use fast index incrementing
			v3s16 em = vmanip.m_area.getExtent();
			u32 i = vmanip.m_area.index(v3s16(p2d.X, node_min.Y, p2d.Y));
			for(s16 y=node_min.Y; y<=node_max.Y; y++)
			{
				// Only modify places that have no content
				if(vmanip.m_data[i].getContent() == CONTENT_IGNORE)
				{
					// First priority: make air and water.
					// This avoids caves inside water.
					if(all_is_ground_except_caves == false
							&& val_is_ground(noisebuf_ground.get(x,y,z),
							v3s16(x,y,z), seed) == false)
					{
						if(y <= WATER_LEVEL)
							vmanip.m_data[i] = MapNode(c_water_source);
						else
							vmanip.m_data[i] = MapNode(CONTENT_AIR);
					}
					else if(noisebuf_cave.get(x,y,z) > CAVE_NOISE_THRESHOLD)
						vmanip.m_data[i] = MapNode(CONTENT_AIR);
					else
						vmanip.m_data[i] = MapNode(c_stone);
				}
			
				vmanip.m_area.add_y(em, i, 1);
			}
		}
	}

	/*
		Add mud and sand and others underground (in place of stone)
	*/

	for(s16 x=node_min.X; x<=node_max.X; x++)
	for(s16 z=node_min.Z; z<=node_max.Z; z++)
	{
		// Node position
		v2s16 p2d(x,z);
		{
			// Use fast index incrementing
			v3s16 em = vmanip.m_area.getExtent();
			u32 i = vmanip.m_area.index(v3s16(p2d.X, node_max.Y, p2d.Y));
			for(s16 y=node_max.Y; y>=node_min.Y; y--)
			{
				if(vmanip.m_data[i].getContent() == c_stone)
				{
					if(noisebuf_ground_crumbleness.get(x,y,z) > 1.3)
					{
						if(noisebuf_ground_wetness.get(x,y,z) > 0.0)
							vmanip.m_data[i] = MapNode(c_dirt);
						else
							vmanip.m_data[i] = MapNode(c_sand);
					}
					else if(noisebuf_ground_crumbleness.get(x,y,z) > 0.7)
					{
						if(noisebuf_ground_wetness.get(x,y,z) < -0.6)
							vmanip.m_data[i] = MapNode(c_gravel);
					}
					else if(noisebuf_ground_crumbleness.get(x,y,z) <
							-3.0 + MYMIN(0.1 * sqrt((float)MYMAX(0, -y)), 1.5))
					{
						vmanip.m_data[i] = MapNode(c_lava_source);
						// TODO: Is this needed?
						/*for(s16 x1=-1; x1<=1; x1++)
						for(s16 y1=-1; y1<=1; y1++)
						for(s16 z1=-1; z1<=1; z1++)
							data->transforming_liquid.push_back(
									v3s16(p2d.X+x1, y+y1, p2d.Y+z1));*/
					}
				}

				vmanip.m_area.add_y(em, i, -1);
			}
		}
	}
	
	// Add dungeons
	{
		DungeonParams dp;

		dp.np_rarity  = nparams_dungeon_rarity;
		dp.np_density = nparams_dungeon_density;
		dp.np_wetness = nparams_dungeon_wetness;
		dp.c_water = c_water_source;
		// TODO
		//if (getBiome(0, v2s16(node_min.X, node_min.Z)) == BT_NORMAL) {
		if (1) {
			dp.c_cobble  = c_cobble;
			dp.c_moss    = c_mossycobble;
			dp.c_stair   = c_stair_cobble;

			dp.diagonal_dirs = false;
			dp.mossratio  = 3.0;
			dp.holesize   = v3s16(1, 2, 1);
			dp.roomsize   = v3s16(0, 0, 0);
			dp.notifytype = GENNOTIFY_DUNGEON;
		} /*else {
			dp.c_cobble  = c_sandbrick;
			dp.c_moss    = c_sandbrick; // should make this 'cracked sandstone' later
			dp.c_stair   = c_stair_sandstone;

			dp.diagonal_dirs = true;
			dp.mossratio  = 0.0;
			dp.holesize   = v3s16(2, 3, 2);
			dp.roomsize   = v3s16(2, 5, 2);
			dp.notifytype = GENNOTIFY_TEMPLE;
		}*/

		DungeonGen dgen(this, &dp);
		dgen.generate(blockseed, full_node_min, full_node_max);
	}

	/*
		If close to ground level
	*/

	//if(abs(approx_ground_depth) < 30)
	if(minimum_ground_depth < 5 && maximum_ground_depth > -5)
	{
		/*
			Add grass and mud
		*/

		for(s16 x=node_min.X; x<=node_max.X; x++)
		for(s16 z=node_min.Z; z<=node_max.Z; z++)
		{
			// Node position
			v2s16 p2d(x,z);
			{
				bool possibly_have_sand = get_have_sand(seed, p2d);
				bool have_sand = false;
				u32 current_depth = 0;
				bool air_detected = false;
				bool water_detected = false;

				// Use fast index incrementing
				s16 start_y = node_max.Y+2;
				v3s16 em = vmanip.m_area.getExtent();
				u32 i = vmanip.m_area.index(v3s16(p2d.X, start_y, p2d.Y));
				for(s16 y=start_y; y>=node_min.Y-3; y--)
				{
					if(vmanip.m_data[i].getContent() == c_water_source)
						water_detected = true;
					if(vmanip.m_data[i].getContent() == CONTENT_AIR)
						air_detected = true;

					if((vmanip.m_data[i].getContent() == c_stone
							|| vmanip.m_data[i].getContent() == c_dirt_with_grass
							|| vmanip.m_data[i].getContent() == c_dirt
							|| vmanip.m_data[i].getContent() == c_sand
							|| vmanip.m_data[i].getContent() == c_gravel
							) && (air_detected || water_detected))
					{
						if(current_depth == 0 && y <= WATER_LEVEL+2
								&& possibly_have_sand)
							have_sand = true;
						
						if(current_depth < 4)
						{
							if(have_sand)
								vmanip.m_data[i] = MapNode(c_sand);
							#if 1
							else if(current_depth==0 && !water_detected
									&& y >= WATER_LEVEL && air_detected)
								vmanip.m_data[i] = MapNode(c_dirt_with_grass);
							#endif
							else
								vmanip.m_data[i] = MapNode(c_dirt);
						}
						else
						{
							if(vmanip.m_data[i].getContent() == c_dirt
								|| vmanip.m_data[i].getContent() == c_dirt_with_grass)
								vmanip.m_data[i] = MapNode(c_stone);
						}

						current_depth++;

						if(current_depth >= 8)
							break;
					}
					else if(current_depth != 0)
						break;

					vmanip.m_area.add_y(em, i, -1);
				}
			}
		}
	}
}

// EOF
