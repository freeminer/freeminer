// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMesh.h"

namespace irr
{
namespace scene
{
//! Interface for an animated mesh.
/** There are already simple implementations of this interface available so
you don't have to implement this interface on your own if you need to:
You might want to use irr::scene::SAnimatedMesh, irr::scene::SMesh,
irr::scene::SMeshBuffer etc. */
class IAnimatedMesh : public IMesh
{
public:
	//! Gets the maximum frame number, 0 if the mesh is static.
	virtual f32 getMaxFrameNumber() const = 0;

	//! Gets the animation speed of the animated mesh.
	/** \return The number of frames per second to play the
	animation with by default. If the amount is 0,
	it is a static, non animated mesh. */
	virtual f32 getAnimationSpeed() const = 0;

	//! Sets the animation speed of the animated mesh.
	/** \param fps Number of frames per second to play the
	animation with by default. If the amount is 0,
	it is not animated. The actual speed is set in the
	scene node the mesh is instantiated in.*/
	virtual void setAnimationSpeed(f32 fps) = 0;

	//! Returns the IMesh interface for a frame.
	/** \param frame: Frame number, >= 0, <= getMaxFrameNumber()
	Linear interpolation is used if this is between two frames.
	\return Returns the animated mesh for the given frame */
	virtual IMesh *getMesh(f32 frame) = 0;

	//! Returns the type of the animated mesh.
	/** In most cases it is not necessary to use this method.
	This is useful for making a safe downcast. For example,
	if getMeshType() returns EAMT_MD2 it's safe to cast the
	IAnimatedMesh to IAnimatedMeshMD2.
	\returns Type of the mesh. */
	E_ANIMATED_MESH_TYPE getMeshType() const override
	{
		return EAMT_UNKNOWN;
	}
};

} // end namespace scene
} // end namespace irr