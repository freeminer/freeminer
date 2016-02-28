/*
mesh.cpp
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

#include "mesh.h"
#include "debug.h"
#include "log.h"
#include "irrMap.h"
#include <iostream>
#include <IAnimatedMesh.h>
#include <SAnimatedMesh.h>
#include "client/tile.h"

// In Irrlicht 1.8 the signature of ITexture::lock was changed from
// (bool, u32) to (E_TEXTURE_LOCK_MODE, u32).
#if IRRLICHT_VERSION_MAJOR == 1 && IRRLICHT_VERSION_MINOR <= 7
#define MY_ETLM_READ_ONLY true
#else
#define MY_ETLM_READ_ONLY video::ETLM_READ_ONLY
#endif

static void applyFacesShading(video::SColor& color, float factor)
{
	color.setRed(core::clamp(core::round32(color.getRed()*factor), 0, 255));
	color.setGreen(core::clamp(core::round32(color.getGreen()*factor), 0, 255));
	color.setBlue(core::clamp(core::round32(color.getBlue()*factor), 0, 255));
}

scene::IAnimatedMesh* createCubeMesh(v3f scale)
{
	video::SColor c(255,255,255,255);
	video::S3DVertex vertices[24] =
	{
		// Up
		video::S3DVertex(-0.5,+0.5,-0.5, 0,1,0, c, 0,1),
		video::S3DVertex(-0.5,+0.5,+0.5, 0,1,0, c, 0,0),
		video::S3DVertex(+0.5,+0.5,+0.5, 0,1,0, c, 1,0),
		video::S3DVertex(+0.5,+0.5,-0.5, 0,1,0, c, 1,1),
		// Down
		video::S3DVertex(-0.5,-0.5,-0.5, 0,-1,0, c, 0,0),
		video::S3DVertex(+0.5,-0.5,-0.5, 0,-1,0, c, 1,0),
		video::S3DVertex(+0.5,-0.5,+0.5, 0,-1,0, c, 1,1),
		video::S3DVertex(-0.5,-0.5,+0.5, 0,-1,0, c, 0,1),
		// Right
		video::S3DVertex(+0.5,-0.5,-0.5, 1,0,0, c, 0,1),
		video::S3DVertex(+0.5,+0.5,-0.5, 1,0,0, c, 0,0),
		video::S3DVertex(+0.5,+0.5,+0.5, 1,0,0, c, 1,0),
		video::S3DVertex(+0.5,-0.5,+0.5, 1,0,0, c, 1,1),
		// Left
		video::S3DVertex(-0.5,-0.5,-0.5, -1,0,0, c, 1,1),
		video::S3DVertex(-0.5,-0.5,+0.5, -1,0,0, c, 0,1),
		video::S3DVertex(-0.5,+0.5,+0.5, -1,0,0, c, 0,0),
		video::S3DVertex(-0.5,+0.5,-0.5, -1,0,0, c, 1,0),
		// Back
		video::S3DVertex(-0.5,-0.5,+0.5, 0,0,1, c, 1,1),
		video::S3DVertex(+0.5,-0.5,+0.5, 0,0,1, c, 0,1),
		video::S3DVertex(+0.5,+0.5,+0.5, 0,0,1, c, 0,0),
		video::S3DVertex(-0.5,+0.5,+0.5, 0,0,1, c, 1,0),
		// Front
		video::S3DVertex(-0.5,-0.5,-0.5, 0,0,-1, c, 0,1),
		video::S3DVertex(-0.5,+0.5,-0.5, 0,0,-1, c, 0,0),
		video::S3DVertex(+0.5,+0.5,-0.5, 0,0,-1, c, 1,0),
		video::S3DVertex(+0.5,-0.5,-0.5, 0,0,-1, c, 1,1),
	};

	u16 indices[6] = {0,1,2,2,3,0};

	scene::SMesh *mesh = new scene::SMesh();
	for (u32 i=0; i<6; ++i)
	{
		scene::IMeshBuffer *buf = new scene::SMeshBuffer();
		buf->append(vertices + 4 * i, 4, indices, 6);
		// Set default material
		buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
		buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
		buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		// Add mesh buffer to mesh
		mesh->addMeshBuffer(buf);
		buf->drop();
	}

	scene::SAnimatedMesh *anim_mesh = new scene::SAnimatedMesh(mesh);
	mesh->drop();
	scaleMesh(anim_mesh, scale);  // also recalculates bounding box
	return anim_mesh;
}

void scaleMesh(scene::IMesh *mesh, v3f scale)
{
	if (mesh == NULL)
		return;

	aabb3f bbox;
	bbox.reset(0, 0, 0);

	u32 mc = mesh->getMeshBufferCount();
	for (u32 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++)
			((video::S3DVertex *)(vertices + i * stride))->Pos *= scale;

		buf->recalculateBoundingBox();

		// calculate total bounding box
		if (j == 0)
			bbox = buf->getBoundingBox();
		else
			bbox.addInternalBox(buf->getBoundingBox());
	}
	mesh->setBoundingBox(bbox);
}

void translateMesh(scene::IMesh *mesh, v3f vec)
{
	if (mesh == NULL)
		return;

	aabb3f bbox;
	bbox.reset(0, 0, 0);

	u32 mc = mesh->getMeshBufferCount();
	for (u32 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++)
			((video::S3DVertex *)(vertices + i * stride))->Pos += vec;

		buf->recalculateBoundingBox();

		// calculate total bounding box
		if (j == 0)
			bbox = buf->getBoundingBox();
		else
			bbox.addInternalBox(buf->getBoundingBox());
	}
	mesh->setBoundingBox(bbox);
}


void setMeshColor(scene::IMesh *mesh, const video::SColor &color)
{
	if (mesh == NULL)
		return;

	u32 mc = mesh->getMeshBufferCount();
	for (u32 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++)
			((video::S3DVertex *)(vertices + i * stride))->Color = color;
	}
}

void shadeMeshFaces(scene::IMesh *mesh)
{
	if (mesh == NULL)
		return;

	u32 mc = mesh->getMeshBufferCount();
	for (u32 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++) {
			video::S3DVertex *vertex = (video::S3DVertex *)(vertices + i * stride);
			video::SColor &vc = vertex->Color;
			if (vertex->Normal.Y < -0.5) {
				applyFacesShading (vc, 0.447213);
			} else if (vertex->Normal.Z > 0.5) {
				applyFacesShading (vc, 0.670820);
			} else if (vertex->Normal.Z < -0.5) {
				applyFacesShading (vc, 0.670820);
			} else if (vertex->Normal.X > 0.5) {
				applyFacesShading (vc, 0.836660);
			} else if (vertex->Normal.X < -0.5) {
				applyFacesShading (vc, 0.836660);
			}
		}
	}
}

void setMeshColorByNormalXYZ(scene::IMesh *mesh,
		const video::SColor &colorX,
		const video::SColor &colorY,
		const video::SColor &colorZ)
{
	if (mesh == NULL)
		return;

	u16 mc = mesh->getMeshBufferCount();
	for (u16 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++) {
			video::S3DVertex *vertex = (video::S3DVertex *)(vertices + i * stride);
			f32 x = fabs(vertex->Normal.X);
			f32 y = fabs(vertex->Normal.Y);
			f32 z = fabs(vertex->Normal.Z);
			if (x >= y && x >= z)
				vertex->Color = colorX;
			else if (y >= z)
				vertex->Color = colorY;
			else
				vertex->Color = colorZ;

		}
	}
}

void rotateMeshXYby(scene::IMesh *mesh, f64 degrees)
{
	u16 mc = mesh->getMeshBufferCount();
	for (u16 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++)
			((video::S3DVertex *)(vertices + i * stride))->Pos.rotateXYBy(degrees);
	}
}

void rotateMeshXZby(scene::IMesh *mesh, f64 degrees)
{
	u16 mc = mesh->getMeshBufferCount();
	for (u16 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++)
			((video::S3DVertex *)(vertices + i * stride))->Pos.rotateXZBy(degrees);
	}
}

void rotateMeshYZby(scene::IMesh *mesh, f64 degrees)
{
	u16 mc = mesh->getMeshBufferCount();
	for (u16 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++)
			((video::S3DVertex *)(vertices + i * stride))->Pos.rotateYZBy(degrees);
	}
}

void rotateMeshBy6dFacedir(scene::IMesh *mesh, int facedir)
{
	int axisdir = facedir >> 2;
	facedir &= 0x03;

	u16 mc = mesh->getMeshBufferCount();
	for (u16 j = 0; j < mc; j++) {
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);
		const u32 stride = getVertexPitchFromType(buf->getVertexType());
		u32 vertex_count = buf->getVertexCount();
		u8 *vertices = (u8 *)buf->getVertices();
		for (u32 i = 0; i < vertex_count; i++) {
			video::S3DVertex *vertex = (video::S3DVertex *)(vertices + i * stride);
			switch (axisdir) {
				case 0:
					if (facedir == 1)
						vertex->Pos.rotateXZBy(-90);
					else if (facedir == 2)
						vertex->Pos.rotateXZBy(180);
					else if (facedir == 3)
						vertex->Pos.rotateXZBy(90);
					break;
				case 1: // z+
					vertex->Pos.rotateYZBy(90);
					if (facedir == 1)
						vertex->Pos.rotateXYBy(90);
					else if (facedir == 2)
						vertex->Pos.rotateXYBy(180);
					else if (facedir == 3)
						vertex->Pos.rotateXYBy(-90);
					break;
				case 2: //z-
					vertex->Pos.rotateYZBy(-90);
					if (facedir == 1)
						vertex->Pos.rotateXYBy(-90);
					else if (facedir == 2)
						vertex->Pos.rotateXYBy(180);
					else if (facedir == 3)
						vertex->Pos.rotateXYBy(90);
					break;
				case 3:  //x+
					vertex->Pos.rotateXYBy(-90);
					if (facedir == 1)
						vertex->Pos.rotateYZBy(90);
					else if (facedir == 2)
						vertex->Pos.rotateYZBy(180);
					else if (facedir == 3)
						vertex->Pos.rotateYZBy(-90);
					break;
				case 4:  //x-
					vertex->Pos.rotateXYBy(90);
					if (facedir == 1)
						vertex->Pos.rotateYZBy(-90);
					else if (facedir == 2)
						vertex->Pos.rotateYZBy(180);
					else if (facedir == 3)
						vertex->Pos.rotateYZBy(90);
					break;
				case 5:
					vertex->Pos.rotateXYBy(-180);
					if (facedir == 1)
						vertex->Pos.rotateXZBy(90);
					else if (facedir == 2)
						vertex->Pos.rotateXZBy(180);
					else if (facedir == 3)
						vertex->Pos.rotateXZBy(-90);
					break;
				default:
					break;
			}
		}
	}
}

void recalculateBoundingBox(scene::IMesh *src_mesh)
{
	aabb3f bbox;
	bbox.reset(0,0,0);
	for (u16 j = 0; j < src_mesh->getMeshBufferCount(); j++) {
		scene::IMeshBuffer *buf = src_mesh->getMeshBuffer(j);
		buf->recalculateBoundingBox();
		if (j == 0)
			bbox = buf->getBoundingBox();
		else
			bbox.addInternalBox(buf->getBoundingBox());
	}
	src_mesh->setBoundingBox(bbox);
}

scene::IMesh* cloneMesh(scene::IMesh *src_mesh)
{
	scene::SMesh* dst_mesh = new scene::SMesh();
	for (u16 j = 0; j < src_mesh->getMeshBufferCount(); j++) {
		scene::IMeshBuffer *buf = src_mesh->getMeshBuffer(j);
		switch (buf->getVertexType()) {
			case video::EVT_STANDARD: {
				video::S3DVertex *v =
					(video::S3DVertex *) buf->getVertices();
				u16 *indices = (u16*)buf->getIndices();
				scene::SMeshBuffer *temp_buf = new scene::SMeshBuffer();
				temp_buf->append(v, buf->getVertexCount(),
					indices, buf->getIndexCount());
				dst_mesh->addMeshBuffer(temp_buf);
				temp_buf->drop();
				break;
			}
			case video::EVT_2TCOORDS: {
				video::S3DVertex2TCoords *v =
					(video::S3DVertex2TCoords *) buf->getVertices();
				u16 *indices = (u16*)buf->getIndices();
				scene::SMeshBufferTangents *temp_buf =
					new scene::SMeshBufferTangents();
				temp_buf->append(v, buf->getVertexCount(),
					indices, buf->getIndexCount());
				dst_mesh->addMeshBuffer(temp_buf);
				temp_buf->drop();
				break;
			}
			case video::EVT_TANGENTS: {
				video::S3DVertexTangents *v =
					(video::S3DVertexTangents *) buf->getVertices();
				u16 *indices = (u16*)buf->getIndices();
				scene::SMeshBufferTangents *temp_buf =
					new scene::SMeshBufferTangents();
				temp_buf->append(v, buf->getVertexCount(),
					indices, buf->getIndexCount());
				dst_mesh->addMeshBuffer(temp_buf);
				temp_buf->drop();
				break;
			}
		}
	}
	return dst_mesh;
}

scene::IMesh* convertNodeboxesToMesh(const std::vector<aabb3f> &boxes,
		const f32 *uv_coords, float expand)
{
	scene::SMesh* dst_mesh = new scene::SMesh();

	for (u16 j = 0; j < 6; j++)
	{
		scene::IMeshBuffer *buf = new scene::SMeshBuffer();
		buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
		buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
		dst_mesh->addMeshBuffer(buf);
		buf->drop();
	}

	video::SColor c(255,255,255,255);	

	for (std::vector<aabb3f>::const_iterator
			i = boxes.begin();
			i != boxes.end(); ++i)
	{
		aabb3f box = *i;
		box.repair();

		box.MinEdge.X -= expand;
		box.MinEdge.Y -= expand;
		box.MinEdge.Z -= expand;
		box.MaxEdge.X += expand;
		box.MaxEdge.Y += expand;
		box.MaxEdge.Z += expand;

		// Compute texture UV coords
		f32 tx1 = (box.MinEdge.X / BS) + 0.5;
		f32 ty1 = (box.MinEdge.Y / BS) + 0.5;
		f32 tz1 = (box.MinEdge.Z / BS) + 0.5;
		f32 tx2 = (box.MaxEdge.X / BS) + 0.5;
		f32 ty2 = (box.MaxEdge.Y / BS) + 0.5;
		f32 tz2 = (box.MaxEdge.Z / BS) + 0.5;

		f32 txc_default[24] = {
			// up
			tx1, 1 - tz2, tx2, 1 - tz1,
			// down
			tx1, tz1, tx2, tz2,
			// right
			tz1, 1 - ty2, tz2, 1 - ty1,
			// left
			1 - tz2, 1 - ty2, 1 - tz1, 1 - ty1,
			// back
			1 - tx2, 1 - ty2, 1 - tx1, 1 - ty1,
			// front
			tx1, 1 - ty2, tx2, 1 - ty1,
		};

		// use default texture UV mapping if not provided
		const f32 *txc = uv_coords ? uv_coords : txc_default;

		v3f min = box.MinEdge;
		v3f max = box.MaxEdge;

		video::S3DVertex vertices[24] =
		{
			// up
			video::S3DVertex(min.X,max.Y,max.Z, 0,1,0, c, txc[0],txc[1]),
			video::S3DVertex(max.X,max.Y,max.Z, 0,1,0, c, txc[2],txc[1]),
			video::S3DVertex(max.X,max.Y,min.Z, 0,1,0, c, txc[2],txc[3]),
			video::S3DVertex(min.X,max.Y,min.Z, 0,1,0, c, txc[0],txc[3]),
			// down
			video::S3DVertex(min.X,min.Y,min.Z, 0,-1,0, c, txc[4],txc[5]),
			video::S3DVertex(max.X,min.Y,min.Z, 0,-1,0, c, txc[6],txc[5]),
			video::S3DVertex(max.X,min.Y,max.Z, 0,-1,0, c, txc[6],txc[7]),
			video::S3DVertex(min.X,min.Y,max.Z, 0,-1,0, c, txc[4],txc[7]),
			// right
			video::S3DVertex(max.X,max.Y,min.Z, 1,0,0, c, txc[ 8],txc[9]),
			video::S3DVertex(max.X,max.Y,max.Z, 1,0,0, c, txc[10],txc[9]),
			video::S3DVertex(max.X,min.Y,max.Z, 1,0,0, c, txc[10],txc[11]),
			video::S3DVertex(max.X,min.Y,min.Z, 1,0,0, c, txc[ 8],txc[11]),
			// left
			video::S3DVertex(min.X,max.Y,max.Z, -1,0,0, c, txc[12],txc[13]),
			video::S3DVertex(min.X,max.Y,min.Z, -1,0,0, c, txc[14],txc[13]),
			video::S3DVertex(min.X,min.Y,min.Z, -1,0,0, c, txc[14],txc[15]),
			video::S3DVertex(min.X,min.Y,max.Z, -1,0,0, c, txc[12],txc[15]),
			// back
			video::S3DVertex(max.X,max.Y,max.Z, 0,0,1, c, txc[16],txc[17]),
			video::S3DVertex(min.X,max.Y,max.Z, 0,0,1, c, txc[18],txc[17]),
			video::S3DVertex(min.X,min.Y,max.Z, 0,0,1, c, txc[18],txc[19]),
			video::S3DVertex(max.X,min.Y,max.Z, 0,0,1, c, txc[16],txc[19]),
			// front
			video::S3DVertex(min.X,max.Y,min.Z, 0,0,-1, c, txc[20],txc[21]),
			video::S3DVertex(max.X,max.Y,min.Z, 0,0,-1, c, txc[22],txc[21]),
			video::S3DVertex(max.X,min.Y,min.Z, 0,0,-1, c, txc[22],txc[23]),
			video::S3DVertex(min.X,min.Y,min.Z, 0,0,-1, c, txc[20],txc[23]),
		};

		u16 indices[] = {0,1,2,2,3,0};

		for(u16 j = 0; j < 24; j += 4)
		{
			scene::IMeshBuffer *buf = dst_mesh->getMeshBuffer(j / 4);
			buf->append(vertices + j, 4, indices, 6);
		}
	}
	return dst_mesh;					
}

struct vcache
{
	core::array<u32> tris;
	float score;
	s16 cachepos;
	u16 NumActiveTris;
};

struct tcache
{
	u16 ind[3];
	float score;
	bool drawn;
};

const u16 cachesize = 32;

float FindVertexScore(vcache *v)
{
	const float CacheDecayPower = 1.5f;
	const float LastTriScore = 0.75f;
	const float ValenceBoostScale = 2.0f;
	const float ValenceBoostPower = 0.5f;
	const float MaxSizeVertexCache = 32.0f;

	if (v->NumActiveTris == 0)
	{
		// No tri needs this vertex!
		return -1.0f;
	}

	float Score = 0.0f;
	int CachePosition = v->cachepos;
	if (CachePosition < 0)
	{
		// Vertex is not in FIFO cache - no score.
	}
	else
	{
		if (CachePosition < 3)
		{
			// This vertex was used in the last triangle,
			// so it has a fixed score.
			Score = LastTriScore;
		}
		else
		{
			// Points for being high in the cache.
			const float Scaler = 1.0f / (MaxSizeVertexCache - 3);
			Score = 1.0f - (CachePosition - 3) * Scaler;
			Score = powf(Score, CacheDecayPower);
		}
	}

	// Bonus points for having a low number of tris still to
	// use the vert, so we get rid of lone verts quickly.
	float ValenceBoost = powf(v->NumActiveTris,
				-ValenceBoostPower);
	Score += ValenceBoostScale * ValenceBoost;

	return Score;
}

/*
	A specialized LRU cache for the Forsyth algorithm.
*/

class f_lru
{

public:
	f_lru(vcache *v, tcache *t): vc(v), tc(t)
	{
		for (u16 i = 0; i < cachesize; i++)
		{
			cache[i] = -1;
		}
	}

	// Adds this vertex index and returns the highest-scoring triangle index
	u32 add(u16 vert, bool updatetris = false)
	{
		bool found = false;

		// Mark existing pos as empty
		for (u16 i = 0; i < cachesize; i++)
		{
			if (cache[i] == vert)
			{
				// Move everything down
				for (u16 j = i; j; j--)
				{
					cache[j] = cache[j - 1];
				}

				found = true;
				break;
			}
		}

		if (!found)
		{
			if (cache[cachesize-1] != -1)
				vc[cache[cachesize-1]].cachepos = -1;

			// Move everything down
			for (u16 i = cachesize - 1; i; i--)
			{
				cache[i] = cache[i - 1];
			}
		}

		cache[0] = vert;

		u32 highest = 0;
		float hiscore = 0;

		if (updatetris)
		{
			// Update cache positions
			for (u16 i = 0; i < cachesize; i++)
			{
				if (cache[i] == -1)
					break;

				vc[cache[i]].cachepos = i;
				vc[cache[i]].score = FindVertexScore(&vc[cache[i]]);
			}

			// Update triangle scores
			for (u16 i = 0; i < cachesize; i++)
			{
				if (cache[i] == -1)
					break;

				const u16 trisize = vc[cache[i]].tris.size();
				for (u16 t = 0; t < trisize; t++)
				{
					tcache *tri = &tc[vc[cache[i]].tris[t]];

					tri->score =
						vc[tri->ind[0]].score +
						vc[tri->ind[1]].score +
						vc[tri->ind[2]].score;

					if (tri->score > hiscore)
					{
						hiscore = tri->score;
						highest = vc[cache[i]].tris[t];
					}
				}
			}
		}

		return highest;
	}

private:
	s32 cache[cachesize];
	vcache *vc;
	tcache *tc;
};

/**
Vertex cache optimization according to the Forsyth paper:
http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html

The function is thread-safe (read: you can optimize several meshes in different threads)

\param mesh Source mesh for the operation.  */
scene::IMesh* createForsythOptimizedMesh(const scene::IMesh *mesh)
{
	if (!mesh)
		return 0;

	scene::SMesh *newmesh = new scene::SMesh();
	newmesh->BoundingBox = mesh->getBoundingBox();

	const u32 mbcount = mesh->getMeshBufferCount();

	for (u32 b = 0; b < mbcount; ++b)
	{
		const scene::IMeshBuffer *mb = mesh->getMeshBuffer(b);

		if (mb->getIndexType() != video::EIT_16BIT)
		{
			//os::Printer::log("Cannot optimize a mesh with 32bit indices", ELL_ERROR);
			newmesh->drop();
			return 0;
		}

		const u32 icount = mb->getIndexCount();
		const u32 tcount = icount / 3;
		const u32 vcount = mb->getVertexCount();
		const u16 *ind = mb->getIndices();

		vcache *vc = new vcache[vcount];
		tcache *tc = new tcache[tcount];

		f_lru lru(vc, tc);

		// init
		for (u16 i = 0; i < vcount; i++)
		{
			vc[i].score = 0;
			vc[i].cachepos = -1;
			vc[i].NumActiveTris = 0;
		}

		// First pass: count how many times a vert is used
		for (u32 i = 0; i < icount; i += 3)
		{
			vc[ind[i]].NumActiveTris++;
			vc[ind[i + 1]].NumActiveTris++;
			vc[ind[i + 2]].NumActiveTris++;

			const u32 tri_ind = i/3;
			tc[tri_ind].ind[0] = ind[i];
			tc[tri_ind].ind[1] = ind[i + 1];
			tc[tri_ind].ind[2] = ind[i + 2];
		}

		// Second pass: list of each triangle
		for (u32 i = 0; i < tcount; i++)
		{
			vc[tc[i].ind[0]].tris.push_back(i);
			vc[tc[i].ind[1]].tris.push_back(i);
			vc[tc[i].ind[2]].tris.push_back(i);

			tc[i].drawn = false;
		}

		// Give initial scores
		for (u16 i = 0; i < vcount; i++)
		{
			vc[i].score = FindVertexScore(&vc[i]);
		}
		for (u32 i = 0; i < tcount; i++)
		{
			tc[i].score =
					vc[tc[i].ind[0]].score +
					vc[tc[i].ind[1]].score +
					vc[tc[i].ind[2]].score;
		}

		switch(mb->getVertexType())
		{
			case video::EVT_STANDARD:
			{
				video::S3DVertex *v = (video::S3DVertex *) mb->getVertices();

				scene::SMeshBuffer *buf = new scene::SMeshBuffer();
				buf->Material = mb->getMaterial();

				buf->Vertices.reallocate(vcount);
				buf->Indices.reallocate(icount);

				core::map<const video::S3DVertex, const u16> sind; // search index for fast operation
				typedef core::map<const video::S3DVertex, const u16>::Node snode;

				// Main algorithm
				u32 highest = 0;
				u32 drawcalls = 0;
				for (;;)
				{
					if (tc[highest].drawn)
					{
						bool found = false;
						float hiscore = 0;
						for (u32 t = 0; t < tcount; t++)
						{
							if (!tc[t].drawn)
							{
								if (tc[t].score > hiscore)
								{
									highest = t;
									hiscore = tc[t].score;
									found = true;
								}
							}
						}
						if (!found)
							break;
					}

					// Output the best triangle
					u16 newind = buf->Vertices.size();

					snode *s = sind.find(v[tc[highest].ind[0]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[0]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[0]], newind);
						newind++;
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					s = sind.find(v[tc[highest].ind[1]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[1]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[1]], newind);
						newind++;
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					s = sind.find(v[tc[highest].ind[2]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[2]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[2]], newind);
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					vc[tc[highest].ind[0]].NumActiveTris--;
					vc[tc[highest].ind[1]].NumActiveTris--;
					vc[tc[highest].ind[2]].NumActiveTris--;

					tc[highest].drawn = true;

					for (u16 j = 0; j < 3; j++)
					{
						vcache *vert = &vc[tc[highest].ind[j]];
						for (u16 t = 0; t < vert->tris.size(); t++)
						{
							if (highest == vert->tris[t])
							{
								vert->tris.erase(t);
								break;
							}
						}
					}

					lru.add(tc[highest].ind[0]);
					lru.add(tc[highest].ind[1]);
					highest = lru.add(tc[highest].ind[2], true);
					drawcalls++;
				}

				buf->setBoundingBox(mb->getBoundingBox());
				newmesh->addMeshBuffer(buf);
				buf->drop();
			}
			break;
			case video::EVT_2TCOORDS:
			{
				video::S3DVertex2TCoords *v = (video::S3DVertex2TCoords *) mb->getVertices();

				scene::SMeshBufferLightMap *buf = new scene::SMeshBufferLightMap();
				buf->Material = mb->getMaterial();

				buf->Vertices.reallocate(vcount);
				buf->Indices.reallocate(icount);

				core::map<const video::S3DVertex2TCoords, const u16> sind; // search index for fast operation
				typedef core::map<const video::S3DVertex2TCoords, const u16>::Node snode;

				// Main algorithm
				u32 highest = 0;
				u32 drawcalls = 0;
				for (;;)
				{
					if (tc[highest].drawn)
					{
						bool found = false;
						float hiscore = 0;
						for (u32 t = 0; t < tcount; t++)
						{
							if (!tc[t].drawn)
							{
								if (tc[t].score > hiscore)
								{
									highest = t;
									hiscore = tc[t].score;
									found = true;
								}
							}
						}
						if (!found)
							break;
					}

					// Output the best triangle
					u16 newind = buf->Vertices.size();

					snode *s = sind.find(v[tc[highest].ind[0]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[0]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[0]], newind);
						newind++;
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					s = sind.find(v[tc[highest].ind[1]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[1]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[1]], newind);
						newind++;
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					s = sind.find(v[tc[highest].ind[2]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[2]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[2]], newind);
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					vc[tc[highest].ind[0]].NumActiveTris--;
					vc[tc[highest].ind[1]].NumActiveTris--;
					vc[tc[highest].ind[2]].NumActiveTris--;

					tc[highest].drawn = true;

					for (u16 j = 0; j < 3; j++)
					{
						vcache *vert = &vc[tc[highest].ind[j]];
						for (u16 t = 0; t < vert->tris.size(); t++)
						{
							if (highest == vert->tris[t])
							{
								vert->tris.erase(t);
								break;
							}
						}
					}

					lru.add(tc[highest].ind[0]);
					lru.add(tc[highest].ind[1]);
					highest = lru.add(tc[highest].ind[2]);
					drawcalls++;
				}

				buf->setBoundingBox(mb->getBoundingBox());
				newmesh->addMeshBuffer(buf);
				buf->drop();

			}
			break;
			case video::EVT_TANGENTS:
			{
				video::S3DVertexTangents *v = (video::S3DVertexTangents *) mb->getVertices();

				scene::SMeshBufferTangents *buf = new scene::SMeshBufferTangents();
				buf->Material = mb->getMaterial();

				buf->Vertices.reallocate(vcount);
				buf->Indices.reallocate(icount);

				core::map<const video::S3DVertexTangents, const u16> sind; // search index for fast operation
				typedef core::map<const video::S3DVertexTangents, const u16>::Node snode;

				// Main algorithm
				u32 highest = 0;
				u32 drawcalls = 0;
				for (;;)
				{
					if (tc[highest].drawn)
					{
						bool found = false;
						float hiscore = 0;
						for (u32 t = 0; t < tcount; t++)
						{
							if (!tc[t].drawn)
							{
								if (tc[t].score > hiscore)
								{
									highest = t;
									hiscore = tc[t].score;
									found = true;
								}
							}
						}
						if (!found)
							break;
					}

					// Output the best triangle
					u16 newind = buf->Vertices.size();

					snode *s = sind.find(v[tc[highest].ind[0]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[0]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[0]], newind);
						newind++;
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					s = sind.find(v[tc[highest].ind[1]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[1]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[1]], newind);
						newind++;
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					s = sind.find(v[tc[highest].ind[2]]);

					if (!s)
					{
						buf->Vertices.push_back(v[tc[highest].ind[2]]);
						buf->Indices.push_back(newind);
						sind.insert(v[tc[highest].ind[2]], newind);
					}
					else
					{
						buf->Indices.push_back(s->getValue());
					}

					vc[tc[highest].ind[0]].NumActiveTris--;
					vc[tc[highest].ind[1]].NumActiveTris--;
					vc[tc[highest].ind[2]].NumActiveTris--;

					tc[highest].drawn = true;

					for (u16 j = 0; j < 3; j++)
					{
						vcache *vert = &vc[tc[highest].ind[j]];
						for (u16 t = 0; t < vert->tris.size(); t++)
						{
							if (highest == vert->tris[t])
							{
								vert->tris.erase(t);
								break;
							}
						}
					}

					lru.add(tc[highest].ind[0]);
					lru.add(tc[highest].ind[1]);
					highest = lru.add(tc[highest].ind[2]);
					drawcalls++;
				}

				buf->setBoundingBox(mb->getBoundingBox());
				newmesh->addMeshBuffer(buf);
				buf->drop();
			}
			break;
		}

		delete [] vc;
		delete [] tc;

	} // for each meshbuffer

	return newmesh;
}
