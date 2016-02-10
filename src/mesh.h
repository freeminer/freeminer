/*
mesh.h
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

#ifndef MESH_HEADER
#define MESH_HEADER

#include "irrlichttypes_extrabloated.h"
#include "nodedef.h"

/*
	Create a new cube mesh.
	Vertices are at (+-scale.X/2, +-scale.Y/2, +-scale.Z/2).

	The resulting mesh has 6 materials (up, down, right, left, back, front)
	which must be defined by the caller.
*/
scene::IAnimatedMesh* createCubeMesh(v3f scale);

/*
	Multiplies each vertex coordinate by the specified scaling factors
	(componentwise vector multiplication).
*/
void scaleMesh(scene::IMesh *mesh, v3f scale);

/*
	Translate each vertex coordinate by the specified vector.
*/
void translateMesh(scene::IMesh *mesh, v3f vec);

/*
	Set a constant color for all vertices in the mesh
*/
void setMeshColor(scene::IMesh *mesh, const video::SColor &color);

/*
	Shade mesh faces according to their normals
*/

void shadeMeshFaces(scene::IMesh *mesh);

/*
	Set the color of all vertices in the mesh.
	For each vertex, determine the largest absolute entry in
	the normal vector, and choose one of colorX, colorY or
	colorZ accordingly.
*/
void setMeshColorByNormalXYZ(scene::IMesh *mesh,
		const video::SColor &colorX,
		const video::SColor &colorY,
		const video::SColor &colorZ);
/*
	Rotate the mesh by 6d facedir value.
	Method only for meshnodes, not suitable for entities.
*/
void rotateMeshBy6dFacedir(scene::IMesh *mesh, int facedir);

/*
	Rotate the mesh around the axis and given angle in degrees.
*/
void rotateMeshXYby (scene::IMesh *mesh, f64 degrees);
void rotateMeshXZby (scene::IMesh *mesh, f64 degrees);
void rotateMeshYZby (scene::IMesh *mesh, f64 degrees); 
 
/*
	Clone the mesh.
*/
scene::IMesh* cloneMesh(scene::IMesh *src_mesh);

/*
	Convert nodeboxes to mesh.
	boxes - set of nodeboxes to be converted into cuboids
	uv_coords[24] - table of texture uv coords for each cuboid face
*/
scene::IMesh* convertNodeboxesToMesh(const std::vector<aabb3f> &boxes,
		const f32 *uv_coords = NULL);

/*
	Update bounding box for a mesh.
*/
void recalculateBoundingBox(scene::IMesh *src_mesh);

/*
	Vertex cache optimization according to the Forsyth paper:
	http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html
	Ported from irrlicht 1.8
*/
scene::IMesh* createForsythOptimizedMesh(const scene::IMesh *mesh);

#endif
