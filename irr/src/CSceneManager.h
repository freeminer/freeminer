// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneManager.h"
#include "ISceneNode.h"
#include "ICursorControl.h"
#include "irrString.h"
#include "irrArray.h"
#include "IMeshLoader.h"
#include "CAttributes.h"

namespace irr
{
namespace io
{
class IFileSystem;
}
namespace scene
{
class IMeshCache;

/*!
	The Scene Manager manages scene nodes, mesh resources, cameras and all the other stuff.
*/
class CSceneManager : public ISceneManager, public ISceneNode
{
public:
	//! constructor
	CSceneManager(video::IVideoDriver *driver, gui::ICursorControl *cursorControl, IMeshCache *cache = 0);

	//! destructor
	virtual ~CSceneManager();

	//! gets an animateable mesh. loads it if needed. returned pointer must not be dropped.
	IAnimatedMesh *getMesh(io::IReadFile *file) override;

	//! Returns an interface to the mesh cache which is shared between all existing scene managers.
	IMeshCache *getMeshCache() override;

	//! returns the video driver
	video::IVideoDriver *getVideoDriver() override;

	//! adds a scene node for rendering an animated mesh model
	virtual IAnimatedMeshSceneNode *addAnimatedMeshSceneNode(IAnimatedMesh *mesh, ISceneNode *parent = 0, s32 id = -1,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f),
			bool alsoAddIfMeshPointerZero = false) override;

	//! adds a scene node for rendering a static mesh
	//! the returned pointer must not be dropped.
	virtual IMeshSceneNode *addMeshSceneNode(IMesh *mesh, ISceneNode *parent = 0, s32 id = -1,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f),
			bool alsoAddIfMeshPointerZero = false) override;

	//! renders the node.
	void render() override;

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	//! registers a node for rendering it at a specific time.
	u32 registerNodeForRendering(ISceneNode *node, E_SCENE_NODE_RENDER_PASS pass = ESNRP_AUTOMATIC) override;

	//! Clear all nodes which are currently registered for rendering
	void clearAllRegisteredNodesForRendering() override;

	//! draws all scene nodes
	void drawAll() override;

	//! Adds a camera scene node to the tree and sets it as active camera.
	//! \param position: Position of the space relative to its parent where the camera will be placed.
	//! \param lookat: Position where the camera will look at. Also known as target.
	//! \param parent: Parent scene node of the camera. Can be null. If the parent moves,
	//! the camera will move too.
	//! \return Pointer to interface to camera
	virtual ICameraSceneNode *addCameraSceneNode(ISceneNode *parent = 0,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &lookat = core::vector3df(0, 0, 100),
			s32 id = -1, bool makeActive = true) override;

	//! Adds a billboard scene node to the scene. A billboard is like a 3d sprite: A 2d element,
	//! which always looks to the camera. It is usually used for things like explosions, fire,
	//! lensflares and things like that.
	virtual IBillboardSceneNode *addBillboardSceneNode(ISceneNode *parent = 0,
			const core::dimension2d<f32> &size = core::dimension2d<f32>(10.0f, 10.0f),
			const core::vector3df &position = core::vector3df(0, 0, 0), s32 id = -1,
			video::SColor shadeTop = 0xFFFFFFFF, video::SColor shadeBottom = 0xFFFFFFFF) override;

	//! Adds a dummy transformation scene node to the scene graph.
	virtual IDummyTransformationSceneNode *addDummyTransformationSceneNode(
			ISceneNode *parent = 0, s32 id = -1) override;

	//! Adds an empty scene node.
	ISceneNode *addEmptySceneNode(ISceneNode *parent, s32 id = -1) override;

	//! Returns the root scene node. This is the scene node which is parent
	//! of all scene nodes. The root scene node is a special scene node which
	//! only exists to manage all scene nodes. It is not rendered and cannot
	//! be removed from the scene.
	//! \return Pointer to the root scene node.
	ISceneNode *getRootSceneNode() override;

	//! Returns the current active camera.
	//! \return The active camera is returned. Note that this can be NULL, if there
	//! was no camera created yet.
	ICameraSceneNode *getActiveCamera() const override;

	//! Sets the active camera. The previous active camera will be deactivated.
	//! \param camera: The new camera which should be active.
	void setActiveCamera(ICameraSceneNode *camera) override;

	//! Adds an external mesh loader.
	void addExternalMeshLoader(IMeshLoader *externalLoader) override;

	//! Returns the number of mesh loaders supported by Irrlicht at this time
	u32 getMeshLoaderCount() const override;

	//! Retrieve the given mesh loader
	IMeshLoader *getMeshLoader(u32 index) const override;

	//! Returns a pointer to the scene collision manager.
	ISceneCollisionManager *getSceneCollisionManager() override;

	//! Returns a pointer to the mesh manipulator.
	IMeshManipulator *getMeshManipulator() override;

	//! Adds a scene node to the deletion queue.
	void addToDeletionQueue(ISceneNode *node) override;

	//! Returns the first scene node with the specified id.
	ISceneNode *getSceneNodeFromId(s32 id, ISceneNode *start = 0) override;

	//! Returns the first scene node with the specified name.
	ISceneNode *getSceneNodeFromName(const c8 *name, ISceneNode *start = 0) override;

	//! Returns the first scene node with the specified type.
	ISceneNode *getSceneNodeFromType(scene::ESCENE_NODE_TYPE type, ISceneNode *start = 0) override;

	//! returns scene nodes by type.
	void getSceneNodesFromType(ESCENE_NODE_TYPE type, core::array<scene::ISceneNode *> &outNodes, ISceneNode *start = 0) override;

	//! Posts an input event to the environment. Usually you do not have to
	//! use this method, it is used by the internal engine.
	bool postEventFromUser(const SEvent &event) override;

	//! Clears the whole scene. All scene nodes are removed.
	void clear() override;

	//! Removes all children of this scene node
	void removeAll() override;

	//! Returns interface to the parameters set in this scene.
	io::IAttributes *getParameters() override;

	//! Returns current render pass.
	E_SCENE_NODE_RENDER_PASS getSceneNodeRenderPass() const override;

	//! Creates a new scene manager.
	ISceneManager *createNewSceneManager(bool cloneContent) override;

	//! Returns type of the scene node
	ESCENE_NODE_TYPE getType() const override { return ESNT_SCENE_MANAGER; }

	//! Get a skinned mesh, which is not available as header-only code
	ISkinnedMesh *createSkinnedMesh() override;

	//! Sets ambient color of the scene
	void setAmbientLight(const video::SColorf &ambientColor) override;

	//! Returns ambient color of the scene
	const video::SColorf &getAmbientLight() const override;

	//! Get current render time.
	E_SCENE_NODE_RENDER_PASS getCurrentRenderPass() const override { return CurrentRenderPass; }

	//! Set current render time.
	void setCurrentRenderPass(E_SCENE_NODE_RENDER_PASS nextPass) override { CurrentRenderPass = nextPass; }

	//! returns if node is culled
	bool isCulled(const ISceneNode *node) const override;

private:
	// load and create a mesh which we know already isn't in the cache and put it in there
	IAnimatedMesh *getUncachedMesh(io::IReadFile *file, const io::path &filename, const io::path &cachename);

	//! clears the deletion list
	void clearDeletionList();

	struct DefaultNodeEntry
	{
		DefaultNodeEntry()
		{
		}

		DefaultNodeEntry(ISceneNode *n) :
				Node(n), TextureValue(0)
		{
			if (n->getMaterialCount())
				TextureValue = (n->getMaterial(0).getTexture(0));
		}

		bool operator<(const DefaultNodeEntry &other) const
		{
			return (TextureValue < other.TextureValue);
		}

		ISceneNode *Node;

	private:
		void *TextureValue;
	};

	//! sort on distance (center) to camera
	struct TransparentNodeEntry
	{
		TransparentNodeEntry()
		{
		}

		TransparentNodeEntry(ISceneNode *n, const core::vector3df &camera) :
				Node(n)
		{
			Distance = Node->getAbsoluteTransformation().getTranslation().getDistanceFromSQ(camera);
		}

		bool operator<(const TransparentNodeEntry &other) const
		{
			return Distance > other.Distance;
		}

		ISceneNode *Node;

	private:
		f64 Distance;
	};

	//! sort on distance (sphere) to camera
	struct DistanceNodeEntry
	{
		DistanceNodeEntry(ISceneNode *n, const core::vector3df &cameraPos) :
				Node(n)
		{
			setNodeAndDistanceFromPosition(n, cameraPos);
		}

		bool operator<(const DistanceNodeEntry &other) const
		{
			return Distance < other.Distance;
		}

		void setNodeAndDistanceFromPosition(ISceneNode *n, const core::vector3df &fromPosition)
		{
			Node = n;
			Distance = Node->getAbsoluteTransformation().getTranslation().getDistanceFromSQ(fromPosition);
			Distance -= Node->getBoundingBox().getExtent().getLengthSQ() * 0.5;
		}

		ISceneNode *Node;

	private:
		f64 Distance;
	};

	//! video driver
	video::IVideoDriver *Driver;

	//! cursor control
	gui::ICursorControl *CursorControl;

	//! collision manager
	ISceneCollisionManager *CollisionManager;

	//! render pass lists
	core::array<ISceneNode *> CameraList;
	core::array<ISceneNode *> SkyBoxList;
	core::array<DefaultNodeEntry> SolidNodeList;
	core::array<TransparentNodeEntry> TransparentNodeList;
	core::array<TransparentNodeEntry> TransparentEffectNodeList;
	core::array<ISceneNode *> GuiNodeList;

	core::array<IMeshLoader *> MeshLoaderList;
	core::array<ISceneNode *> DeletionList;

	//! current active camera
	ICameraSceneNode *ActiveCamera;
	core::vector3df camWorldPos; // Position of camera for transparent nodes.

	video::SColor ShadowColor;
	video::SColorf AmbientLight;

	//! String parameters
	// NOTE: Attributes are slow and should only be used for debug-info and not in release
	io::CAttributes *Parameters;

	//! Mesh cache
	IMeshCache *MeshCache;

	E_SCENE_NODE_RENDER_PASS CurrentRenderPass;
};

} // end namespace video
} // end namespace scene
