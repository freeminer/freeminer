/*
mapblock_mesh.h
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

#ifndef MAPBLOCK_MESH_HEADER
#define MAPBLOCK_MESH_HEADER

#include "irrlichttypes_extrabloated.h"
#include "tile.h"
#include "voxel.h"
#include <map>

class IGameDef;
struct MapDrawControl;
class Map;

/*
	Mesh making stuff
*/

int getFarmeshStep(MapDrawControl& draw_control, int range);

class MapBlock;

struct MeshMakeData
{
	//VoxelManipulator m_vmanip;
	Map & m_vmanip;
	v3s16 m_blockpos;
	v3s16 m_crack_pos_relative;
	bool m_smooth_lighting;
	IGameDef *m_gamedef;
	int step;
	Map & map;
	MapDrawControl& draw_control;
	bool debug;

	MeshMakeData(IGameDef *gamedef, Map & map_, MapDrawControl& draw_control_);

	/*
		Copy central data directly from block, and other data from
		parent of block.
	*/
	void fill(MapBlock *block);

	/*
		Set up with only a single node at (1,1,1)
	*/
	void fillSingleNode(MapNode *node);

	/*
		Set the (node) position of a crack
	*/
	void setCrack(int crack_level, v3s16 crack_pos);

	/*
		Enable or disable smooth lighting
	*/
	void setSmoothLighting(bool smooth_lighting);
};

/*
	Holds a mesh for a mapblock.

	Besides the SMesh*, this contains information used for animating
	the vertex positions, colors and texture coordinates of the mesh.
	For example:
	- cracks [implemented]
	- day/night transitions [implemented]
	- animated flowing liquids [not implemented]
	- animating vertex positions for e.g. axles [not implemented]
*/
class MapBlockMesh
{
public:
	// Builds the mesh given
	MapBlockMesh(MeshMakeData *data, v3s16 camera_offset);
	~MapBlockMesh();

	// Main animation function, parameters:
	//   faraway: whether the block is far away from the camera (~50 nodes)
	//   time: the global animation time, 0 .. 60 (repeats every minute)
	//   daynight_ratio: 0 .. 1000
	//   crack: -1 .. CRACK_ANIMATION_LENGTH-1 (-1 for off)
	// Returns true if anything has been changed.
	bool animate(bool faraway, float time, int crack, u32 daynight_ratio);

	scene::SMesh* getMesh()
	{
		return m_mesh;
	}

	bool isAnimationForced() const
	{
		return m_animation_force_timer == 0;
	}

	void decreaseAnimationForceTimer()
	{
		if(m_animation_force_timer > 0)
			m_animation_force_timer--;
	}
	
	void updateCameraOffset(v3s16 camera_offset);


	u32 getUsageTimer()
	{
		return m_usage_timer;
	}
	void incrementUsageTimer(float dtime)
	{
		m_usage_timer += dtime;
		if(m_usage_timer > 10)
			setStatic();
	}

	void setStatic();

	bool clearHardwareBuffer;

	int step;

private:
	scene::SMesh *m_mesh;
	IGameDef *m_gamedef;

	// Must animate() be called before rendering?
	bool m_has_animation;
	int m_animation_force_timer;

	// Animation info: cracks
	// Last crack value passed to animate()
	int m_last_crack;
	// Maps mesh buffer (i.e. material) indices to base texture names
	std::map<u32, std::string> m_crack_materials;

	// Animation info: texture animationi
	// Maps meshbuffers to TileSpecs
	std::map<u32, TileSpec> m_animation_tiles;
	std::map<u32, int> m_animation_frames; // last animation frame
	std::map<u32, int> m_animation_frame_offsets;
	
	// Animation info: day/night transitions
	// Last daynight_ratio value passed to animate()
	u32 m_last_daynight_ratio;
	// For each meshbuffer, maps vertex indices to (day,night) pairs
	std::map<u32, std::map<u32, std::pair<u8, u8> > > m_daynight_diffs;

	u32 m_usage_timer;
	
	// Camera offset info -> do we have to translate the mesh?
	v3s16 m_camera_offset;
};



/*
	This is used because CMeshBuffer::append() is very slow
*/
struct PreMeshBuffer
{
	TileSpec tile;
	std::vector<u16> indices;
	std::vector<video::S3DVertex> vertices;
};

struct MeshCollector
{
	std::vector<PreMeshBuffer> prebuffers;

	void append(const TileSpec &material,
			const video::S3DVertex *vertices, u32 numVertices,
			const u16 *indices, u32 numIndices);
};

// This encodes
//   alpha in the A channel of the returned SColor
//   day light (0-255) in the R channel of the returned SColor
//   night light (0-255) in the G channel of the returned SColor
//   light source (0-255) in the B channel of the returned SColor
inline video::SColor MapBlock_LightColor(u8 alpha, u16 light, u8 light_source=0)
{
	return video::SColor(alpha, (light & 0xff), (light >> 8), light_source);
}

// Compute light at node
u16 getInteriorLight(MapNode n, s32 increment, INodeDefManager *ndef);
u16 getFaceLight(MapNode n, MapNode n2, v3s16 face_dir, INodeDefManager *ndef);
u16 getSmoothLight(v3s16 p, v3s16 corner, MeshMakeData *data);

// Retrieves the TileSpec of a face of a node
// Adds MATERIAL_FLAG_CRACK if the node is cracked
TileSpec getNodeTileN(MapNode mn, v3s16 p, u8 tileindex, MeshMakeData *data);
TileSpec getNodeTile(MapNode mn, v3s16 p, v3s16 dir, MeshMakeData *data);

#endif

