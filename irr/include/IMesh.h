// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "SMaterial.h"
#include "EHardwareBufferFlags.h"

namespace irr
{
namespace scene
{
//! Possible types of meshes.
// Note: Was previously only used in IAnimatedMesh so it still has the "animated" in the name.
// 		 But can now be used for all mesh-types as we need those casts as well.
enum E_ANIMATED_MESH_TYPE
{
	//! Unknown animated mesh type.
	EAMT_UNKNOWN = 0,

	//! Quake 2 MD2 model file
	EAMT_MD2,

	//! Quake 3 MD3 model file
	EAMT_MD3,

	//! Maya .obj static model
	EAMT_OBJ,

	//! Quake 3 .bsp static Map
	EAMT_BSP,

	//! 3D Studio .3ds file
	EAMT_3DS,

	//! My3D Mesh, the file format by Zhuck Dimitry
	EAMT_MY3D,

	//! Pulsar LMTools .lmts file. This Irrlicht loader was written by Jonas Petersen
	EAMT_LMTS,

	//! Cartography Shop .csm file. This loader was created by Saurav Mohapatra.
	EAMT_CSM,

	//! .oct file for Paul Nette's FSRad or from Murphy McCauley's Blender .oct exporter.
	/** The oct file format contains 3D geometry and lightmaps and
	can be loaded directly by Irrlicht */
	EAMT_OCT,

	//! Halflife MDL model file
	EAMT_MDL_HALFLIFE,

	//! generic skinned mesh
	EAMT_SKINNED,

	//! generic non-animated mesh
	EAMT_STATIC
};

class IMeshBuffer;

//! Class which holds the geometry of an object.
/** An IMesh is nothing more than a collection of some mesh buffers
(IMeshBuffer). SMesh is a simple implementation of an IMesh.
A mesh is usually added to an IMeshSceneNode in order to be rendered.
*/
class IMesh : public virtual IReferenceCounted
{
public:
	//! Get the amount of mesh buffers.
	/** \return Amount of mesh buffers (IMeshBuffer) in this mesh. */
	virtual u32 getMeshBufferCount() const = 0;

	//! Get pointer to a mesh buffer.
	/** \param nr: Zero based index of the mesh buffer. The maximum value is
	getMeshBufferCount() - 1;
	\return Pointer to the mesh buffer or 0 if there is no such
	mesh buffer. */
	virtual IMeshBuffer *getMeshBuffer(u32 nr) const = 0;

	//! Get pointer to a mesh buffer which fits a material
	/** \param material: material to search for
	\return Pointer to the mesh buffer or 0 if there is no such
	mesh buffer. */
	virtual IMeshBuffer *getMeshBuffer(const video::SMaterial &material) const = 0;

	//! Get an axis aligned bounding box of the mesh.
	/** \return Bounding box of this mesh. */
	virtual const core::aabbox3d<f32> &getBoundingBox() const = 0;

	//! Set user-defined axis aligned bounding box
	/** \param box New bounding box to use for the mesh. */
	virtual void setBoundingBox(const core::aabbox3df &box) = 0;

	//! Set the hardware mapping hint
	/** This methods allows to define optimization hints for the
	hardware. This enables, e.g., the use of hardware buffers on
	platforms that support this feature. This can lead to noticeable
	performance gains. */
	virtual void setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint, E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX) = 0;

	//! Flag the meshbuffer as changed, reloads hardware buffers
	/** This method has to be called every time the vertices or
	indices have changed. Otherwise, changes won't be updated
	on the GPU in the next render cycle. */
	virtual void setDirty(E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX) = 0;

	//! Returns the type of the meshes.
	/** This is useful for making a safe downcast. For example,
	if getMeshType() returns EAMT_MD2 it's safe to cast the
	IMesh to IAnimatedMeshMD2.
	Note: It's no longer just about animated meshes, that name has just historical reasons.
	\returns Type of the mesh  */
	virtual E_ANIMATED_MESH_TYPE getMeshType() const
	{
		return EAMT_STATIC;
	}
};

} // end namespace scene
} // end namespace irr
