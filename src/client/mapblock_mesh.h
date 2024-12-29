// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"
#include "irrlichttypes_extrabloated.h"
#include "irr_ptr.h"
#include "util/numeric.h"
#include "client/tile.h"
#include "voxel.h"
#include <array>
#include <map>
#include <unordered_map>

class Client;
class NodeDefManager;
class IShaderSource;
class ITextureSource;

/*
	Mesh making stuff
*/


class MapBlock;
struct MinimapMapblock;

struct MeshMakeData
{
	VoxelManipulator m_vmanip_store;
	v3bpos_t m_blockpos = v3bpos_t(-1337,-1337,-1337);
	v3pos_t m_crack_pos_relative = v3bpos_t(-1337,-1337,-1337);
	bool m_smooth_lighting = false;
	u16 side_length;

	const NodeDefManager *nodedef;
	bool m_use_shaders;

    // fm:
	NodeContainer & m_vmanip;
	const u16 side_length_data;
	const int lod_step;
	const int far_step;
	const int fscale;

	int range{1};
	bool no_draw{};
	unsigned int timestamp{};
	bool debug{};
	// ==

	explicit MeshMakeData(const NodeDefManager *ndef, u16 side_length, bool use_shaders
			, int lod_step = 0
			, int far_step = 0
			, NodeContainer * nodecontainer = {}
			 );

	/*
		Copy block data manually (to allow optimizations by the caller)
	*/
	void fillBlockDataBegin(const v3bpos_t &blockpos);
	void fillBlockData(const v3bpos_t &bp, MapNode *data);

	/*
		Set the (node) position of a crack
	*/
	void setCrack(int crack_level, v3pos_t crack_pos);

	/*
		Enable or disable smooth lighting
	*/
	void setSmoothLighting(bool smooth_lighting);
};

// represents a triangle as indexes into the vertex buffer in SMeshBuffer
class MeshTriangle
{
public:
	scene::SMeshBuffer *buffer;
	u16 p1, p2, p3;
	v3f centroid;
	float areaSQ;

	void updateAttributes()
	{
		v3f v1 = buffer->getPosition(p1);
		v3f v2 = buffer->getPosition(p2);
		v3f v3 = buffer->getPosition(p3);

		centroid = (v1 + v2 + v3) / 3;
		areaSQ = (v2-v1).crossProduct(v3-v1).getLengthSQ() / 4;
	}

	v3f getNormal() const {
		v3f v1 = buffer->getPosition(p1);
		v3f v2 = buffer->getPosition(p2);
		v3f v3 = buffer->getPosition(p3);

		return (v2-v1).crossProduct(v3-v1);
	}
};

/**
 * Implements a binary space partitioning tree
 * See also: https://en.wikipedia.org/wiki/Binary_space_partitioning
 */
class MapBlockBspTree
{
public:
	MapBlockBspTree() {}

	void buildTree(const std::vector<MeshTriangle> *triangles, u16 side_lingth);

	void traverse(v3f viewpoint, std::vector<s32> &output) const
	{
		traverse(root, viewpoint, output);
	}

private:
	// Tree node definition;
	struct TreeNode
	{
		v3f normal;
		v3f origin;
		std::vector<s32> triangle_refs;
		s32 front_ref;
		s32 back_ref;

		TreeNode() = default;
		TreeNode(v3f normal, v3f origin, const std::vector<s32> &triangle_refs, s32 front_ref, s32 back_ref) :
				normal(normal), origin(origin), triangle_refs(triangle_refs), front_ref(front_ref), back_ref(back_ref)
		{}
	};


	s32 buildTree(v3f normal, v3f origin, float delta, const std::vector<s32> &list, u32 depth);
	void traverse(s32 node, v3f viewpoint, std::vector<s32> &output) const;

	const std::vector<MeshTriangle> *triangles = nullptr; // this reference is managed externally
	std::vector<TreeNode> nodes; // list of nodes
	s32 root = -1; // index of the root node
};

/*
 * PartialMeshBuffer
 *
 * Attach alternate `Indices` to an existing mesh buffer, to make it possible to use different
 * indices with the same vertex buffer.
 */
class PartialMeshBuffer
{
public:
	PartialMeshBuffer(scene::SMeshBuffer *buffer, std::vector<u16> &&vertex_indices) :
			m_buffer(buffer), m_indices(make_irr<scene::SIndexBuffer>())
	{
		m_indices->Data = std::move(vertex_indices);
		m_indices->setHardwareMappingHint(scene::EHM_STATIC);
	}

	auto *getBuffer() const { return m_buffer; }

	void draw(video::IVideoDriver *driver) const;

private:
	scene::SMeshBuffer *m_buffer;
	irr_ptr<scene::SIndexBuffer> m_indices;
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
	MapBlockMesh(Client *client, MeshMakeData *data, v3pos_t camera_offset);
	~MapBlockMesh();

	// Main animation function, parameters:
	//   faraway: whether the block is far away from the camera (~50 nodes)
	//   time: the global animation time, 0 .. 60 (repeats every minute)
	//   daynight_ratio: 0 .. 1000
	//   crack: -1 .. CRACK_ANIMATION_LENGTH-1 (-1 for off)
	// Returns true if anything has been changed.
	bool animate(bool faraway, float time, int crack, u32 daynight_ratio);

	scene::IMesh *getMesh()
	{
		return m_mesh[0].get();
	}

	scene::IMesh *getMesh(u8 layer)
	{
		return m_mesh[layer].get();
	}

	std::vector<MinimapMapblock*> moveMinimapMapblocks()
	{
		std::vector<MinimapMapblock*> minimap_mapblocks;
		minimap_mapblocks.swap(m_minimap_mapblocks);
		return minimap_mapblocks;
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


// fm:
	/*u32 getUsageTimer() {
		return m_usage_timer;
	}
	void incrementUsageTimer(float dtime) {
		m_usage_timer += dtime;
	}*/

	const int far_step;
	const int lod_step;
	const int fscale;
	//bool no_draw = 0;
	unsigned int timestamp = 0;
	//u32 m_usage_timer = 0;
// ===


	/// Radius of the bounding-sphere, in BS-space.
	f32 getBoundingRadius() const { return m_bounding_radius; }

	/// Center of the bounding-sphere, in BS-space, relative to block pos.
	v3opos_t getBoundingSphereCenter() const { return m_bounding_sphere_center; }

	/// update transparent buffers to render towards the camera
	void updateTransparentBuffers(v3opos_t camera_pos, v3bpos_t block_pos);
	void consolidateTransparentBuffers();

	/// get the list of transparent buffers
	const std::vector<PartialMeshBuffer> &getTransparentBuffers() const
	{
		return this->m_transparent_buffers;
	}

private:
	struct AnimationInfo {
		int frame; // last animation frame
		int frame_offset;
		TileLayer tile;
	};

	irr_ptr<scene::IMesh> m_mesh[MAX_TILE_LAYERS];
	std::vector<MinimapMapblock*> m_minimap_mapblocks;
	ITextureSource *m_tsrc;
	IShaderSource *m_shdrsrc;

	f32 m_bounding_radius;
	v3opos_t m_bounding_sphere_center;

	bool m_enable_shaders;

	// Must animate() be called before rendering?
	bool m_has_animation;
	int m_animation_force_timer;

	// Animation info: cracks
	// Last crack value passed to animate()
	int m_last_crack;
	// Maps mesh and mesh buffer (i.e. material) indices to base texture names
	std::map<std::pair<u8, u32>, std::string> m_crack_materials;

	// Animation info: texture animation
	// Maps mesh and mesh buffer indices to TileSpecs
	// Keys are pairs of (mesh index, buffer index in the mesh)
	std::map<std::pair<u8, u32>, AnimationInfo> m_animation_info;

	// Animation info: day/night transitions
	// Last daynight_ratio value passed to animate()
	u32 m_last_daynight_ratio;
	// For each mesh and mesh buffer, stores pre-baked colors
	// of sunlit vertices
	// Keys are pairs of (mesh index, buffer index in the mesh)
	std::map<std::pair<u8, u32>, std::map<u32, video::SColor > > m_daynight_diffs;

	// list of all semitransparent triangles in the mapblock
	std::vector<MeshTriangle> m_transparent_triangles;
	// Binary Space Partitioning tree for the block
	MapBlockBspTree m_bsp_tree;
	// Ordered list of references to parts of transparent buffers to draw
	std::vector<PartialMeshBuffer> m_transparent_buffers;
	// Is m_transparent_buffers currently in consolidated form?
	bool m_transparent_buffers_consolidated = false;
};

/*!
 * Encodes light of a node.
 * The result is not the final color, but a
 * half-baked vertex color.
 * You have to multiply the resulting color
 * with the node's color.
 *
 * \param light the first 8 bits are day light,
 * the last 8 bits are night light
 * \param emissive_light amount of light the surface emits,
 * from 0 to LIGHT_SUN.
 */
video::SColor encode_light(u16 light, u8 emissive_light);

// Compute light at node
u16 getInteriorLight(MapNode n, s32 increment, const NodeDefManager *ndef);
u16 getFaceLight(MapNode n, MapNode n2, const NodeDefManager *ndef);
u16 getSmoothLightSolid(const v3pos_t &p, const v3pos_t &face_dir, const v3pos_t &corner, MeshMakeData *data);
u16 getSmoothLightTransparent(const v3pos_t &p, const v3pos_t &corner, MeshMakeData *data);

/*!
 * Returns the sunlight's color from the current
 * day-night ratio.
 */
void get_sunlight_color(video::SColorf *sunlight, u32 daynight_ratio);

/*!
 * Gives the final  SColor shown on screen.
 *
 * \param result output color
 * \param light first 8 bits are day light, second 8 bits are
 * night light
 */
void final_color_blend(video::SColor *result,
		u16 light, u32 daynight_ratio);

/*!
 * Gives the final  SColor shown on screen.
 *
 * \param result output color
 * \param data the half-baked vertex color
 * \param dayLight color of the sunlight
 */
void final_color_blend(video::SColor *result,
		const video::SColor &data, const video::SColorf &dayLight);

// Retrieves the TileSpec of a face of a node
// Adds MATERIAL_FLAG_CRACK if the node is cracked
// TileSpec should be passed as reference due to the underlying TileFrame and its vector
// TileFrame vector copy cost very much to client
void getNodeTileN(MapNode mn, const v3pos_t &p, u8 tileindex, MeshMakeData *data, TileSpec &tile);
void getNodeTile(MapNode mn, const v3pos_t &p, const v3pos_t &dir, MeshMakeData *data, TileSpec &tile);

/// Return bitset of the sides of the mesh that consist of solid nodes only
/// Bits:
/// 0 0 -Z +Z -X +X -Y +Y
u8 get_solid_sides(MeshMakeData *data);
