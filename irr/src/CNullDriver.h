// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IVideoDriver.h"
#include "IFileSystem.h"
#include "IGPUProgrammingServices.h"
#include "irrArray.h"
#include "irrString.h"
#include "IAttributes.h"
#include "IMesh.h"
#include "IMeshBuffer.h"
#include "IMeshSceneNode.h"
#include "CFPSCounter.h"
#include "S3DVertex.h"
#include "SVertexIndex.h"
#include "SExposedVideoData.h"
#include <list>

namespace irr
{
namespace io
{
class IWriteFile;
class IReadFile;
} // end namespace io
namespace video
{
class IImageLoader;
class IImageWriter;

class CNullDriver : public IVideoDriver, public IGPUProgrammingServices
{
public:
	//! constructor
	CNullDriver(io::IFileSystem *io, const core::dimension2d<u32> &screenSize);

	//! destructor
	virtual ~CNullDriver();

	virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0), f32 clearDepth = 1.f, u8 clearStencil = 0,
			const SExposedVideoData &videoData = SExposedVideoData(), core::rect<s32> *sourceRect = 0) override;

	bool endScene() override;

	//! Disable a feature of the driver.
	void disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag = true) override;

	//! queries the features of the driver, returns true if feature is available
	bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const override;

	//! Get attributes of the actual video driver
	const io::IAttributes &getDriverAttributes() const override;

	//! sets transformation
	void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat) override;

	//! Retrieve the number of image loaders
	u32 getImageLoaderCount() const override;

	//! Retrieve the given image loader
	IImageLoader *getImageLoader(u32 n) override;

	//! Retrieve the number of image writers
	u32 getImageWriterCount() const override;

	//! Retrieve the given image writer
	IImageWriter *getImageWriter(u32 n) override;

	//! sets a material
	void setMaterial(const SMaterial &material) override;

	//! loads a Texture
	ITexture *getTexture(const io::path &filename) override;

	//! loads a Texture
	ITexture *getTexture(io::IReadFile *file) override;

	//! Returns amount of textures currently loaded
	u32 getTextureCount() const override;

	ITexture *addTexture(const core::dimension2d<u32> &size, const io::path &name, ECOLOR_FORMAT format = ECF_A8R8G8B8) override;

	ITexture *addTexture(const io::path &name, IImage *image) override;

	virtual ITexture *addTextureCubemap(const io::path &name, IImage *imagePosX, IImage *imageNegX, IImage *imagePosY,
			IImage *imageNegY, IImage *imagePosZ, IImage *imageNegZ) override;

	ITexture *addTextureCubemap(const irr::u32 sideLen, const io::path &name, ECOLOR_FORMAT format = ECF_A8R8G8B8) override;

	virtual bool setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) override;

	virtual bool setRenderTarget(ITexture *texture, u16 clearFlag, SColor clearColor = SColor(255, 0, 0, 0),
			f32 clearDepth = 1.f, u8 clearStencil = 0) override;

	//! sets a viewport
	void setViewPort(const core::rect<s32> &area) override;

	//! gets the area of the current viewport
	const core::rect<s32> &getViewPort() const override;

	//! draws a vertex primitive list
	virtual void drawVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primitiveCount,
			E_VERTEX_TYPE vType = EVT_STANDARD, scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES,
			E_INDEX_TYPE iType = EIT_16BIT) override;

	//! draws a vertex primitive list in 2d
	virtual void draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount,
			const void *indexList, u32 primitiveCount,
			E_VERTEX_TYPE vType = EVT_STANDARD, scene::E_PRIMITIVE_TYPE pType = scene::EPT_TRIANGLES,
			E_INDEX_TYPE iType = EIT_16BIT) override;

	//! Draws a 3d line.
	virtual void draw3DLine(const core::vector3df &start,
			const core::vector3df &end, SColor color = SColor(255, 255, 255, 255)) override;

	//! Draws a 3d axis aligned box.
	virtual void draw3DBox(const core::aabbox3d<f32> &box,
			SColor color = SColor(255, 255, 255, 255)) override;

	//! draws an 2d image
	void draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos, bool useAlphaChannelOfTexture) override;

	//! Draws a set of 2d images, using a color and the alpha channel of the texture.
	/** All drawings are clipped against clipRect (if != 0).
	The subtextures are defined by the array of sourceRects and are
	positioned using the array of positions.
	\param texture Texture to be drawn.
	\param pos Array of upper left 2d destinations where the images
	will be drawn.
	\param sourceRects Source rectangles of the image.
	\param clipRect Pointer to rectangle on the screen where the
	images are clipped to.
	If this pointer is 0 then the image is not clipped.
	\param color Color with which the image is drawn.
	Note that the alpha component is used. If alpha is other than
	255, the image will be transparent.
	\param useAlphaChannelOfTexture: If true, the alpha channel of
	the texture is used to draw the image. */
	virtual void draw2DImageBatch(const video::ITexture *texture,
			const core::array<core::position2d<s32>> &positions,
			const core::array<core::rect<s32>> &sourceRects,
			const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255),
			bool useAlphaChannelOfTexture = false) override;

	//! Draws a 2d image, using a color (if color is other then Color(255,255,255,255)) and the alpha channel of the texture if wanted.
	virtual void draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			SColor color = SColor(255, 255, 255, 255), bool useAlphaChannelOfTexture = false) override;

	//! Draws a part of the texture into the rectangle.
	virtual void draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
			const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect = 0,
			const video::SColor *const colors = 0, bool useAlphaChannelOfTexture = false) override;

	//! Draws a 2d rectangle
	void draw2DRectangle(SColor color, const core::rect<s32> &pos, const core::rect<s32> *clip = 0) override;

	//! Draws a 2d rectangle with a gradient.
	virtual void draw2DRectangle(const core::rect<s32> &pos,
			SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
			const core::rect<s32> *clip = 0) override;

	//! Draws a 2d line.
	virtual void draw2DLine(const core::position2d<s32> &start,
			const core::position2d<s32> &end,
			SColor color = SColor(255, 255, 255, 255)) override;

	virtual void setFog(SColor color = SColor(0, 255, 255, 255),
			E_FOG_TYPE fogType = EFT_FOG_LINEAR,
			f32 start = 50.0f, f32 end = 100.0f, f32 density = 0.01f,
			bool pixelFog = false, bool rangeFog = false) override;

	virtual void getFog(SColor &color, E_FOG_TYPE &fogType,
			f32 &start, f32 &end, f32 &density,
			bool &pixelFog, bool &rangeFog) override;

	//! get color format of the current color buffer
	ECOLOR_FORMAT getColorFormat() const override;

	//! get screen size
	const core::dimension2d<u32> &getScreenSize() const override;

	//! get current render target
	IRenderTarget *getCurrentRenderTarget() const;

	//! get render target size
	const core::dimension2d<u32> &getCurrentRenderTargetSize() const override;

	// get current frames per second value
	s32 getFPS() const override;

	//! returns amount of primitives (mostly triangles) were drawn in the last frame.
	//! very useful method for statistics.
	u32 getPrimitiveCountDrawn(u32 param = 0) const override;

	//! \return Returns the name of the video driver. Example: In case of the DIRECT3D8
	//! driver, it would return "Direct3D8.1".
	const char *getName() const override;

	//! Sets the dynamic ambient light color. The default color is
	//! (0,0,0,0) which means it is dark.
	//! \param color: New color of the ambient light.
	void setAmbientLight(const SColorf &color) override;

	//! Get the global ambient light currently used by the driver
	const SColorf &getAmbientLight() const override;

	//! Adds an external image loader to the engine.
	void addExternalImageLoader(IImageLoader *loader) override;

	//! Adds an external image writer to the engine.
	void addExternalImageWriter(IImageWriter *writer) override;

	//! Removes a texture from the texture cache and deletes it, freeing lot of
	//! memory.
	void removeTexture(ITexture *texture) override;

	//! Removes all texture from the texture cache and deletes them, freeing lot of
	//! memory.
	void removeAllTextures() override;

	//! Creates a render target texture.
	virtual ITexture *addRenderTargetTexture(const core::dimension2d<u32> &size,
			const io::path &name, const ECOLOR_FORMAT format = ECF_UNKNOWN) override;

	//! Creates a render target texture for a cubemap
	ITexture *addRenderTargetTextureCubemap(const irr::u32 sideLen,
			const io::path &name, const ECOLOR_FORMAT format) override;

	//! Creates an 1bit alpha channel of the texture based of an color key.
	void makeColorKeyTexture(video::ITexture *texture, video::SColor color) const override;

	//! Creates an 1bit alpha channel of the texture based of an color key position.
	virtual void makeColorKeyTexture(video::ITexture *texture,
			core::position2d<s32> colorKeyPixelPos) const override;

	//! Returns the maximum amount of primitives (mostly vertices) which
	//! the device is able to render with one drawIndexedTriangleList
	//! call.
	u32 getMaximalPrimitiveCount() const override;

	//! Enables or disables a texture creation flag.
	void setTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag, bool enabled) override;

	//! Returns if a texture creation flag is enabled or disabled.
	bool getTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag) const override;

	IImage *createImageFromFile(const io::path &filename) override;

	IImage *createImageFromFile(io::IReadFile *file) override;

	//! Creates a software image from a byte array.
	/** \param useForeignMemory: If true, the image will use the data pointer
	directly and own it from now on, which means it will also try to delete [] the
	data when the image will be destructed. If false, the memory will by copied. */
	virtual IImage *createImageFromData(ECOLOR_FORMAT format,
			const core::dimension2d<u32> &size, void *data, bool ownForeignMemory = false,
			bool deleteMemory = true) override;

	//! Creates an empty software image.
	IImage *createImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size) override;

	//! Creates a software image from part of a texture.
	virtual IImage *createImage(ITexture *texture,
			const core::position2d<s32> &pos,
			const core::dimension2d<u32> &size) override;

	//! Draws a mesh buffer
	void drawMeshBuffer(const scene::IMeshBuffer *mb) override;

	//! Draws the normals of a mesh buffer
	virtual void drawMeshBufferNormals(const scene::IMeshBuffer *mb, f32 length = 10.f,
			SColor color = 0xffffffff) override;

	//! Check if the driver supports creating textures with the given color format
	bool queryTextureFormat(ECOLOR_FORMAT format) const override
	{
		return false;
	}

protected:
	struct SHWBufferLink
	{
		SHWBufferLink(const scene::IMeshBuffer *_MeshBuffer) :
				MeshBuffer(_MeshBuffer),
				ChangedID_Vertex(0), ChangedID_Index(0),
				Mapped_Vertex(scene::EHM_NEVER), Mapped_Index(scene::EHM_NEVER)
		{
			if (MeshBuffer) {
				MeshBuffer->grab();
				MeshBuffer->setHWBuffer(reinterpret_cast<void *>(this));
			}
		}

		virtual ~SHWBufferLink()
		{
			if (MeshBuffer) {
				MeshBuffer->setHWBuffer(NULL);
				MeshBuffer->drop();
			}
		}

		const scene::IMeshBuffer *MeshBuffer;
		u32 ChangedID_Vertex;
		u32 ChangedID_Index;
		scene::E_HARDWARE_MAPPING Mapped_Vertex;
		scene::E_HARDWARE_MAPPING Mapped_Index;
		std::list<SHWBufferLink *>::iterator listPosition;
	};

	//! Gets hardware buffer link from a meshbuffer (may create or update buffer)
	virtual SHWBufferLink *getBufferLink(const scene::IMeshBuffer *mb);

	//! updates hardware buffer if needed  (only some drivers can)
	virtual bool updateHardwareBuffer(SHWBufferLink *HWBuffer) { return false; }

	//! Draw hardware buffer (only some drivers can)
	virtual void drawHardwareBuffer(SHWBufferLink *HWBuffer) {}

	//! Delete hardware buffer
	virtual void deleteHardwareBuffer(SHWBufferLink *HWBuffer);

	//! Create hardware buffer from mesh (only some drivers can)
	virtual SHWBufferLink *createHardwareBuffer(const scene::IMeshBuffer *mb) { return 0; }

public:
	//! Remove hardware buffer
	void removeHardwareBuffer(const scene::IMeshBuffer *mb) override;

	//! Remove all hardware buffers
	void removeAllHardwareBuffers() override;

	//! Update all hardware buffers, remove unused ones
	virtual void updateAllHardwareBuffers();

	//! is vbo recommended on this mesh?
	virtual bool isHardwareBufferRecommend(const scene::IMeshBuffer *mb);

	//! Create occlusion query.
	/** Use node for identification and mesh for occlusion test. */
	virtual void addOcclusionQuery(scene::ISceneNode *node,
			const scene::IMesh *mesh = 0) override;

	//! Remove occlusion query.
	void removeOcclusionQuery(scene::ISceneNode *node) override;

	//! Remove all occlusion queries.
	void removeAllOcclusionQueries() override;

	//! Run occlusion query. Draws mesh stored in query.
	/** If the mesh shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	void runOcclusionQuery(scene::ISceneNode *node, bool visible = false) override;

	//! Run all occlusion queries. Draws all meshes stored in queries.
	/** If the meshes shall not be rendered visible, use
	overrideMaterial to disable the color and depth buffer. */
	void runAllOcclusionQueries(bool visible = false) override;

	//! Update occlusion query. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	void updateOcclusionQuery(scene::ISceneNode *node, bool block = true) override;

	//! Update all occlusion queries. Retrieves results from GPU.
	/** If the query shall not block, set the flag to false.
	Update might not occur in this case, though */
	void updateAllOcclusionQueries(bool block = true) override;

	//! Return query result.
	/** Return value is the number of visible pixels/fragments.
	The value is a safe approximation, i.e. can be larger than the
	actual value of pixels. */
	u32 getOcclusionQueryResult(scene::ISceneNode *node) const override;

	//! Create render target.
	IRenderTarget *addRenderTarget() override;

	//! Remove render target.
	void removeRenderTarget(IRenderTarget *renderTarget) override;

	//! Remove all render targets.
	void removeAllRenderTargets() override;

	//! Only used by the engine internally.
	/** Used to notify the driver that the window was resized. */
	void OnResize(const core::dimension2d<u32> &size) override;

	//! Adds a new material renderer to the video device.
	virtual s32 addMaterialRenderer(IMaterialRenderer *renderer,
			const char *name = 0) override;

	//! Returns driver and operating system specific data about the IVideoDriver.
	const SExposedVideoData &getExposedVideoData() override;

	//! Returns type of video driver
	E_DRIVER_TYPE getDriverType() const override;

	//! Returns the transformation set by setTransform
	const core::matrix4 &getTransform(E_TRANSFORMATION_STATE state) const override;

	//! Returns pointer to the IGPUProgrammingServices interface.
	IGPUProgrammingServices *getGPUProgrammingServices() override;

	//! Returns pointer to material renderer or null
	IMaterialRenderer *getMaterialRenderer(u32 idx) const override;

	//! Returns amount of currently available material renderers.
	u32 getMaterialRendererCount() const override;

	//! Returns name of the material renderer
	const char *getMaterialRendererName(u32 idx) const override;

	//! Adds a new material renderer to the VideoDriver, based on a high level shading language.
	virtual s32 addHighLevelShaderMaterial(
			const c8 *vertexShaderProgram,
			const c8 *vertexShaderEntryPointName = 0,
			E_VERTEX_SHADER_TYPE vsCompileTarget = EVST_VS_1_1,
			const c8 *pixelShaderProgram = 0,
			const c8 *pixelShaderEntryPointName = 0,
			E_PIXEL_SHADER_TYPE psCompileTarget = EPST_PS_1_1,
			const c8 *geometryShaderProgram = 0,
			const c8 *geometryShaderEntryPointName = "main",
			E_GEOMETRY_SHADER_TYPE gsCompileTarget = EGST_GS_4_0,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = 0,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0) override;

	virtual s32 addHighLevelShaderMaterialFromFiles(
			const io::path &vertexShaderProgramFile,
			const c8 *vertexShaderEntryPointName = "main",
			E_VERTEX_SHADER_TYPE vsCompileTarget = EVST_VS_1_1,
			const io::path &pixelShaderProgramFile = "",
			const c8 *pixelShaderEntryPointName = "main",
			E_PIXEL_SHADER_TYPE psCompileTarget = EPST_PS_1_1,
			const io::path &geometryShaderProgramFileName = "",
			const c8 *geometryShaderEntryPointName = "main",
			E_GEOMETRY_SHADER_TYPE gsCompileTarget = EGST_GS_4_0,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = 0,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0) override;

	virtual s32 addHighLevelShaderMaterialFromFiles(
			io::IReadFile *vertexShaderProgram,
			const c8 *vertexShaderEntryPointName = "main",
			E_VERTEX_SHADER_TYPE vsCompileTarget = EVST_VS_1_1,
			io::IReadFile *pixelShaderProgram = 0,
			const c8 *pixelShaderEntryPointName = "main",
			E_PIXEL_SHADER_TYPE psCompileTarget = EPST_PS_1_1,
			io::IReadFile *geometryShaderProgram = 0,
			const c8 *geometryShaderEntryPointName = "main",
			E_GEOMETRY_SHADER_TYPE gsCompileTarget = EGST_GS_4_0,
			scene::E_PRIMITIVE_TYPE inType = scene::EPT_TRIANGLES,
			scene::E_PRIMITIVE_TYPE outType = scene::EPT_TRIANGLE_STRIP,
			u32 verticesOut = 0,
			IShaderConstantSetCallBack *callback = 0,
			E_MATERIAL_TYPE baseMaterial = video::EMT_SOLID,
			s32 userData = 0) override;

	virtual void deleteShaderMaterial(s32 material) override;

	//! Returns a pointer to the mesh manipulator.
	scene::IMeshManipulator *getMeshManipulator() override;

	void clearBuffers(u16 flag, SColor color = SColor(255, 0, 0, 0), f32 depth = 1.f, u8 stencil = 0) override;

	//! Returns an image created from the last rendered frame.
	IImage *createScreenShot(video::ECOLOR_FORMAT format = video::ECF_UNKNOWN, video::E_RENDER_TARGET target = video::ERT_FRAME_BUFFER) override;

	//! Writes the provided image to disk file
	bool writeImageToFile(IImage *image, const io::path &filename, u32 param = 0) override;

	//! Writes the provided image to a file.
	bool writeImageToFile(IImage *image, io::IWriteFile *file, u32 param = 0) override;

	//! Sets the name of a material renderer.
	void setMaterialRendererName(u32 idx, const char *name) override;

	//! Swap the material renderers used for certain id's
	void swapMaterialRenderers(u32 idx1, u32 idx2, bool swapNames) override;

	//! looks if the image is already loaded
	video::ITexture *findTexture(const io::path &filename) override;

	//! Set/unset a clipping plane.
	//! There are at least 6 clipping planes available for the user to set at will.
	//! \param index: The plane index. Must be between 0 and MaxUserClipPlanes.
	//! \param plane: The plane itself.
	//! \param enable: If true, enable the clipping plane else disable it.
	bool setClipPlane(u32 index, const core::plane3df &plane, bool enable = false) override;

	//! Enable/disable a clipping plane.
	//! There are at least 6 clipping planes available for the user to set at will.
	//! \param index: The plane index. Must be between 0 and MaxUserClipPlanes.
	//! \param enable: If true, enable the clipping plane else disable it.
	void enableClipPlane(u32 index, bool enable) override;

	//! Returns the graphics card vendor name.
	core::stringc getVendorInfo() override { return "Not available on this driver."; }

	//! Set the minimum number of vertices for which a hw buffer will be created
	/** \param count Number of vertices to set as minimum. */
	void setMinHardwareBufferVertexCount(u32 count) override;

	//! Get the global Material, which might override local materials.
	/** Depending on the enable flags, values from this Material
	are used to override those of local materials of some
	meshbuffer being rendered. */
	SOverrideMaterial &getOverrideMaterial() override;

	//! Get the 2d override material for altering its values
	SMaterial &getMaterial2D() override;

	//! Enable the 2d override material
	void enableMaterial2D(bool enable = true) override;

	//! Only used by the engine internally.
	void setAllowZWriteOnTransparent(bool flag) override
	{
		AllowZWriteOnTransparent = flag;
	}

	//! Returns the maximum texture size supported.
	core::dimension2du getMaxTextureSize() const override;

	//! Used by some SceneNodes to check if a material should be rendered in the transparent render pass
	bool needsTransparentRenderPass(const irr::video::SMaterial &material) const override;

	//! Color conversion convenience function
	/** Convert an image (as array of pixels) from source to destination
	array, thereby converting the color format. The pixel size is
	determined by the color formats.
	\param sP Pointer to source
	\param sF Color format of source
	\param sN Number of pixels to convert, both array must be large enough
	\param dP Pointer to destination
	\param dF Color format of destination
	*/
	virtual void convertColor(const void *sP, ECOLOR_FORMAT sF, s32 sN,
			void *dP, ECOLOR_FORMAT dF) const override;

	bool checkDriverReset() override { return false; }

protected:
	//! deletes all textures
	void deleteAllTextures();

	//! opens the file and loads it into the surface
	ITexture *loadTextureFromFile(io::IReadFile *file, const io::path &hashName = "");

	//! adds a surface, not loaded or created by the Irrlicht Engine
	void addTexture(ITexture *surface);

	virtual ITexture *createDeviceDependentTexture(const io::path &name, IImage *image);

	virtual ITexture *createDeviceDependentTextureCubemap(const io::path &name, const core::array<IImage *> &image);

	//! checks triangle count and print warning if wrong
	bool checkPrimitiveCount(u32 prmcnt) const;

	bool checkImage(IImage *image) const;

	bool checkImage(const core::array<IImage *> &image) const;

	// adds a material renderer and drops it afterwards. To be used for internal creation
	s32 addAndDropMaterialRenderer(IMaterialRenderer *m);

	//! deletes all material renderers
	void deleteMaterialRenders();

	// prints renderer version
	void printVersion();

	inline bool getWriteZBuffer(const SMaterial &material) const
	{
		switch (material.ZWriteEnable) {
		case video::EZW_OFF:
			return false;
		case video::EZW_AUTO:
			return AllowZWriteOnTransparent || !needsTransparentRenderPass(material);
		case video::EZW_ON:
			return true;
		}
		return true; // never should get here, but some compilers don't know and complain
	}

	struct SSurface
	{
		video::ITexture *Surface;

		bool operator<(const SSurface &other) const
		{
			return Surface->getName() < other.Surface->getName();
		}
	};

	struct SMaterialRenderer
	{
		core::stringc Name;
		IMaterialRenderer *Renderer;
	};

	struct SDummyTexture : public ITexture
	{
		SDummyTexture(const io::path &name, E_TEXTURE_TYPE type) :
				ITexture(name, type){};

		void setSize(const core::dimension2d<u32> &size) { Size = OriginalSize = size; }

		void *lock(E_TEXTURE_LOCK_MODE mode = ETLM_READ_WRITE, u32 mipmapLevel = 0, u32 layer = 0, E_TEXTURE_LOCK_FLAGS lockFlags = ETLF_FLIP_Y_UP_RTT) override { return 0; }
		void unlock() override {}
		void regenerateMipMapLevels(void *data = 0, u32 layer = 0) override {}
	};
	core::array<SSurface> Textures;

	struct SOccQuery
	{
		SOccQuery(scene::ISceneNode *node, const scene::IMesh *mesh = 0) :
				Node(node), Mesh(mesh), PID(0), Result(0xffffffff), Run(0xffffffff)
		{
			if (Node)
				Node->grab();
			if (Mesh)
				Mesh->grab();
		}

		SOccQuery(const SOccQuery &other) :
				Node(other.Node), Mesh(other.Mesh), PID(other.PID), Result(other.Result), Run(other.Run)
		{
			if (Node)
				Node->grab();
			if (Mesh)
				Mesh->grab();
		}

		~SOccQuery()
		{
			if (Node)
				Node->drop();
			if (Mesh)
				Mesh->drop();
		}

		SOccQuery &operator=(const SOccQuery &other)
		{
			Node = other.Node;
			Mesh = other.Mesh;
			PID = other.PID;
			Result = other.Result;
			Run = other.Run;
			if (Node)
				Node->grab();
			if (Mesh)
				Mesh->grab();
			return *this;
		}

		bool operator==(const SOccQuery &other) const
		{
			return other.Node == Node;
		}

		scene::ISceneNode *Node;
		const scene::IMesh *Mesh;
		union
		{
			void *PID;
			unsigned int UID;
		};
		u32 Result;
		u32 Run;
	};
	core::array<SOccQuery> OcclusionQueries;

	core::array<IRenderTarget *> RenderTargets;

	// Shared objects used with simplified IVideoDriver::setRenderTarget method with ITexture* param.
	IRenderTarget *SharedRenderTarget;
	core::array<ITexture *> SharedDepthTextures;

	IRenderTarget *CurrentRenderTarget;
	core::dimension2d<u32> CurrentRenderTargetSize;

	core::array<video::IImageLoader *> SurfaceLoader;
	core::array<video::IImageWriter *> SurfaceWriter;
	core::array<SMaterialRenderer> MaterialRenderers;

	std::list<SHWBufferLink *> HWBufferList;

	io::IFileSystem *FileSystem;

	//! mesh manipulator
	scene::IMeshManipulator *MeshManipulator;

	core::rect<s32> ViewPort;
	core::dimension2d<u32> ScreenSize;
	core::matrix4 TransformationMatrix;

	CFPSCounter FPSCounter;

	u32 PrimitivesDrawn;
	u32 MinVertexCountForVBO;

	u32 TextureCreationFlags;

	f32 FogStart;
	f32 FogEnd;
	f32 FogDensity;
	SColor FogColor;
	SExposedVideoData ExposedData;

	io::IAttributes *DriverAttributes;

	SOverrideMaterial OverrideMaterial;
	SMaterial OverrideMaterial2D;
	SMaterial InitMaterial2D;
	bool OverrideMaterial2DEnabled;

	E_FOG_TYPE FogType;
	bool PixelFog;
	bool RangeFog;
	bool AllowZWriteOnTransparent;

	bool FeatureEnabled[video::EVDF_COUNT];

	SColorf AmbientLight;
};

} // end namespace video
} // end namespace irr
