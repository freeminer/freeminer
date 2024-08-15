// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COpenGLDriver.h"
#include <cassert>
#include "CNullDriver.h"
#include "IContextManager.h"

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "os.h"

#include "COpenGLCacheHandler.h"
#include "COpenGLMaterialRenderer.h"
#include "COpenGLSLMaterialRenderer.h"

#include "COpenGLCoreTexture.h"
#include "COpenGLCoreRenderTarget.h"

#include "mt_opengl.h"

namespace irr
{
namespace video
{

// Statics variables
const u16 COpenGLDriver::Quad2DIndices[4] = {0, 1, 2, 3};

COpenGLDriver::COpenGLDriver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager) :
		CNullDriver(io, params.WindowSize), COpenGLExtensionHandler(), CacheHandler(0), CurrentRenderMode(ERM_NONE), ResetRenderStates(true),
		Transformation3DChanged(true), AntiAlias(params.AntiAlias), ColorFormat(ECF_R8G8B8), FixedPipelineState(EOFPS_ENABLE), Params(params),
		ContextManager(contextManager)
{
#ifdef _DEBUG
	setDebugName("COpenGLDriver");
#endif
}

bool COpenGLDriver::initDriver()
{
	ContextManager->generateSurface();
	ContextManager->generateContext();
	ExposedData = ContextManager->getContext();
	ContextManager->activateContext(ExposedData, false);
	GL.LoadAllProcedures(ContextManager);

	genericDriverInit();

#if defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_) || defined(_IRR_COMPILE_WITH_X11_DEVICE_)
	extGlSwapInterval(Params.Vsync ? 1 : 0);
#endif

	return true;
}

//! destructor
COpenGLDriver::~COpenGLDriver()
{
	deleteMaterialRenders();

	CacheHandler->getTextureCache().clear();
	// I get a blue screen on my laptop, when I do not delete the
	// textures manually before releasing the dc. Oh how I love this.
	removeAllRenderTargets();
	deleteAllTextures();
	removeAllOcclusionQueries();
	removeAllHardwareBuffers();

	delete CacheHandler;

	if (ContextManager) {
		ContextManager->destroyContext();
		ContextManager->destroySurface();
		ContextManager->terminate();
		ContextManager->drop();
	}
}

// -----------------------------------------------------------------------
// METHODS
// -----------------------------------------------------------------------

bool COpenGLDriver::genericDriverInit()
{
	if (ContextManager)
		ContextManager->grab();

	Name = "OpenGL ";
	Name.append(glGetString(GL_VERSION));
	s32 pos = Name.findNext(' ', 7);
	if (pos != -1)
		Name = Name.subString(0, pos);
	printVersion();

	// print renderer information
	const GLubyte *renderer = glGetString(GL_RENDERER);
	const GLubyte *vendor = glGetString(GL_VENDOR);
	if (renderer && vendor) {
		os::Printer::log(reinterpret_cast<const c8 *>(renderer), reinterpret_cast<const c8 *>(vendor), ELL_INFORMATION);
		VendorName = reinterpret_cast<const c8 *>(vendor);
	}

	u32 i;

	// load extensions
	initExtensions(ContextManager, Params.Stencilbuffer);

	// reset cache handler
	delete CacheHandler;
	CacheHandler = new COpenGLCacheHandler(this);

	if (queryFeature(EVDF_ARB_GLSL)) {
		char buf[32];
		const u32 maj = ShaderLanguageVersion / 100;
		snprintf_irr(buf, 32, "%u.%u", maj, ShaderLanguageVersion - maj * 100);
		os::Printer::log("GLSL version", buf, ELL_INFORMATION);
	} else
		os::Printer::log("GLSL not available.", ELL_INFORMATION);
	DriverAttributes->setAttribute("MaxTextures", (s32)Feature.MaxTextureUnits);
	DriverAttributes->setAttribute("MaxSupportedTextures", (s32)Feature.MaxTextureUnits);
	DriverAttributes->setAttribute("MaxLights", MaxLights);
	DriverAttributes->setAttribute("MaxAnisotropy", MaxAnisotropy);
	DriverAttributes->setAttribute("MaxUserClipPlanes", MaxUserClipPlanes);
	DriverAttributes->setAttribute("MaxAuxBuffers", MaxAuxBuffers);
	DriverAttributes->setAttribute("MaxMultipleRenderTargets", (s32)Feature.MultipleRenderTarget);
	DriverAttributes->setAttribute("MaxIndices", (s32)MaxIndices);
	DriverAttributes->setAttribute("MaxTextureSize", (s32)MaxTextureSize);
	DriverAttributes->setAttribute("MaxGeometryVerticesOut", (s32)MaxGeometryVerticesOut);
	DriverAttributes->setAttribute("MaxTextureLODBias", MaxTextureLODBias);
	DriverAttributes->setAttribute("Version", Version);
	DriverAttributes->setAttribute("ShaderLanguageVersion", ShaderLanguageVersion);
	DriverAttributes->setAttribute("AntiAlias", AntiAlias);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	UserClipPlanes.reallocate(MaxUserClipPlanes);
	for (i = 0; i < MaxUserClipPlanes; ++i)
		UserClipPlanes.push_back(SUserClipPlane());

	for (i = 0; i < ETS_COUNT; ++i)
		setTransform(static_cast<E_TRANSFORMATION_STATE>(i), core::IdentityMatrix);

	setAmbientLight(SColorf(0.0f, 0.0f, 0.0f, 0.0f));
#ifdef GL_EXT_separate_specular_color
	if (FeatureAvailable[IRR_EXT_separate_specular_color])
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#endif
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);

	// This is a fast replacement for NORMALIZE_NORMALS
	//	if ((Version>101) || FeatureAvailable[IRR_EXT_rescale_normal])
	//		glEnable(GL_RESCALE_NORMAL_EXT);

	glClearDepth(1.0);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glFrontFace(GL_CW);
	// adjust flat coloring scheme to DirectX version
#if defined(GL_ARB_provoking_vertex) || defined(GL_EXT_provoking_vertex)
	extGlProvokingVertex(GL_FIRST_VERTEX_CONVENTION_EXT);
#endif

	// Create built-in 2D quad for 2D rendering (both quads and lines).
	Quad2DVertices[0] = S3DVertex(core::vector3df(-1.0f, 1.0f, 0.0f), core::vector3df(0.0f, 0.0f, 0.0f), SColor(255, 255, 255, 255), core::vector2df(0.0f, 1.0f));
	Quad2DVertices[1] = S3DVertex(core::vector3df(1.0f, 1.0f, 0.0f), core::vector3df(0.0f, 0.0f, 0.0f), SColor(255, 255, 255, 255), core::vector2df(1.0f, 1.0f));
	Quad2DVertices[2] = S3DVertex(core::vector3df(1.0f, -1.0f, 0.0f), core::vector3df(0.0f, 0.0f, 0.0f), SColor(255, 255, 255, 255), core::vector2df(1.0f, 0.0f));
	Quad2DVertices[3] = S3DVertex(core::vector3df(-1.0f, -1.0f, 0.0f), core::vector3df(0.0f, 0.0f, 0.0f), SColor(255, 255, 255, 255), core::vector2df(0.0f, 0.0f));

	// create material renderers
	createMaterialRenderers();

	// set the renderstates
	setRenderStates3DMode();

	// set fog mode
	setFog(FogColor, FogType, FogStart, FogEnd, FogDensity, PixelFog, RangeFog);

	// create matrix for flipping textures
	TextureFlipMatrix.buildTextureTransform(0.0f, core::vector2df(0, 0), core::vector2df(0, 1.0f), core::vector2df(1.0f, -1.0f));

	// We need to reset once more at the beginning of the first rendering.
	// This fixes problems with intermediate changes to the material during texture load.
	ResetRenderStates = true;

	return true;
}

void COpenGLDriver::createMaterialRenderers()
{
	addAndDropMaterialRenderer(new COpenGLMaterialRenderer_SOLID(this));
	addAndDropMaterialRenderer(new COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL(this));
	addAndDropMaterialRenderer(new COpenGLMaterialRenderer_TRANSPARENT_ALPHA_CHANNEL_REF(this));
	addAndDropMaterialRenderer(new COpenGLMaterialRenderer_TRANSPARENT_VERTEX_ALPHA(this));
	addAndDropMaterialRenderer(new COpenGLMaterialRenderer_ONETEXTURE_BLEND(this));
}

bool COpenGLDriver::beginScene(u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil, const SExposedVideoData &videoData, core::rect<s32> *sourceRect)
{
	CNullDriver::beginScene(clearFlag, clearColor, clearDepth, clearStencil, videoData, sourceRect);

	if (ContextManager)
		ContextManager->activateContext(videoData, true);

	clearBuffers(clearFlag, clearColor, clearDepth, clearStencil);

	return true;
}

bool COpenGLDriver::endScene()
{
	CNullDriver::endScene();

	glFlush();

	bool status = false;

	if (ContextManager)
		status = ContextManager->swapBuffers();

	// todo: console device present

	return status;
}

//! Returns the transformation set by setTransform
const core::matrix4 &COpenGLDriver::getTransform(E_TRANSFORMATION_STATE state) const
{
	return Matrices[state];
}

//! sets transformation
void COpenGLDriver::setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat)
{
	Matrices[state] = mat;
	Transformation3DChanged = true;

	switch (state) {
	case ETS_VIEW:
	case ETS_WORLD: {
		// OpenGL only has a model matrix, view and world is not existent. so lets fake these two.
		CacheHandler->setMatrixMode(GL_MODELVIEW);

		// first load the viewing transformation for user clip planes
		glLoadMatrixf((Matrices[ETS_VIEW]).pointer());

		// we have to update the clip planes to the latest view matrix
		for (u32 i = 0; i < MaxUserClipPlanes; ++i)
			if (UserClipPlanes[i].Enabled)
				uploadClipPlane(i);

		// now the real model-view matrix
		glMultMatrixf(Matrices[ETS_WORLD].pointer());
	} break;
	case ETS_PROJECTION: {
		CacheHandler->setMatrixMode(GL_PROJECTION);
		glLoadMatrixf(mat.pointer());
	} break;
	default:
		break;
	}
}

bool COpenGLDriver::updateVertexHardwareBuffer(SHWBufferLink_opengl *HWBuffer)
{
	if (!HWBuffer)
		return false;

	if (!FeatureAvailable[IRR_ARB_vertex_buffer_object])
		return false;

#if defined(GL_ARB_vertex_buffer_object)
	const scene::IMeshBuffer *mb = HWBuffer->MeshBuffer;
	const void *vertices = mb->getVertices();
	const u32 vertexCount = mb->getVertexCount();
	const E_VERTEX_TYPE vType = mb->getVertexType();
	const u32 vertexSize = getVertexPitchFromType(vType);

	const c8 *vbuf = static_cast<const c8 *>(vertices);
	core::array<c8> buffer;
	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra]) {
		// buffer vertex data, and convert colors...
		buffer.set_used(vertexSize * vertexCount);
		memcpy(buffer.pointer(), vertices, vertexSize * vertexCount);
		vbuf = buffer.const_pointer();

		// in order to convert the colors into opengl format (RGBA)
		switch (vType) {
		case EVT_STANDARD: {
			S3DVertex *pb = reinterpret_cast<S3DVertex *>(buffer.pointer());
			const S3DVertex *po = static_cast<const S3DVertex *>(vertices);
			for (u32 i = 0; i < vertexCount; i++) {
				po[i].Color.toOpenGLColor((u8 *)&(pb[i].Color));
			}
		} break;
		case EVT_2TCOORDS: {
			S3DVertex2TCoords *pb = reinterpret_cast<S3DVertex2TCoords *>(buffer.pointer());
			const S3DVertex2TCoords *po = static_cast<const S3DVertex2TCoords *>(vertices);
			for (u32 i = 0; i < vertexCount; i++) {
				po[i].Color.toOpenGLColor((u8 *)&(pb[i].Color));
			}
		} break;
		case EVT_TANGENTS: {
			S3DVertexTangents *pb = reinterpret_cast<S3DVertexTangents *>(buffer.pointer());
			const S3DVertexTangents *po = static_cast<const S3DVertexTangents *>(vertices);
			for (u32 i = 0; i < vertexCount; i++) {
				po[i].Color.toOpenGLColor((u8 *)&(pb[i].Color));
			}
		} break;
		default: {
			return false;
		}
		}
	}

	// get or create buffer
	bool newBuffer = false;
	if (!HWBuffer->vbo_verticesID) {
		extGlGenBuffers(1, &HWBuffer->vbo_verticesID);
		if (!HWBuffer->vbo_verticesID)
			return false;
		newBuffer = true;
	} else if (HWBuffer->vbo_verticesSize < vertexCount * vertexSize) {
		newBuffer = true;
	}

	extGlBindBuffer(GL_ARRAY_BUFFER, HWBuffer->vbo_verticesID);

	// copy data to graphics card
	if (!newBuffer)
		extGlBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * vertexSize, vbuf);
	else {
		HWBuffer->vbo_verticesSize = vertexCount * vertexSize;

		if (HWBuffer->Mapped_Vertex == scene::EHM_STATIC)
			extGlBufferData(GL_ARRAY_BUFFER, vertexCount * vertexSize, vbuf, GL_STATIC_DRAW);
		else if (HWBuffer->Mapped_Vertex == scene::EHM_DYNAMIC)
			extGlBufferData(GL_ARRAY_BUFFER, vertexCount * vertexSize, vbuf, GL_DYNAMIC_DRAW);
		else // scene::EHM_STREAM
			extGlBufferData(GL_ARRAY_BUFFER, vertexCount * vertexSize, vbuf, GL_STREAM_DRAW);
	}

	extGlBindBuffer(GL_ARRAY_BUFFER, 0);

	return (!testGLError(__LINE__));
#else
	return false;
#endif
}

bool COpenGLDriver::updateIndexHardwareBuffer(SHWBufferLink_opengl *HWBuffer)
{
	if (!HWBuffer)
		return false;

	if (!FeatureAvailable[IRR_ARB_vertex_buffer_object])
		return false;

#if defined(GL_ARB_vertex_buffer_object)
	const scene::IMeshBuffer *mb = HWBuffer->MeshBuffer;

	const void *indices = mb->getIndices();
	u32 indexCount = mb->getIndexCount();

	GLenum indexSize;
	switch (mb->getIndexType()) {
	case EIT_16BIT: {
		indexSize = sizeof(u16);
		break;
	}
	case EIT_32BIT: {
		indexSize = sizeof(u32);
		break;
	}
	default: {
		return false;
	}
	}

	// get or create buffer
	bool newBuffer = false;
	if (!HWBuffer->vbo_indicesID) {
		extGlGenBuffers(1, &HWBuffer->vbo_indicesID);
		if (!HWBuffer->vbo_indicesID)
			return false;
		newBuffer = true;
	} else if (HWBuffer->vbo_indicesSize < indexCount * indexSize) {
		newBuffer = true;
	}

	extGlBindBuffer(GL_ELEMENT_ARRAY_BUFFER, HWBuffer->vbo_indicesID);

	// copy data to graphics card
	if (!newBuffer)
		extGlBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexCount * indexSize, indices);
	else {
		HWBuffer->vbo_indicesSize = indexCount * indexSize;

		if (HWBuffer->Mapped_Index == scene::EHM_STATIC)
			extGlBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * indexSize, indices, GL_STATIC_DRAW);
		else if (HWBuffer->Mapped_Index == scene::EHM_DYNAMIC)
			extGlBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * indexSize, indices, GL_DYNAMIC_DRAW);
		else // scene::EHM_STREAM
			extGlBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * indexSize, indices, GL_STREAM_DRAW);
	}

	extGlBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return (!testGLError(__LINE__));
#else
	return false;
#endif
}

//! updates hardware buffer if needed
bool COpenGLDriver::updateHardwareBuffer(SHWBufferLink *HWBuffer)
{
	if (!HWBuffer)
		return false;

	if (HWBuffer->Mapped_Vertex != scene::EHM_NEVER) {
		if (HWBuffer->ChangedID_Vertex != HWBuffer->MeshBuffer->getChangedID_Vertex() || !((SHWBufferLink_opengl *)HWBuffer)->vbo_verticesID) {

			HWBuffer->ChangedID_Vertex = HWBuffer->MeshBuffer->getChangedID_Vertex();

			if (!updateVertexHardwareBuffer((SHWBufferLink_opengl *)HWBuffer))
				return false;
		}
	}

	if (HWBuffer->Mapped_Index != scene::EHM_NEVER) {
		if (HWBuffer->ChangedID_Index != HWBuffer->MeshBuffer->getChangedID_Index() || !((SHWBufferLink_opengl *)HWBuffer)->vbo_indicesID) {

			HWBuffer->ChangedID_Index = HWBuffer->MeshBuffer->getChangedID_Index();

			if (!updateIndexHardwareBuffer((SHWBufferLink_opengl *)HWBuffer))
				return false;
		}
	}

	return true;
}

//! Create hardware buffer from meshbuffer
COpenGLDriver::SHWBufferLink *COpenGLDriver::createHardwareBuffer(const scene::IMeshBuffer *mb)
{
#if defined(GL_ARB_vertex_buffer_object)
	if (!mb || (mb->getHardwareMappingHint_Index() == scene::EHM_NEVER && mb->getHardwareMappingHint_Vertex() == scene::EHM_NEVER))
		return 0;

	SHWBufferLink_opengl *HWBuffer = new SHWBufferLink_opengl(mb);

	// add to map
	HWBuffer->listPosition = HWBufferList.insert(HWBufferList.end(), HWBuffer);

	HWBuffer->ChangedID_Vertex = HWBuffer->MeshBuffer->getChangedID_Vertex();
	HWBuffer->ChangedID_Index = HWBuffer->MeshBuffer->getChangedID_Index();
	HWBuffer->Mapped_Vertex = mb->getHardwareMappingHint_Vertex();
	HWBuffer->Mapped_Index = mb->getHardwareMappingHint_Index();
	HWBuffer->vbo_verticesID = 0;
	HWBuffer->vbo_indicesID = 0;
	HWBuffer->vbo_verticesSize = 0;
	HWBuffer->vbo_indicesSize = 0;

	if (!updateHardwareBuffer(HWBuffer)) {
		deleteHardwareBuffer(HWBuffer);
		return 0;
	}

	return HWBuffer;
#else
	return 0;
#endif
}

void COpenGLDriver::deleteHardwareBuffer(SHWBufferLink *_HWBuffer)
{
	if (!_HWBuffer)
		return;

#if defined(GL_ARB_vertex_buffer_object)
	SHWBufferLink_opengl *HWBuffer = (SHWBufferLink_opengl *)_HWBuffer;
	if (HWBuffer->vbo_verticesID) {
		extGlDeleteBuffers(1, &HWBuffer->vbo_verticesID);
		HWBuffer->vbo_verticesID = 0;
	}
	if (HWBuffer->vbo_indicesID) {
		extGlDeleteBuffers(1, &HWBuffer->vbo_indicesID);
		HWBuffer->vbo_indicesID = 0;
	}
#endif

	CNullDriver::deleteHardwareBuffer(_HWBuffer);
}

//! Draw hardware buffer
void COpenGLDriver::drawHardwareBuffer(SHWBufferLink *_HWBuffer)
{
	if (!_HWBuffer)
		return;

	updateHardwareBuffer(_HWBuffer); // check if update is needed

#if defined(GL_ARB_vertex_buffer_object)
	SHWBufferLink_opengl *HWBuffer = (SHWBufferLink_opengl *)_HWBuffer;

	const scene::IMeshBuffer *mb = HWBuffer->MeshBuffer;
	const void *vertices = mb->getVertices();
	const void *indexList = mb->getIndices();

	if (HWBuffer->Mapped_Vertex != scene::EHM_NEVER) {
		extGlBindBuffer(GL_ARRAY_BUFFER, HWBuffer->vbo_verticesID);
		vertices = 0;
	}

	if (HWBuffer->Mapped_Index != scene::EHM_NEVER) {
		extGlBindBuffer(GL_ELEMENT_ARRAY_BUFFER, HWBuffer->vbo_indicesID);
		indexList = 0;
	}

	drawVertexPrimitiveList(vertices, mb->getVertexCount(), indexList, mb->getPrimitiveCount(), mb->getVertexType(), mb->getPrimitiveType(), mb->getIndexType());

	if (HWBuffer->Mapped_Vertex != scene::EHM_NEVER)
		extGlBindBuffer(GL_ARRAY_BUFFER, 0);
	if (HWBuffer->Mapped_Index != scene::EHM_NEVER)
		extGlBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
}

//! Create occlusion query.
/** Use node for identification and mesh for occlusion test. */
void COpenGLDriver::addOcclusionQuery(scene::ISceneNode *node,
		const scene::IMesh *mesh)
{
	if (!queryFeature(EVDF_OCCLUSION_QUERY))
		return;

	CNullDriver::addOcclusionQuery(node, mesh);
	const s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if ((index != -1) && (OcclusionQueries[index].UID == 0))
		extGlGenQueries(1, reinterpret_cast<GLuint *>(&OcclusionQueries[index].UID));
}

//! Remove occlusion query.
void COpenGLDriver::removeOcclusionQuery(scene::ISceneNode *node)
{
	const s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index != -1) {
		if (OcclusionQueries[index].UID != 0)
			extGlDeleteQueries(1, reinterpret_cast<GLuint *>(&OcclusionQueries[index].UID));
		CNullDriver::removeOcclusionQuery(node);
	}
}

//! Run occlusion query. Draws mesh stored in query.
/** If the mesh shall not be rendered visible, use
overrideMaterial to disable the color and depth buffer. */
void COpenGLDriver::runOcclusionQuery(scene::ISceneNode *node, bool visible)
{
	if (!node)
		return;

	const s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index != -1) {
		if (OcclusionQueries[index].UID)
			extGlBeginQuery(
#ifdef GL_ARB_occlusion_query
					GL_SAMPLES_PASSED_ARB,
#else
					0,
#endif
					OcclusionQueries[index].UID);
		CNullDriver::runOcclusionQuery(node, visible);
		if (OcclusionQueries[index].UID)
			extGlEndQuery(
#ifdef GL_ARB_occlusion_query
					GL_SAMPLES_PASSED_ARB);
#else
					0);
#endif
		testGLError(__LINE__);
	}
}

//! Update occlusion query. Retrieves results from GPU.
/** If the query shall not block, set the flag to false.
Update might not occur in this case, though */
void COpenGLDriver::updateOcclusionQuery(scene::ISceneNode *node, bool block)
{
	const s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index != -1) {
		// not yet started
		if (OcclusionQueries[index].Run == u32(~0))
			return;
		GLint available = block ? GL_TRUE : GL_FALSE;
		if (!block) {
			extGlGetQueryObjectiv(OcclusionQueries[index].UID,
#ifdef GL_ARB_occlusion_query
					GL_QUERY_RESULT_AVAILABLE_ARB,
#elif defined(GL_NV_occlusion_query)
					GL_PIXEL_COUNT_AVAILABLE_NV,
#else
					0,
#endif
					&available);
			testGLError(__LINE__);
		}
		if (available == GL_TRUE) {
			extGlGetQueryObjectiv(OcclusionQueries[index].UID,
#ifdef GL_ARB_occlusion_query
					GL_QUERY_RESULT_ARB,
#elif defined(GL_NV_occlusion_query)
					GL_PIXEL_COUNT_NV,
#else
					0,
#endif
					&available);
			if (queryFeature(EVDF_OCCLUSION_QUERY))
				OcclusionQueries[index].Result = available;
		}
		testGLError(__LINE__);
	}
}

//! Return query result.
/** Return value is the number of visible pixels/fragments.
The value is a safe approximation, i.e. can be larger than the
actual value of pixels. */
u32 COpenGLDriver::getOcclusionQueryResult(scene::ISceneNode *node) const
{
	const s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index != -1)
		return OcclusionQueries[index].Result;
	else
		return ~0;
}

//! Create render target.
IRenderTarget *COpenGLDriver::addRenderTarget()
{
	COpenGLRenderTarget *renderTarget = new COpenGLRenderTarget(this);
	RenderTargets.push_back(renderTarget);

	return renderTarget;
}

// small helper function to create vertex buffer object address offsets
static inline const GLvoid *buffer_offset(const size_t offset)
{
	return (const GLvoid *)offset;
}

//! draws a vertex primitive list
void COpenGLDriver::drawVertexPrimitiveList(const void *vertices, u32 vertexCount,
		const void *indexList, u32 primitiveCount,
		E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	if (!primitiveCount || !vertexCount)
		return;

	if (!checkPrimitiveCount(primitiveCount))
		return;

	CNullDriver::drawVertexPrimitiveList(vertices, vertexCount, indexList, primitiveCount, vType, pType, iType);

	if (vertices && !FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(vertices, vertexCount, vType);

	// draw everything
	setRenderStates3DMode();

	if ((pType != scene::EPT_POINTS) && (pType != scene::EPT_POINT_SPRITES))
		CacheHandler->setClientState(true, true, true, true);
	else
		CacheHandler->setClientState(true, false, true, false);

// due to missing defines in OSX headers, we have to be more specific with this check
// #if defined(GL_ARB_vertex_array_bgra) || defined(GL_EXT_vertex_array_bgra)
#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (vertices) {
		if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) {
			switch (vType) {
			case EVT_STANDARD:
				glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].Color);
				break;
			case EVT_2TCOORDS:
				glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].Color);
				break;
			case EVT_TANGENTS:
				glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Color);
				break;
			}
		} else {
			// avoid passing broken pointer to OpenGL
			_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
		}
	}

	switch (vType) {
	case EVT_STANDARD:
		if (vertices) {
			glNormalPointer(GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].Normal);
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].TCoords);
			glVertexPointer(3, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].Pos);
		} else {
			glNormalPointer(GL_FLOAT, sizeof(S3DVertex), buffer_offset(12));
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), buffer_offset(24));
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), buffer_offset(28));
			glVertexPointer(3, GL_FLOAT, sizeof(S3DVertex), 0);
		}

		if (Feature.MaxTextureUnits > 0 && CacheHandler->getTextureCache()[1]) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if (vertices)
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].TCoords);
			else
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), buffer_offset(28));
		}
		break;
	case EVT_2TCOORDS:
		if (vertices) {
			glNormalPointer(GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].Normal);
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].TCoords);
			glVertexPointer(3, GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].Pos);
		} else {
			glNormalPointer(GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(12));
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex2TCoords), buffer_offset(24));
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(28));
			glVertexPointer(3, GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(0));
		}

		if (Feature.MaxTextureUnits > 0) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if (vertices)
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].TCoords2);
			else
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(36));
		}
		break;
	case EVT_TANGENTS:
		if (vertices) {
			glNormalPointer(GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Normal);
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].TCoords);
			glVertexPointer(3, GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Pos);
		} else {
			glNormalPointer(GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(12));
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertexTangents), buffer_offset(24));
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(28));
			glVertexPointer(3, GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(0));
		}

		if (Feature.MaxTextureUnits > 0) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if (vertices)
				glTexCoordPointer(3, GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Tangent);
			else
				glTexCoordPointer(3, GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(36));

			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 2);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if (vertices)
				glTexCoordPointer(3, GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Binormal);
			else
				glTexCoordPointer(3, GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(48));
		}
		break;
	}

	renderArray(indexList, primitiveCount, pType, iType);

	if (Feature.MaxTextureUnits > 0) {
		if (vType == EVT_TANGENTS) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 2);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		if ((vType != EVT_STANDARD) || CacheHandler->getTextureCache()[1]) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		CacheHandler->setClientActiveTexture(GL_TEXTURE0);
	}
}

void COpenGLDriver::getColorBuffer(const void *vertices, u32 vertexCount, E_VERTEX_TYPE vType)
{
	// convert colors to gl color format.
	vertexCount *= 4; // reused as color component count
	ColorBuffer.set_used(vertexCount);
	u32 i;

	switch (vType) {
	case EVT_STANDARD: {
		const S3DVertex *p = static_cast<const S3DVertex *>(vertices);
		for (i = 0; i < vertexCount; i += 4) {
			p->Color.toOpenGLColor(&ColorBuffer[i]);
			++p;
		}
	} break;
	case EVT_2TCOORDS: {
		const S3DVertex2TCoords *p = static_cast<const S3DVertex2TCoords *>(vertices);
		for (i = 0; i < vertexCount; i += 4) {
			p->Color.toOpenGLColor(&ColorBuffer[i]);
			++p;
		}
	} break;
	case EVT_TANGENTS: {
		const S3DVertexTangents *p = static_cast<const S3DVertexTangents *>(vertices);
		for (i = 0; i < vertexCount; i += 4) {
			p->Color.toOpenGLColor(&ColorBuffer[i]);
			++p;
		}
	} break;
	}
}

void COpenGLDriver::renderArray(const void *indexList, u32 primitiveCount,
		scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	GLenum indexSize = 0;

	switch (iType) {
	case EIT_16BIT: {
		indexSize = GL_UNSIGNED_SHORT;
		break;
	}
	case EIT_32BIT: {
		indexSize = GL_UNSIGNED_INT;
		break;
	}
	}

	switch (pType) {
	case scene::EPT_POINTS:
	case scene::EPT_POINT_SPRITES: {
#ifdef GL_ARB_point_sprite
		if (pType == scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite])
			glEnable(GL_POINT_SPRITE_ARB);
#endif

		// prepare size and attenuation (where supported)
		GLfloat particleSize = Material.Thickness;
		//			if (AntiAlias)
		//				particleSize=core::clamp(particleSize, DimSmoothedPoint[0], DimSmoothedPoint[1]);
		//			else
		particleSize = core::clamp(particleSize, DimAliasedPoint[0], DimAliasedPoint[1]);
#if defined(GL_VERSION_1_4) || defined(GL_ARB_point_parameters) || defined(GL_EXT_point_parameters) || defined(GL_SGIS_point_parameters)
		const float att[] = {1.0f, 1.0f, 0.0f};
#if defined(GL_VERSION_1_4)
		extGlPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, att);
		//			extGlPointParameterf(GL_POINT_SIZE_MIN,1.f);
		extGlPointParameterf(GL_POINT_SIZE_MAX, particleSize);
		extGlPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, 1.0f);
#elif defined(GL_ARB_point_parameters)
		extGlPointParameterfv(GL_POINT_DISTANCE_ATTENUATION_ARB, att);
		//			extGlPointParameterf(GL_POINT_SIZE_MIN_ARB,1.f);
		extGlPointParameterf(GL_POINT_SIZE_MAX_ARB, particleSize);
		extGlPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE_ARB, 1.0f);
#elif defined(GL_EXT_point_parameters)
		extGlPointParameterfv(GL_DISTANCE_ATTENUATION_EXT, att);
		//			extGlPointParameterf(GL_POINT_SIZE_MIN_EXT,1.f);
		extGlPointParameterf(GL_POINT_SIZE_MAX_EXT, particleSize);
		extGlPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE_EXT, 1.0f);
#elif defined(GL_SGIS_point_parameters)
		extGlPointParameterfv(GL_DISTANCE_ATTENUATION_SGIS, att);
		//			extGlPointParameterf(GL_POINT_SIZE_MIN_SGIS,1.f);
		extGlPointParameterf(GL_POINT_SIZE_MAX_SGIS, particleSize);
		extGlPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE_SGIS, 1.0f);
#endif
#endif
		glPointSize(particleSize);

#ifdef GL_ARB_point_sprite
		if (pType == scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite]) {
			CacheHandler->setActiveTexture(GL_TEXTURE0_ARB);
			glTexEnvf(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE, GL_TRUE);
		}
#endif
		glDrawArrays(GL_POINTS, 0, primitiveCount);
#ifdef GL_ARB_point_sprite
		if (pType == scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite]) {
			glDisable(GL_POINT_SPRITE_ARB);

			CacheHandler->setActiveTexture(GL_TEXTURE0_ARB);
			glTexEnvf(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE, GL_FALSE);
		}
#endif
	} break;
	case scene::EPT_LINE_STRIP:
		glDrawElements(GL_LINE_STRIP, primitiveCount + 1, indexSize, indexList);
		break;
	case scene::EPT_LINE_LOOP:
		glDrawElements(GL_LINE_LOOP, primitiveCount, indexSize, indexList);
		break;
	case scene::EPT_LINES:
		glDrawElements(GL_LINES, primitiveCount * 2, indexSize, indexList);
		break;
	case scene::EPT_TRIANGLE_STRIP:
		glDrawElements(GL_TRIANGLE_STRIP, primitiveCount + 2, indexSize, indexList);
		break;
	case scene::EPT_TRIANGLE_FAN:
		glDrawElements(GL_TRIANGLE_FAN, primitiveCount + 2, indexSize, indexList);
		break;
	case scene::EPT_TRIANGLES:
		glDrawElements(GL_TRIANGLES, primitiveCount * 3, indexSize, indexList);
		break;
	}
}

//! draws a vertex primitive list in 2d
void COpenGLDriver::draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount,
		const void *indexList, u32 primitiveCount,
		E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	if (!primitiveCount || !vertexCount)
		return;

	if (!checkPrimitiveCount(primitiveCount))
		return;

	CNullDriver::draw2DVertexPrimitiveList(vertices, vertexCount, indexList, primitiveCount, vType, pType, iType);

	if (vertices && !FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(vertices, vertexCount, vType);

	// draw everything
	CacheHandler->getTextureCache().set(0, Material.getTexture(0));
	if (Material.MaterialType == EMT_ONETEXTURE_BLEND) {
		E_BLEND_FACTOR srcFact;
		E_BLEND_FACTOR dstFact;
		E_MODULATE_FUNC modulo;
		u32 alphaSource;
		unpack_textureBlendFunc(srcFact, dstFact, modulo, alphaSource, Material.MaterialTypeParam);
		setRenderStates2DMode(alphaSource & video::EAS_VERTEX_COLOR, (Material.getTexture(0) != 0), (alphaSource & video::EAS_TEXTURE) != 0);
	} else
		setRenderStates2DMode(Material.MaterialType == EMT_TRANSPARENT_VERTEX_ALPHA, (Material.getTexture(0) != 0), Material.MaterialType == EMT_TRANSPARENT_ALPHA_CHANNEL);

	if ((pType != scene::EPT_POINTS) && (pType != scene::EPT_POINT_SPRITES))
		CacheHandler->setClientState(true, false, true, true);
	else
		CacheHandler->setClientState(true, false, true, false);

// due to missing defines in OSX headers, we have to be more specific with this check
// #if defined(GL_ARB_vertex_array_bgra) || defined(GL_EXT_vertex_array_bgra)
#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (vertices) {
		if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) {
			switch (vType) {
			case EVT_STANDARD:
				glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].Color);
				break;
			case EVT_2TCOORDS:
				glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].Color);
				break;
			case EVT_TANGENTS:
				glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Color);
				break;
			}
		} else {
			// avoid passing broken pointer to OpenGL
			_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
		}
	}

	switch (vType) {
	case EVT_STANDARD:
		if (vertices) {
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].TCoords);
			glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].Pos);
		} else {
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), buffer_offset(24));
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), buffer_offset(28));
			glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), 0);
		}

		if (Feature.MaxTextureUnits > 0 && CacheHandler->getTextureCache()[1]) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if (vertices)
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(vertices))[0].TCoords);
			else
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), buffer_offset(28));
		}
		break;
	case EVT_2TCOORDS:
		if (vertices) {
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].TCoords);
			glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].Pos);
		} else {
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex2TCoords), buffer_offset(24));
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(28));
			glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(0));
		}

		if (Feature.MaxTextureUnits > 0) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if (vertices)
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), &(static_cast<const S3DVertex2TCoords *>(vertices))[0].TCoords2);
			else
				glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex2TCoords), buffer_offset(36));
		}
		break;
	case EVT_TANGENTS:
		if (vertices) {
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].TCoords);
			glVertexPointer(2, GL_FLOAT, sizeof(S3DVertexTangents), &(static_cast<const S3DVertexTangents *>(vertices))[0].Pos);
		} else {
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertexTangents), buffer_offset(24));
			glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(28));
			glVertexPointer(2, GL_FLOAT, sizeof(S3DVertexTangents), buffer_offset(0));
		}

		break;
	}

	renderArray(indexList, primitiveCount, pType, iType);

	if (Feature.MaxTextureUnits > 0) {
		if ((vType != EVT_STANDARD) || CacheHandler->getTextureCache()[1]) {
			CacheHandler->setClientActiveTexture(GL_TEXTURE0 + 1);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		CacheHandler->setClientActiveTexture(GL_TEXTURE0);
	}
}

void COpenGLDriver::draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
		const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect, SColor color,
		bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	if (!sourceRect.isValid())
		return;

	// clip these coordinates
	core::rect<s32> targetRect(destPos, sourceRect.getSize());
	if (clipRect) {
		targetRect.clipAgainst(*clipRect);
		if (targetRect.getWidth() < 0 || targetRect.getHeight() < 0)
			return;
	}

	const core::dimension2d<u32> &renderTargetSize = getCurrentRenderTargetSize();
	targetRect.clipAgainst(core::rect<s32>(0, 0, (s32)renderTargetSize.Width, (s32)renderTargetSize.Height));
	if (targetRect.getWidth() < 0 || targetRect.getHeight() < 0)
		return;

	// ok, we've clipped everything.
	// now draw it.
	const core::dimension2d<s32> sourceSize(targetRect.getSize());
	const core::position2d<s32> sourcePos(sourceRect.UpperLeftCorner + (targetRect.UpperLeftCorner - destPos));

	const core::dimension2d<u32> &ss = texture->getOriginalSize();
	const f32 invW = 1.f / static_cast<f32>(ss.Width);
	const f32 invH = 1.f / static_cast<f32>(ss.Height);
	const core::rect<f32> tcoords(
			sourcePos.X * invW,
			sourcePos.Y * invH,
			(sourcePos.X + sourceSize.Width) * invW,
			(sourcePos.Y + sourceSize.Height) * invH);

	disableTextures(1);
	if (!CacheHandler->getTextureCache().set(0, texture))
		return;
	setRenderStates2DMode(color.getAlpha() < 255, true, useAlphaChannelOfTexture);

	Quad2DVertices[0].Color = color;
	Quad2DVertices[1].Color = color;
	Quad2DVertices[2].Color = color;
	Quad2DVertices[3].Color = color;

	Quad2DVertices[0].Pos = core::vector3df((f32)targetRect.UpperLeftCorner.X, (f32)targetRect.UpperLeftCorner.Y, 0.0f);
	Quad2DVertices[1].Pos = core::vector3df((f32)targetRect.LowerRightCorner.X, (f32)targetRect.UpperLeftCorner.Y, 0.0f);
	Quad2DVertices[2].Pos = core::vector3df((f32)targetRect.LowerRightCorner.X, (f32)targetRect.LowerRightCorner.Y, 0.0f);
	Quad2DVertices[3].Pos = core::vector3df((f32)targetRect.UpperLeftCorner.X, (f32)targetRect.LowerRightCorner.Y, 0.0f);

	Quad2DVertices[0].TCoords = core::vector2df(tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
	Quad2DVertices[1].TCoords = core::vector2df(tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
	Quad2DVertices[2].TCoords = core::vector2df(tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
	Quad2DVertices[3].TCoords = core::vector2df(tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);

	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(Quad2DVertices, 4, EVT_STANDARD);

	CacheHandler->setClientState(true, false, true, true);

	glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].TCoords);
	glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Pos);

#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Color);
	else {
		_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
	}

	glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, Quad2DIndices);
}

void COpenGLDriver::draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
		const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect,
		const video::SColor *const colors, bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	const core::dimension2d<u32> &ss = texture->getOriginalSize();
	const f32 invW = 1.f / static_cast<f32>(ss.Width);
	const f32 invH = 1.f / static_cast<f32>(ss.Height);
	const core::rect<f32> tcoords(
			sourceRect.UpperLeftCorner.X * invW,
			sourceRect.UpperLeftCorner.Y * invH,
			sourceRect.LowerRightCorner.X * invW,
			sourceRect.LowerRightCorner.Y * invH);

	const video::SColor temp[4] = {
			0xFFFFFFFF,
			0xFFFFFFFF,
			0xFFFFFFFF,
			0xFFFFFFFF,
		};

	const video::SColor *const useColor = colors ? colors : temp;

	disableTextures(1);
	if (!CacheHandler->getTextureCache().set(0, texture))
		return;
	setRenderStates2DMode(useColor[0].getAlpha() < 255 || useColor[1].getAlpha() < 255 ||
								  useColor[2].getAlpha() < 255 || useColor[3].getAlpha() < 255,
			true, useAlphaChannelOfTexture);

	if (clipRect) {
		if (!clipRect->isValid())
			return;

		glEnable(GL_SCISSOR_TEST);
		const core::dimension2d<u32> &renderTargetSize = getCurrentRenderTargetSize();
		glScissor(clipRect->UpperLeftCorner.X, renderTargetSize.Height - clipRect->LowerRightCorner.Y,
				clipRect->getWidth(), clipRect->getHeight());
	}

	Quad2DVertices[0].Color = useColor[0];
	Quad2DVertices[1].Color = useColor[3];
	Quad2DVertices[2].Color = useColor[2];
	Quad2DVertices[3].Color = useColor[1];

	Quad2DVertices[0].Pos = core::vector3df((f32)destRect.UpperLeftCorner.X, (f32)destRect.UpperLeftCorner.Y, 0.0f);
	Quad2DVertices[1].Pos = core::vector3df((f32)destRect.LowerRightCorner.X, (f32)destRect.UpperLeftCorner.Y, 0.0f);
	Quad2DVertices[2].Pos = core::vector3df((f32)destRect.LowerRightCorner.X, (f32)destRect.LowerRightCorner.Y, 0.0f);
	Quad2DVertices[3].Pos = core::vector3df((f32)destRect.UpperLeftCorner.X, (f32)destRect.LowerRightCorner.Y, 0.0f);

	Quad2DVertices[0].TCoords = core::vector2df(tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
	Quad2DVertices[1].TCoords = core::vector2df(tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
	Quad2DVertices[2].TCoords = core::vector2df(tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
	Quad2DVertices[3].TCoords = core::vector2df(tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);

	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(Quad2DVertices, 4, EVT_STANDARD);

	CacheHandler->setClientState(true, false, true, true);

	glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].TCoords);
	glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Pos);

#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Color);
	else {
		_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
	}

	glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, Quad2DIndices);

	if (clipRect)
		glDisable(GL_SCISSOR_TEST);
}

void COpenGLDriver::draw2DImage(const video::ITexture *texture, u32 layer, bool flip)
{
	if (!texture || !CacheHandler->getTextureCache().set(0, texture))
		return;

	disableTextures(1);

	setRenderStates2DMode(false, true, true);

	CacheHandler->setMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	CacheHandler->setMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	Transformation3DChanged = true;

	CacheHandler->setClientState(true, false, false, true);

	const core::vector3df positionData[4] = {
			core::vector3df(-1.f, 1.f, 0.f),
			core::vector3df(1.f, 1.f, 0.f),
			core::vector3df(1.f, -1.f, 0.f),
			core::vector3df(-1.f, -1.f, 0.f)};

	glVertexPointer(2, GL_FLOAT, sizeof(core::vector3df), positionData);

	if (texture && texture->getType() == ETT_CUBEMAP) {
		const core::vector3df texcoordCubeData[6][4] = {

				// GL_TEXTURE_CUBE_MAP_POSITIVE_X
				{
						core::vector3df(1.f, 1.f, 1.f),
						core::vector3df(1.f, 1.f, -1.f),
						core::vector3df(1.f, -1.f, -1.f),
						core::vector3df(1.f, -1.f, 1.f)},

				// GL_TEXTURE_CUBE_MAP_NEGATIVE_X
				{
						core::vector3df(-1.f, 1.f, -1.f),
						core::vector3df(-1.f, 1.f, 1.f),
						core::vector3df(-1.f, -1.f, 1.f),
						core::vector3df(-1.f, -1.f, -1.f)},

				// GL_TEXTURE_CUBE_MAP_POSITIVE_Y
				{
						core::vector3df(-1.f, 1.f, -1.f),
						core::vector3df(1.f, 1.f, -1.f),
						core::vector3df(1.f, 1.f, 1.f),
						core::vector3df(-1.f, 1.f, 1.f)},

				// GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
				{
						core::vector3df(-1.f, -1.f, 1.f),
						core::vector3df(-1.f, -1.f, -1.f),
						core::vector3df(1.f, -1.f, -1.f),
						core::vector3df(1.f, -1.f, 1.f)},

				// GL_TEXTURE_CUBE_MAP_POSITIVE_Z
				{
						core::vector3df(-1.f, 1.f, 1.f),
						core::vector3df(-1.f, -1.f, 1.f),
						core::vector3df(1.f, -1.f, 1.f),
						core::vector3df(1.f, 1.f, 1.f)},

				// GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				{
						core::vector3df(1.f, 1.f, -1.f),
						core::vector3df(-1.f, 1.f, -1.f),
						core::vector3df(-1.f, -1.f, -1.f),
						core::vector3df(1.f, -1.f, -1.f)}};

		const core::vector3df texcoordData[4] = {
				texcoordCubeData[layer][(flip) ? 3 : 0],
				texcoordCubeData[layer][(flip) ? 2 : 1],
				texcoordCubeData[layer][(flip) ? 1 : 2],
				texcoordCubeData[layer][(flip) ? 0 : 3]};

		glTexCoordPointer(3, GL_FLOAT, sizeof(core::vector3df), texcoordData);
	} else {
		f32 modificator = (flip) ? 1.f : 0.f;

		core::vector2df texcoordData[4] = {
				core::vector2df(0.f, 0.f + modificator),
				core::vector2df(1.f, 0.f + modificator),
				core::vector2df(1.f, 1.f - modificator),
				core::vector2df(0.f, 1.f - modificator)};

		glTexCoordPointer(2, GL_FLOAT, sizeof(core::vector2df), texcoordData);
	}

	glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, Quad2DIndices);
}

//! draws a set of 2d images, using a color and the alpha channel of the
//! texture if desired.
void COpenGLDriver::draw2DImageBatch(const video::ITexture *texture,
		const core::array<core::position2d<s32>> &positions,
		const core::array<core::rect<s32>> &sourceRects,
		const core::rect<s32> *clipRect,
		SColor color,
		bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	const u32 drawCount = core::min_<u32>(positions.size(), sourceRects.size());

	const core::dimension2d<u32> &ss = texture->getOriginalSize();
	const f32 invW = 1.f / static_cast<f32>(ss.Width);
	const f32 invH = 1.f / static_cast<f32>(ss.Height);
	const core::dimension2d<u32> &renderTargetSize = getCurrentRenderTargetSize();

	disableTextures(1);
	if (!CacheHandler->getTextureCache().set(0, texture))
		return;
	setRenderStates2DMode(color.getAlpha() < 255, true, useAlphaChannelOfTexture);

	Quad2DVertices[0].Color = color;
	Quad2DVertices[1].Color = color;
	Quad2DVertices[2].Color = color;
	Quad2DVertices[3].Color = color;

	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(Quad2DVertices, 4, EVT_STANDARD);

	CacheHandler->setClientState(true, false, true, true);

	glTexCoordPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].TCoords);
	glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Pos);

#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Color);
	else {
		_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
	}

	for (u32 i = 0; i < drawCount; ++i) {
		if (!sourceRects[i].isValid())
			continue;

		core::position2d<s32> targetPos(positions[i]);
		core::position2d<s32> sourcePos(sourceRects[i].UpperLeftCorner);
		// This needs to be signed as it may go negative.
		core::dimension2d<s32> sourceSize(sourceRects[i].getSize());
		if (clipRect) {
			if (targetPos.X < clipRect->UpperLeftCorner.X) {
				sourceSize.Width += targetPos.X - clipRect->UpperLeftCorner.X;
				if (sourceSize.Width <= 0)
					continue;

				sourcePos.X -= targetPos.X - clipRect->UpperLeftCorner.X;
				targetPos.X = clipRect->UpperLeftCorner.X;
			}

			if (targetPos.X + sourceSize.Width > clipRect->LowerRightCorner.X) {
				sourceSize.Width -= (targetPos.X + sourceSize.Width) - clipRect->LowerRightCorner.X;
				if (sourceSize.Width <= 0)
					continue;
			}

			if (targetPos.Y < clipRect->UpperLeftCorner.Y) {
				sourceSize.Height += targetPos.Y - clipRect->UpperLeftCorner.Y;
				if (sourceSize.Height <= 0)
					continue;

				sourcePos.Y -= targetPos.Y - clipRect->UpperLeftCorner.Y;
				targetPos.Y = clipRect->UpperLeftCorner.Y;
			}

			if (targetPos.Y + sourceSize.Height > clipRect->LowerRightCorner.Y) {
				sourceSize.Height -= (targetPos.Y + sourceSize.Height) - clipRect->LowerRightCorner.Y;
				if (sourceSize.Height <= 0)
					continue;
			}
		}

		// clip these coordinates

		if (targetPos.X < 0) {
			sourceSize.Width += targetPos.X;
			if (sourceSize.Width <= 0)
				continue;

			sourcePos.X -= targetPos.X;
			targetPos.X = 0;
		}

		if (targetPos.X + sourceSize.Width > (s32)renderTargetSize.Width) {
			sourceSize.Width -= (targetPos.X + sourceSize.Width) - renderTargetSize.Width;
			if (sourceSize.Width <= 0)
				continue;
		}

		if (targetPos.Y < 0) {
			sourceSize.Height += targetPos.Y;
			if (sourceSize.Height <= 0)
				continue;

			sourcePos.Y -= targetPos.Y;
			targetPos.Y = 0;
		}

		if (targetPos.Y + sourceSize.Height > (s32)renderTargetSize.Height) {
			sourceSize.Height -= (targetPos.Y + sourceSize.Height) - renderTargetSize.Height;
			if (sourceSize.Height <= 0)
				continue;
		}

		// ok, we've clipped everything.
		// now draw it.

		const core::rect<f32> tcoords(
				sourcePos.X * invW,
				sourcePos.Y * invH,
				(sourcePos.X + sourceSize.Width) * invW,
				(sourcePos.Y + sourceSize.Height) * invH);

		const core::rect<s32> poss(targetPos, sourceSize);

		Quad2DVertices[0].Pos = core::vector3df((f32)poss.UpperLeftCorner.X, (f32)poss.UpperLeftCorner.Y, 0.0f);
		Quad2DVertices[1].Pos = core::vector3df((f32)poss.LowerRightCorner.X, (f32)poss.UpperLeftCorner.Y, 0.0f);
		Quad2DVertices[2].Pos = core::vector3df((f32)poss.LowerRightCorner.X, (f32)poss.LowerRightCorner.Y, 0.0f);
		Quad2DVertices[3].Pos = core::vector3df((f32)poss.UpperLeftCorner.X, (f32)poss.LowerRightCorner.Y, 0.0f);

		Quad2DVertices[0].TCoords = core::vector2df(tcoords.UpperLeftCorner.X, tcoords.UpperLeftCorner.Y);
		Quad2DVertices[1].TCoords = core::vector2df(tcoords.LowerRightCorner.X, tcoords.UpperLeftCorner.Y);
		Quad2DVertices[2].TCoords = core::vector2df(tcoords.LowerRightCorner.X, tcoords.LowerRightCorner.Y);
		Quad2DVertices[3].TCoords = core::vector2df(tcoords.UpperLeftCorner.X, tcoords.LowerRightCorner.Y);

		glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, Quad2DIndices);
	}
}

//! draw a 2d rectangle
void COpenGLDriver::draw2DRectangle(SColor color, const core::rect<s32> &position,
		const core::rect<s32> *clip)
{
	disableTextures();
	setRenderStates2DMode(color.getAlpha() < 255, false, false);

	core::rect<s32> pos = position;

	if (clip)
		pos.clipAgainst(*clip);

	if (!pos.isValid())
		return;

	glColor4ub(color.getRed(), color.getGreen(), color.getBlue(), color.getAlpha());
	glRectf(GLfloat(pos.UpperLeftCorner.X), GLfloat(pos.UpperLeftCorner.Y),
			GLfloat(pos.LowerRightCorner.X), GLfloat(pos.LowerRightCorner.Y));
}

//! draw an 2d rectangle
void COpenGLDriver::draw2DRectangle(const core::rect<s32> &position,
		SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
		const core::rect<s32> *clip)
{
	core::rect<s32> pos = position;

	if (clip)
		pos.clipAgainst(*clip);

	if (!pos.isValid())
		return;

	disableTextures();

	setRenderStates2DMode(colorLeftUp.getAlpha() < 255 ||
								  colorRightUp.getAlpha() < 255 ||
								  colorLeftDown.getAlpha() < 255 ||
								  colorRightDown.getAlpha() < 255,
			false, false);

	Quad2DVertices[0].Color = colorLeftUp;
	Quad2DVertices[1].Color = colorRightUp;
	Quad2DVertices[2].Color = colorRightDown;
	Quad2DVertices[3].Color = colorLeftDown;

	Quad2DVertices[0].Pos = core::vector3df((f32)pos.UpperLeftCorner.X, (f32)pos.UpperLeftCorner.Y, 0.0f);
	Quad2DVertices[1].Pos = core::vector3df((f32)pos.LowerRightCorner.X, (f32)pos.UpperLeftCorner.Y, 0.0f);
	Quad2DVertices[2].Pos = core::vector3df((f32)pos.LowerRightCorner.X, (f32)pos.LowerRightCorner.Y, 0.0f);
	Quad2DVertices[3].Pos = core::vector3df((f32)pos.UpperLeftCorner.X, (f32)pos.LowerRightCorner.Y, 0.0f);

	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(Quad2DVertices, 4, EVT_STANDARD);

	CacheHandler->setClientState(true, false, true, false);

	glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Pos);

#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Color);
	else {
		_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
	}

	glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, Quad2DIndices);
}

//! Draws a 2d line.
void COpenGLDriver::draw2DLine(const core::position2d<s32> &start,
		const core::position2d<s32> &end, SColor color)
{
	{
		disableTextures();
		setRenderStates2DMode(color.getAlpha() < 255, false, false);

		Quad2DVertices[0].Color = color;
		Quad2DVertices[1].Color = color;

		Quad2DVertices[0].Pos = core::vector3df((f32)start.X, (f32)start.Y, 0.0f);
		Quad2DVertices[1].Pos = core::vector3df((f32)end.X, (f32)end.Y, 0.0f);

		if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
			getColorBuffer(Quad2DVertices, 2, EVT_STANDARD);

		CacheHandler->setClientState(true, false, true, false);

		glVertexPointer(2, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Pos);

#ifdef GL_BGRA
		const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
		const GLint colorSize = 4;
#endif
		if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Color);
		else {
			_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
			glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
		}

		glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, Quad2DIndices);

		// Draw sometimes non-drawn first & last pixel (search for "diamond exit rule")
		// HACK this messes with alpha blending
		glDrawArrays(GL_POINTS, 0, 1);
		glDrawArrays(GL_POINTS, 1, 1);
	}
}

//! disables all textures beginning with the optional fromStage parameter. Otherwise all texture stages are disabled.
//! Returns whether disabling was successful or not.
bool COpenGLDriver::disableTextures(u32 fromStage)
{
	bool result = true;
	for (u32 i = fromStage; i < Feature.MaxTextureUnits; ++i) {
		result &= CacheHandler->getTextureCache().set(i, 0, EST_ACTIVE_ON_CHANGE);
	}
	return result;
}

//! creates a matrix in supplied GLfloat array to pass to OpenGL
inline void COpenGLDriver::getGLMatrix(GLfloat gl_matrix[16], const core::matrix4 &m)
{
	memcpy(gl_matrix, m.pointer(), 16 * sizeof(f32));
}

//! creates a opengltexturematrix from a D3D style texture matrix
inline void COpenGLDriver::getGLTextureMatrix(GLfloat *o, const core::matrix4 &m)
{
	o[0] = m[0];
	o[1] = m[1];
	o[2] = 0.f;
	o[3] = 0.f;

	o[4] = m[4];
	o[5] = m[5];
	o[6] = 0.f;
	o[7] = 0.f;

	o[8] = 0.f;
	o[9] = 0.f;
	o[10] = 1.f;
	o[11] = 0.f;

	o[12] = m[8];
	o[13] = m[9];
	o[14] = 0.f;
	o[15] = 1.f;
}

ITexture *COpenGLDriver::createDeviceDependentTexture(const io::path &name, IImage *image)
{
	core::array<IImage *> imageArray(1);
	imageArray.push_back(image);

	COpenGLTexture *texture = new COpenGLTexture(name, imageArray, ETT_2D, this);

	return texture;
}

ITexture *COpenGLDriver::createDeviceDependentTextureCubemap(const io::path &name, const core::array<IImage *> &image)
{
	COpenGLTexture *texture = new COpenGLTexture(name, image, ETT_CUBEMAP, this);

	return texture;
}

void COpenGLDriver::disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag)
{
	CNullDriver::disableFeature(feature, flag);

	if (feature == EVDF_TEXTURE_CUBEMAP_SEAMLESS) {
		if (queryFeature(feature))
			glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		else if (COpenGLExtensionHandler::queryFeature(feature))
			glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}
}

//! Sets a material. All 3d drawing functions draw geometry now using this material.
void COpenGLDriver::setMaterial(const SMaterial &material)
{
	Material = material;
	OverrideMaterial.apply(Material);

	for (u32 i = 0; i < Feature.MaxTextureUnits; ++i) {
		const ITexture *texture = Material.getTexture(i);
		CacheHandler->getTextureCache().set(i, texture, EST_ACTIVE_ON_CHANGE);
		if (texture) {
			setTransform((E_TRANSFORMATION_STATE)(ETS_TEXTURE_0 + i), material.getTextureMatrix(i));
		}
	}
}

//! prints error if an error happened.
bool COpenGLDriver::testGLError(int code)
{
	if (!Params.DriverDebug)
		return false;

	GLenum g = glGetError();
	switch (g) {
	case GL_NO_ERROR:
		return false;
	case GL_INVALID_ENUM:
		os::Printer::log("GL_INVALID_ENUM", core::stringc(code).c_str(), ELL_ERROR);
		break;
	case GL_INVALID_VALUE:
		os::Printer::log("GL_INVALID_VALUE", core::stringc(code).c_str(), ELL_ERROR);
		break;
	case GL_INVALID_OPERATION:
		os::Printer::log("GL_INVALID_OPERATION", core::stringc(code).c_str(), ELL_ERROR);
		break;
	case GL_STACK_OVERFLOW:
		os::Printer::log("GL_STACK_OVERFLOW", core::stringc(code).c_str(), ELL_ERROR);
		break;
	case GL_STACK_UNDERFLOW:
		os::Printer::log("GL_STACK_UNDERFLOW", core::stringc(code).c_str(), ELL_ERROR);
		break;
	case GL_OUT_OF_MEMORY:
		os::Printer::log("GL_OUT_OF_MEMORY", core::stringc(code).c_str(), ELL_ERROR);
		break;
	case GL_TABLE_TOO_LARGE:
		os::Printer::log("GL_TABLE_TOO_LARGE", core::stringc(code).c_str(), ELL_ERROR);
		break;
#if defined(GL_EXT_framebuffer_object)
	case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
		os::Printer::log("GL_INVALID_FRAMEBUFFER_OPERATION", core::stringc(code).c_str(), ELL_ERROR);
		break;
#endif
	};
	return true;
}

//! sets the needed renderstates
void COpenGLDriver::setRenderStates3DMode()
{
	if (CurrentRenderMode != ERM_3D) {
		// Reset Texture Stages
		CacheHandler->setBlend(false);
		CacheHandler->setAlphaTest(false);
		CacheHandler->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		CacheHandler->setActiveTexture(GL_TEXTURE0_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		// switch back the matrices
		CacheHandler->setMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((Matrices[ETS_VIEW] * Matrices[ETS_WORLD]).pointer());

		CacheHandler->setMatrixMode(GL_PROJECTION);
		glLoadMatrixf(Matrices[ETS_PROJECTION].pointer());

		ResetRenderStates = true;
#ifdef GL_EXT_clip_volume_hint
		if (FeatureAvailable[IRR_EXT_clip_volume_hint])
			glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_NICEST);
#endif
	}

	if (ResetRenderStates || LastMaterial != Material) {
		// unset old material

		if (LastMaterial.MaterialType != Material.MaterialType &&
				static_cast<u32>(LastMaterial.MaterialType) < MaterialRenderers.size())
			MaterialRenderers[LastMaterial.MaterialType].Renderer->OnUnsetMaterial();

		// set new material.
		if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
			MaterialRenderers[Material.MaterialType].Renderer->OnSetMaterial(
					Material, LastMaterial, ResetRenderStates, this);

		LastMaterial = Material;
		CacheHandler->correctCacheMaterial(LastMaterial);
		ResetRenderStates = false;
	}

	if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
		MaterialRenderers[Material.MaterialType].Renderer->OnRender(this, video::EVT_STANDARD);

	CurrentRenderMode = ERM_3D;
}

//! Get native wrap mode value
GLint COpenGLDriver::getTextureWrapMode(const u8 clamp)
{
	GLint mode = GL_REPEAT;
	switch (clamp) {
	case ETC_REPEAT:
		mode = GL_REPEAT;
		break;
	case ETC_CLAMP:
		mode = GL_CLAMP;
		break;
	case ETC_CLAMP_TO_EDGE:
#ifdef GL_VERSION_1_2
		if (Version > 101)
			mode = GL_CLAMP_TO_EDGE;
		else
#endif
#ifdef GL_SGIS_texture_edge_clamp
				if (FeatureAvailable[IRR_SGIS_texture_edge_clamp])
			mode = GL_CLAMP_TO_EDGE_SGIS;
		else
#endif
			// fallback
			mode = GL_CLAMP;
		break;
	case ETC_CLAMP_TO_BORDER:
#ifdef GL_VERSION_1_3
		if (Version > 102)
			mode = GL_CLAMP_TO_BORDER;
		else
#endif
#ifdef GL_ARB_texture_border_clamp
				if (FeatureAvailable[IRR_ARB_texture_border_clamp])
			mode = GL_CLAMP_TO_BORDER_ARB;
		else
#endif
#ifdef GL_SGIS_texture_border_clamp
				if (FeatureAvailable[IRR_SGIS_texture_border_clamp])
			mode = GL_CLAMP_TO_BORDER_SGIS;
		else
#endif
			// fallback
			mode = GL_CLAMP;
		break;
	case ETC_MIRROR:
#ifdef GL_VERSION_1_4
		if (Version > 103)
			mode = GL_MIRRORED_REPEAT;
		else
#endif
#ifdef GL_ARB_texture_border_clamp
				if (FeatureAvailable[IRR_ARB_texture_mirrored_repeat])
			mode = GL_MIRRORED_REPEAT_ARB;
		else
#endif
#ifdef GL_IBM_texture_mirrored_repeat
				if (FeatureAvailable[IRR_IBM_texture_mirrored_repeat])
			mode = GL_MIRRORED_REPEAT_IBM;
		else
#endif
			mode = GL_REPEAT;
		break;
	case ETC_MIRROR_CLAMP:
#ifdef GL_EXT_texture_mirror_clamp
		if (FeatureAvailable[IRR_EXT_texture_mirror_clamp])
			mode = GL_MIRROR_CLAMP_EXT;
		else
#endif
#if defined(GL_ATI_texture_mirror_once)
				if (FeatureAvailable[IRR_ATI_texture_mirror_once])
			mode = GL_MIRROR_CLAMP_ATI;
		else
#endif
			mode = GL_CLAMP;
		break;
	case ETC_MIRROR_CLAMP_TO_EDGE:
#ifdef GL_EXT_texture_mirror_clamp
		if (FeatureAvailable[IRR_EXT_texture_mirror_clamp])
			mode = GL_MIRROR_CLAMP_TO_EDGE_EXT;
		else
#endif
#if defined(GL_ATI_texture_mirror_once)
				if (FeatureAvailable[IRR_ATI_texture_mirror_once])
			mode = GL_MIRROR_CLAMP_TO_EDGE_ATI;
		else
#endif
			mode = GL_CLAMP;
		break;
	case ETC_MIRROR_CLAMP_TO_BORDER:
#ifdef GL_EXT_texture_mirror_clamp
		if (FeatureAvailable[IRR_EXT_texture_mirror_clamp])
			mode = GL_MIRROR_CLAMP_TO_BORDER_EXT;
		else
#endif
			mode = GL_CLAMP;
		break;
	}
	return mode;
}

//! Can be called by an IMaterialRenderer to make its work easier.
void COpenGLDriver::setBasicRenderStates(const SMaterial &material, const SMaterial &lastmaterial,
		bool resetAllRenderStates)
{
	// Fixed pipeline isn't important for shader based materials

	E_OPENGL_FIXED_PIPELINE_STATE tempState = FixedPipelineState;

	if (resetAllRenderStates || tempState == EOFPS_ENABLE || tempState == EOFPS_DISABLE_TO_ENABLE) {
		// material colors
		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.ColorMaterial != material.ColorMaterial) {
			switch (material.ColorMaterial) {
			case ECM_NONE:
				glDisable(GL_COLOR_MATERIAL);
				break;
			case ECM_DIFFUSE:
				glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
				break;
			case ECM_AMBIENT:
				glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT);
				break;
			case ECM_EMISSIVE:
				glColorMaterial(GL_FRONT_AND_BACK, GL_EMISSION);
				break;
			case ECM_SPECULAR:
				glColorMaterial(GL_FRONT_AND_BACK, GL_SPECULAR);
				break;
			case ECM_DIFFUSE_AND_AMBIENT:
				glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
				break;
			}
			if (material.ColorMaterial != ECM_NONE)
				glEnable(GL_COLOR_MATERIAL);
		}

		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.AmbientColor != material.AmbientColor ||
				lastmaterial.DiffuseColor != material.DiffuseColor ||
				lastmaterial.EmissiveColor != material.EmissiveColor ||
				lastmaterial.ColorMaterial != material.ColorMaterial) {
			GLfloat color[4];

			const f32 inv = 1.0f / 255.0f;

			if ((material.ColorMaterial != video::ECM_AMBIENT) &&
					(material.ColorMaterial != video::ECM_DIFFUSE_AND_AMBIENT)) {
				color[0] = material.AmbientColor.getRed() * inv;
				color[1] = material.AmbientColor.getGreen() * inv;
				color[2] = material.AmbientColor.getBlue() * inv;
				color[3] = material.AmbientColor.getAlpha() * inv;
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, color);
			}

			if ((material.ColorMaterial != video::ECM_DIFFUSE) &&
					(material.ColorMaterial != video::ECM_DIFFUSE_AND_AMBIENT)) {
				color[0] = material.DiffuseColor.getRed() * inv;
				color[1] = material.DiffuseColor.getGreen() * inv;
				color[2] = material.DiffuseColor.getBlue() * inv;
				color[3] = material.DiffuseColor.getAlpha() * inv;
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
			}

			if (material.ColorMaterial != video::ECM_EMISSIVE) {
				color[0] = material.EmissiveColor.getRed() * inv;
				color[1] = material.EmissiveColor.getGreen() * inv;
				color[2] = material.EmissiveColor.getBlue() * inv;
				color[3] = material.EmissiveColor.getAlpha() * inv;
				glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, color);
			}
		}

		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.SpecularColor != material.SpecularColor ||
				lastmaterial.Shininess != material.Shininess ||
				lastmaterial.ColorMaterial != material.ColorMaterial) {
			GLfloat color[4] = {0.f, 0.f, 0.f, 1.f};
			const f32 inv = 1.0f / 255.0f;

			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, material.Shininess);
			// disable Specular colors if no shininess is set
			if ((material.Shininess != 0.0f) &&
					(material.ColorMaterial != video::ECM_SPECULAR)) {
#ifdef GL_EXT_separate_specular_color
				if (FeatureAvailable[IRR_EXT_separate_specular_color])
					glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#endif
				color[0] = material.SpecularColor.getRed() * inv;
				color[1] = material.SpecularColor.getGreen() * inv;
				color[2] = material.SpecularColor.getBlue() * inv;
				color[3] = material.SpecularColor.getAlpha() * inv;
			}
#ifdef GL_EXT_separate_specular_color
			else if (FeatureAvailable[IRR_EXT_separate_specular_color])
				glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
#endif
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, color);
		}

		// shademode
		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.GouraudShading != material.GouraudShading) {
			if (material.GouraudShading)
				glShadeModel(GL_SMOOTH);
			else
				glShadeModel(GL_FLAT);
		}

		// lighting
		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.Lighting != material.Lighting) {
			if (material.Lighting)
				glEnable(GL_LIGHTING);
			else
				glDisable(GL_LIGHTING);
		}

		// fog
		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.FogEnable != material.FogEnable) {
			if (material.FogEnable)
				glEnable(GL_FOG);
			else
				glDisable(GL_FOG);
		}

		// normalization
		if (resetAllRenderStates || tempState == EOFPS_DISABLE_TO_ENABLE ||
				lastmaterial.NormalizeNormals != material.NormalizeNormals) {
			if (material.NormalizeNormals)
				glEnable(GL_NORMALIZE);
			else
				glDisable(GL_NORMALIZE);
		}

		// Set fixed pipeline as active.
		tempState = EOFPS_ENABLE;
	} else if (tempState == EOFPS_ENABLE_TO_DISABLE) {
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glDisable(GL_NORMALIZE);

		// Set programmable pipeline as active.
		tempState = EOFPS_DISABLE;
	}

	// tempState == EOFPS_DISABLE - driver doesn't calls functions related to fixed pipeline.

	// fillmode - fixed pipeline call, but it emulate GL_LINES behaviour in rendering, so it stay here.
	if (resetAllRenderStates || (lastmaterial.Wireframe != material.Wireframe) || (lastmaterial.PointCloud != material.PointCloud))
		glPolygonMode(GL_FRONT_AND_BACK, material.Wireframe ? GL_LINE : material.PointCloud ? GL_POINT
																							: GL_FILL);

	// ZBuffer
	switch (material.ZBuffer) {
	case ECFN_DISABLED:
		CacheHandler->setDepthTest(false);
		break;
	case ECFN_LESSEQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_LEQUAL);
		break;
	case ECFN_EQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_EQUAL);
		break;
	case ECFN_LESS:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_LESS);
		break;
	case ECFN_NOTEQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_NOTEQUAL);
		break;
	case ECFN_GREATEREQUAL:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_GEQUAL);
		break;
	case ECFN_GREATER:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_GREATER);
		break;
	case ECFN_ALWAYS:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_ALWAYS);
		break;
	case ECFN_NEVER:
		CacheHandler->setDepthTest(true);
		CacheHandler->setDepthFunc(GL_NEVER);
		break;
	default:
		break;
	}

	// ZWrite
	if (getWriteZBuffer(material)) {
		CacheHandler->setDepthMask(true);
	} else {
		CacheHandler->setDepthMask(false);
	}

	// Back face culling
	if ((material.FrontfaceCulling) && (material.BackfaceCulling)) {
		CacheHandler->setCullFaceFunc(GL_FRONT_AND_BACK);
		CacheHandler->setCullFace(true);
	} else if (material.BackfaceCulling) {
		CacheHandler->setCullFaceFunc(GL_BACK);
		CacheHandler->setCullFace(true);
	} else if (material.FrontfaceCulling) {
		CacheHandler->setCullFaceFunc(GL_FRONT);
		CacheHandler->setCullFace(true);
	} else {
		CacheHandler->setCullFace(false);
	}

	// Color Mask
	CacheHandler->setColorMask(material.ColorMask);

	// Blend Equation
	if (material.BlendOperation == EBO_NONE)
		CacheHandler->setBlend(false);
	else {
		CacheHandler->setBlend(true);

#if defined(GL_EXT_blend_subtract) || defined(GL_EXT_blend_minmax) || defined(GL_EXT_blend_logic_op) || defined(GL_VERSION_1_4)
		if (queryFeature(EVDF_BLEND_OPERATIONS)) {
			switch (material.BlendOperation) {
			case EBO_SUBTRACT:
#if defined(GL_VERSION_1_4)
				CacheHandler->setBlendEquation(GL_FUNC_SUBTRACT);
#elif defined(GL_EXT_blend_subtract)
				CacheHandler->setBlendEquation(GL_FUNC_SUBTRACT_EXT);
#endif
				break;
			case EBO_REVSUBTRACT:
#if defined(GL_VERSION_1_4)
				CacheHandler->setBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
#elif defined(GL_EXT_blend_subtract)
				CacheHandler->setBlendEquation(GL_FUNC_REVERSE_SUBTRACT_EXT);
#endif
				break;
			case EBO_MIN:
#if defined(GL_VERSION_1_4)
				CacheHandler->setBlendEquation(GL_MIN);
#elif defined(GL_EXT_blend_minmax)
				CacheHandler->setBlendEquation(GL_MIN_EXT);
#endif
				break;
			case EBO_MAX:
#if defined(GL_VERSION_1_4)
				CacheHandler->setBlendEquation(GL_MAX);
#elif defined(GL_EXT_blend_minmax)
				CacheHandler->setBlendEquation(GL_MAX_EXT);
#endif
				break;
			case EBO_MIN_FACTOR:
#if defined(GL_AMD_blend_minmax_factor)
				if (FeatureAvailable[IRR_AMD_blend_minmax_factor])
					CacheHandler->setBlendEquation(GL_FACTOR_MIN_AMD);
#endif
					// fallback in case of missing extension
#if defined(GL_VERSION_1_4)
#if defined(GL_AMD_blend_minmax_factor)
				else
#endif
					CacheHandler->setBlendEquation(GL_MIN);
#endif
				break;
			case EBO_MAX_FACTOR:
#if defined(GL_AMD_blend_minmax_factor)
				if (FeatureAvailable[IRR_AMD_blend_minmax_factor])
					CacheHandler->setBlendEquation(GL_FACTOR_MAX_AMD);
#endif
					// fallback in case of missing extension
#if defined(GL_VERSION_1_4)
#if defined(GL_AMD_blend_minmax_factor)
				else
#endif
					CacheHandler->setBlendEquation(GL_MAX);
#endif
				break;
			case EBO_MIN_ALPHA:
#if defined(GL_SGIX_blend_alpha_minmax)
				if (FeatureAvailable[IRR_SGIX_blend_alpha_minmax])
					CacheHandler->setBlendEquation(GL_ALPHA_MIN_SGIX);
				// fallback in case of missing extension
				else if (FeatureAvailable[IRR_EXT_blend_minmax])
					CacheHandler->setBlendEquation(GL_MIN_EXT);
#endif
				break;
			case EBO_MAX_ALPHA:
#if defined(GL_SGIX_blend_alpha_minmax)
				if (FeatureAvailable[IRR_SGIX_blend_alpha_minmax])
					CacheHandler->setBlendEquation(GL_ALPHA_MAX_SGIX);
				// fallback in case of missing extension
				else if (FeatureAvailable[IRR_EXT_blend_minmax])
					CacheHandler->setBlendEquation(GL_MAX_EXT);
#endif
				break;
			default:
#if defined(GL_VERSION_1_4)
				CacheHandler->setBlendEquation(GL_FUNC_ADD);
#elif defined(GL_EXT_blend_subtract) || defined(GL_EXT_blend_minmax) || defined(GL_EXT_blend_logic_op)
				CacheHandler->setBlendEquation(GL_FUNC_ADD_EXT);
#endif
				break;
			}
		}
#endif
	}

	// Blend Factor
	if (IR(material.BlendFactor) & 0xFFFFFFFF // TODO: why the & 0xFFFFFFFF?
			&& material.MaterialType != EMT_ONETEXTURE_BLEND) {
		E_BLEND_FACTOR srcRGBFact = EBF_ZERO;
		E_BLEND_FACTOR dstRGBFact = EBF_ZERO;
		E_BLEND_FACTOR srcAlphaFact = EBF_ZERO;
		E_BLEND_FACTOR dstAlphaFact = EBF_ZERO;
		E_MODULATE_FUNC modulo = EMFN_MODULATE_1X;
		u32 alphaSource = 0;

		unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulo, alphaSource, material.BlendFactor);

		if (queryFeature(EVDF_BLEND_SEPARATE)) {
			CacheHandler->setBlendFuncSeparate(getGLBlend(srcRGBFact), getGLBlend(dstRGBFact),
					getGLBlend(srcAlphaFact), getGLBlend(dstAlphaFact));
		} else {
			CacheHandler->setBlendFunc(getGLBlend(srcRGBFact), getGLBlend(dstRGBFact));
		}
	}

	// Polygon Offset
	if (queryFeature(EVDF_POLYGON_OFFSET) && (resetAllRenderStates ||
													 lastmaterial.PolygonOffsetSlopeScale != material.PolygonOffsetSlopeScale ||
													 lastmaterial.PolygonOffsetDepthBias != material.PolygonOffsetDepthBias)) {
		glDisable(lastmaterial.Wireframe ? GL_POLYGON_OFFSET_LINE : lastmaterial.PointCloud ? GL_POLYGON_OFFSET_POINT
																							: GL_POLYGON_OFFSET_FILL);
		if (material.PolygonOffsetSlopeScale || material.PolygonOffsetDepthBias) {
			glEnable(material.Wireframe ? GL_POLYGON_OFFSET_LINE : material.PointCloud ? GL_POLYGON_OFFSET_POINT
																					   : GL_POLYGON_OFFSET_FILL);

			glPolygonOffset(material.PolygonOffsetSlopeScale, material.PolygonOffsetDepthBias);
		} else {
			glPolygonOffset(0.0f, 0.f);
		}
	}

	// thickness
	if (resetAllRenderStates || lastmaterial.Thickness != material.Thickness) {
		if (AntiAlias) {
			//			glPointSize(core::clamp(static_cast<GLfloat>(material.Thickness), DimSmoothedPoint[0], DimSmoothedPoint[1]));
			// we don't use point smoothing
			glPointSize(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedPoint[0], DimAliasedPoint[1]));
			glLineWidth(core::clamp(static_cast<GLfloat>(material.Thickness), DimSmoothedLine[0], DimSmoothedLine[1]));
		} else {
			glPointSize(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedPoint[0], DimAliasedPoint[1]));
			glLineWidth(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedLine[0], DimAliasedLine[1]));
		}
	}

	// Anti aliasing
	if ((resetAllRenderStates || lastmaterial.AntiAliasing != material.AntiAliasing) && FeatureAvailable[IRR_ARB_multisample]) {
		if (material.AntiAliasing & EAAM_ALPHA_TO_COVERAGE)
			glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);
		else if (lastmaterial.AntiAliasing & EAAM_ALPHA_TO_COVERAGE)
			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);

		if ((AntiAlias >= 2) && (material.AntiAliasing & (EAAM_SIMPLE | EAAM_QUALITY))) {
			glEnable(GL_MULTISAMPLE_ARB);
#ifdef GL_NV_multisample_filter_hint
			if (FeatureAvailable[IRR_NV_multisample_filter_hint]) {
				if ((material.AntiAliasing & EAAM_QUALITY) == EAAM_QUALITY)
					glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
				else
					glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_FASTEST);
			}
#endif
		} else
			glDisable(GL_MULTISAMPLE_ARB);
	}

	// Texture parameters
	setTextureRenderStates(material, resetAllRenderStates);

	// set current fixed pipeline state
	FixedPipelineState = tempState;
}

//! Compare in SMaterial doesn't check texture parameters, so we should call this on each OnRender call.
void COpenGLDriver::setTextureRenderStates(const SMaterial &material, bool resetAllRenderstates)
{
	// Set textures to TU/TIU and apply filters to them

	for (s32 i = Feature.MaxTextureUnits - 1; i >= 0; --i) {
		bool fixedPipeline = false;

		if (FixedPipelineState == EOFPS_ENABLE || FixedPipelineState == EOFPS_DISABLE_TO_ENABLE)
			fixedPipeline = true;

		const COpenGLTexture *tmpTexture = CacheHandler->getTextureCache().get(i);

		if (tmpTexture) {
			CacheHandler->setActiveTexture(GL_TEXTURE0 + i);

			// Minetest uses the first texture matrix even with the programmable pipeline
			if (fixedPipeline || i == 0) {
				const bool isRTT = tmpTexture->isRenderTarget();

				CacheHandler->setMatrixMode(GL_TEXTURE);

				if (!isRTT && Matrices[ETS_TEXTURE_0 + i].isIdentity())
					glLoadIdentity();
				else {
					GLfloat glmat[16];
					if (isRTT)
						getGLTextureMatrix(glmat, Matrices[ETS_TEXTURE_0 + i] * TextureFlipMatrix);
					else
						getGLTextureMatrix(glmat, Matrices[ETS_TEXTURE_0 + i]);
					glLoadMatrixf(glmat);
				}
			}

			const GLenum tmpType = tmpTexture->getOpenGLTextureType();

			COpenGLTexture::SStatesCache &statesCache = tmpTexture->getStatesCache();

			if (resetAllRenderstates)
				statesCache.IsCached = false;

#ifdef GL_VERSION_2_1
			if (Version >= 201) {
				if (!statesCache.IsCached || material.TextureLayers[i].LODBias != statesCache.LODBias) {
					if (material.TextureLayers[i].LODBias) {
						const float tmp = core::clamp(material.TextureLayers[i].LODBias * 0.125f, -MaxTextureLODBias, MaxTextureLODBias);
						glTexParameterf(tmpType, GL_TEXTURE_LOD_BIAS, tmp);
					} else
						glTexParameterf(tmpType, GL_TEXTURE_LOD_BIAS, 0.f);

					statesCache.LODBias = material.TextureLayers[i].LODBias;
				}
			} else if (FeatureAvailable[IRR_EXT_texture_lod_bias]) {
				if (material.TextureLayers[i].LODBias) {
					const float tmp = core::clamp(material.TextureLayers[i].LODBias * 0.125f, -MaxTextureLODBias, MaxTextureLODBias);
					glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, tmp);
				} else
					glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 0.f);
			}
#elif defined(GL_EXT_texture_lod_bias)
			if (FeatureAvailable[IRR_EXT_texture_lod_bias]) {
				if (material.TextureLayers[i].LODBias) {
					const float tmp = core::clamp(material.TextureLayers[i].LODBias * 0.125f, -MaxTextureLODBias, MaxTextureLODBias);
					glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, tmp);
				} else
					glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 0.f);
			}
#endif

			if (!statesCache.IsCached || material.TextureLayers[i].MagFilter != statesCache.MagFilter) {
				E_TEXTURE_MAG_FILTER magFilter = material.TextureLayers[i].MagFilter;
				glTexParameteri(tmpType, GL_TEXTURE_MAG_FILTER,
						magFilter == ETMAGF_NEAREST ? GL_NEAREST : (assert(magFilter == ETMAGF_LINEAR), GL_LINEAR));

				statesCache.MagFilter = magFilter;
			}

			if (material.UseMipMaps && tmpTexture->hasMipMaps()) {
				if (!statesCache.IsCached || material.TextureLayers[i].MinFilter != statesCache.MinFilter ||
						!statesCache.MipMapStatus) {
					E_TEXTURE_MIN_FILTER minFilter = material.TextureLayers[i].MinFilter;
					glTexParameteri(tmpType, GL_TEXTURE_MIN_FILTER,
							minFilter == ETMINF_NEAREST_MIPMAP_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : minFilter == ETMINF_LINEAR_MIPMAP_NEAREST ? GL_LINEAR_MIPMAP_NEAREST
																							 : minFilter == ETMINF_NEAREST_MIPMAP_LINEAR       ? GL_NEAREST_MIPMAP_LINEAR
																																			   : (assert(minFilter == ETMINF_LINEAR_MIPMAP_LINEAR), GL_LINEAR_MIPMAP_LINEAR));

					statesCache.MinFilter = minFilter;
					statesCache.MipMapStatus = true;
				}
			} else {
				if (!statesCache.IsCached || material.TextureLayers[i].MinFilter != statesCache.MinFilter ||
						statesCache.MipMapStatus) {
					E_TEXTURE_MIN_FILTER minFilter = material.TextureLayers[i].MinFilter;
					glTexParameteri(tmpType, GL_TEXTURE_MIN_FILTER,
							(minFilter == ETMINF_NEAREST_MIPMAP_NEAREST || minFilter == ETMINF_NEAREST_MIPMAP_LINEAR) ? GL_NEAREST : (assert(minFilter == ETMINF_LINEAR_MIPMAP_NEAREST || minFilter == ETMINF_LINEAR_MIPMAP_LINEAR), GL_LINEAR));

					statesCache.MinFilter = minFilter;
					statesCache.MipMapStatus = false;
				}
			}

#ifdef GL_EXT_texture_filter_anisotropic
			if (FeatureAvailable[IRR_EXT_texture_filter_anisotropic] &&
					(!statesCache.IsCached || material.TextureLayers[i].AnisotropicFilter != statesCache.AnisotropicFilter)) {
				glTexParameteri(tmpType, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						material.TextureLayers[i].AnisotropicFilter > 1 ? core::min_(MaxAnisotropy, material.TextureLayers[i].AnisotropicFilter) : 1);

				statesCache.AnisotropicFilter = material.TextureLayers[i].AnisotropicFilter;
			}
#endif

			if (!statesCache.IsCached || material.TextureLayers[i].TextureWrapU != statesCache.WrapU) {
				glTexParameteri(tmpType, GL_TEXTURE_WRAP_S, getTextureWrapMode(material.TextureLayers[i].TextureWrapU));
				statesCache.WrapU = material.TextureLayers[i].TextureWrapU;
			}

			if (!statesCache.IsCached || material.TextureLayers[i].TextureWrapV != statesCache.WrapV) {
				glTexParameteri(tmpType, GL_TEXTURE_WRAP_T, getTextureWrapMode(material.TextureLayers[i].TextureWrapV));
				statesCache.WrapV = material.TextureLayers[i].TextureWrapV;
			}

			if (!statesCache.IsCached || material.TextureLayers[i].TextureWrapW != statesCache.WrapW) {
				glTexParameteri(tmpType, GL_TEXTURE_WRAP_R, getTextureWrapMode(material.TextureLayers[i].TextureWrapW));
				statesCache.WrapW = material.TextureLayers[i].TextureWrapW;
			}

			statesCache.IsCached = true;
		}
	}
}

//! Enable the 2d override material
void COpenGLDriver::enableMaterial2D(bool enable)
{
	if (!enable)
		CurrentRenderMode = ERM_NONE;
	CNullDriver::enableMaterial2D(enable);
}

//! sets the needed renderstates
void COpenGLDriver::setRenderStates2DMode(bool alpha, bool texture, bool alphaChannel)
{
	// 2d methods uses fixed pipeline
	if (FixedPipelineState == COpenGLDriver::EOFPS_DISABLE)
		FixedPipelineState = COpenGLDriver::EOFPS_DISABLE_TO_ENABLE;
	else
		FixedPipelineState = COpenGLDriver::EOFPS_ENABLE;

	bool resetAllRenderStates = false;

	if (CurrentRenderMode != ERM_2D || Transformation3DChanged) {
		// unset last 3d material
		if (CurrentRenderMode == ERM_3D) {
			if (static_cast<u32>(LastMaterial.MaterialType) < MaterialRenderers.size())
				MaterialRenderers[LastMaterial.MaterialType].Renderer->OnUnsetMaterial();
		}

		if (Transformation3DChanged) {
			CacheHandler->setMatrixMode(GL_PROJECTION);

			const core::dimension2d<u32> &renderTargetSize = getCurrentRenderTargetSize();
			core::matrix4 m(core::matrix4::EM4CONST_NOTHING);
			m.buildProjectionMatrixOrthoLH(f32(renderTargetSize.Width), f32(-(s32)(renderTargetSize.Height)), -1.0f, 1.0f);
			m.setTranslation(core::vector3df(-1, 1, 0));
			glLoadMatrixf(m.pointer());

			CacheHandler->setMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glTranslatef(0.375f, 0.375f, 0.0f);

			Transformation3DChanged = false;
		}

		CacheHandler->setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		CacheHandler->setBlendEquation(GL_FUNC_ADD);

#ifdef GL_EXT_clip_volume_hint
		if (FeatureAvailable[IRR_EXT_clip_volume_hint])
			glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST);
#endif

		resetAllRenderStates = true;
	}

	SMaterial currentMaterial = (!OverrideMaterial2DEnabled) ? InitMaterial2D : OverrideMaterial2D;
	currentMaterial.Lighting = false;

	if (texture) {
		setTransform(ETS_TEXTURE_0, core::IdentityMatrix);

		// Due to the transformation change, the previous line would call a reset each frame
		// but we can safely reset the variable as it was false before
		Transformation3DChanged = false;
	} else {
		CacheHandler->getTextureCache().set(0, 0);
	}

	setBasicRenderStates(currentMaterial, LastMaterial, resetAllRenderStates);

	LastMaterial = currentMaterial;
	CacheHandler->correctCacheMaterial(LastMaterial);

	// no alphaChannel without texture
	alphaChannel &= texture;

	if (alphaChannel || alpha) {
		CacheHandler->setBlend(true);
		CacheHandler->setAlphaTest(true);
		CacheHandler->setAlphaFunc(GL_GREATER, 0.f);
	} else {
		CacheHandler->setBlend(false);
		CacheHandler->setAlphaTest(false);
	}

	if (texture) {
		CacheHandler->setActiveTexture(GL_TEXTURE0_ARB);

		if (alphaChannel) {
			// if alpha and alpha texture just modulate, otherwise use only the alpha channel
			if (alpha) {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			} else {
#if defined(GL_ARB_texture_env_combine) || defined(GL_EXT_texture_env_combine)
				if (FeatureAvailable[IRR_ARB_texture_env_combine] || FeatureAvailable[IRR_EXT_texture_env_combine]) {
#ifdef GL_ARB_texture_env_combine
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
					// rgb always modulates
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
#else
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
					// rgb always modulates
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
#endif
				} else
#endif
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		} else {
			if (alpha) {
#if defined(GL_ARB_texture_env_combine) || defined(GL_EXT_texture_env_combine)
				if (FeatureAvailable[IRR_ARB_texture_env_combine] || FeatureAvailable[IRR_EXT_texture_env_combine]) {
#ifdef GL_ARB_texture_env_combine
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PRIMARY_COLOR_ARB);
					// rgb always modulates
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
#else
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PRIMARY_COLOR_EXT);
					// rgb always modulates
					glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
					glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
#endif
				} else
#endif
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			} else {
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		}
	}

	CurrentRenderMode = ERM_2D;
}

//! \return Returns the name of the video driver.
const char *COpenGLDriver::getName() const
{
	return Name.c_str();
}

//! Sets the dynamic ambient light color. The default color is
//! (0,0,0,0) which means it is dark.
//! \param color: New color of the ambient light.
void COpenGLDriver::setAmbientLight(const SColorf &color)
{
	CNullDriver::setAmbientLight(color);
	GLfloat data[4] = {color.r, color.g, color.b, color.a};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, data);
}

// this code was sent in by Oliver Klems, thank you! (I modified the glViewport
// method just a bit.
void COpenGLDriver::setViewPort(const core::rect<s32> &area)
{
	core::rect<s32> vp = area;
	core::rect<s32> rendert(0, 0, getCurrentRenderTargetSize().Width, getCurrentRenderTargetSize().Height);
	vp.clipAgainst(rendert);

	if (vp.getHeight() > 0 && vp.getWidth() > 0)
		CacheHandler->setViewport(vp.UpperLeftCorner.X, getCurrentRenderTargetSize().Height - vp.UpperLeftCorner.Y - vp.getHeight(), vp.getWidth(), vp.getHeight());

	ViewPort = vp;
}

void COpenGLDriver::setViewPortRaw(u32 width, u32 height)
{
	CacheHandler->setViewport(0, 0, width, height);
	ViewPort = core::recti(0, 0, width, height);
}

//! Sets the fog mode.
void COpenGLDriver::setFog(SColor c, E_FOG_TYPE fogType, f32 start,
		f32 end, f32 density, bool pixelFog, bool rangeFog)
{
	CNullDriver::setFog(c, fogType, start, end, density, pixelFog, rangeFog);

	glFogf(GL_FOG_MODE, GLfloat((fogType == EFT_FOG_LINEAR) ? GL_LINEAR : (fogType == EFT_FOG_EXP) ? GL_EXP
																								   : GL_EXP2));

#ifdef GL_EXT_fog_coord
	if (FeatureAvailable[IRR_EXT_fog_coord])
		glFogi(GL_FOG_COORDINATE_SOURCE, GL_FRAGMENT_DEPTH);
#endif
#ifdef GL_NV_fog_distance
	if (FeatureAvailable[IRR_NV_fog_distance]) {
		if (rangeFog)
			glFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_RADIAL_NV);
		else
			glFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_PLANE_ABSOLUTE_NV);
	}
#endif

	if (fogType == EFT_FOG_LINEAR) {
		glFogf(GL_FOG_START, start);
		glFogf(GL_FOG_END, end);
	} else
		glFogf(GL_FOG_DENSITY, density);

	if (pixelFog)
		glHint(GL_FOG_HINT, GL_NICEST);
	else
		glHint(GL_FOG_HINT, GL_FASTEST);

	SColorf color(c);
	GLfloat data[4] = {color.r, color.g, color.b, color.a};
	glFogfv(GL_FOG_COLOR, data);
}

//! Draws a 3d box.
void COpenGLDriver::draw3DBox(const core::aabbox3d<f32> &box, SColor color)
{
	core::vector3df edges[8];
	box.getEdges(edges);

	setRenderStates3DMode();

	video::S3DVertex v[24];

	for (u32 i = 0; i < 24; i++)
		v[i].Color = color;

	v[0].Pos = edges[5];
	v[1].Pos = edges[1];
	v[2].Pos = edges[1];
	v[3].Pos = edges[3];
	v[4].Pos = edges[3];
	v[5].Pos = edges[7];
	v[6].Pos = edges[7];
	v[7].Pos = edges[5];
	v[8].Pos = edges[0];
	v[9].Pos = edges[2];
	v[10].Pos = edges[2];
	v[11].Pos = edges[6];
	v[12].Pos = edges[6];
	v[13].Pos = edges[4];
	v[14].Pos = edges[4];
	v[15].Pos = edges[0];
	v[16].Pos = edges[1];
	v[17].Pos = edges[0];
	v[18].Pos = edges[3];
	v[19].Pos = edges[2];
	v[20].Pos = edges[7];
	v[21].Pos = edges[6];
	v[22].Pos = edges[5];
	v[23].Pos = edges[4];

	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(v, 24, EVT_STANDARD);

	CacheHandler->setClientState(true, false, true, false);

	glVertexPointer(3, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(v))[0].Pos);

#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(v))[0].Color);
	else {
		_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
	}

	glDrawArrays(GL_LINES, 0, 24);
}

//! Draws a 3d line.
void COpenGLDriver::draw3DLine(const core::vector3df &start,
		const core::vector3df &end, SColor color)
{
	setRenderStates3DMode();

	Quad2DVertices[0].Color = color;
	Quad2DVertices[1].Color = color;

	Quad2DVertices[0].Pos = core::vector3df((f32)start.X, (f32)start.Y, (f32)start.Z);
	Quad2DVertices[1].Pos = core::vector3df((f32)end.X, (f32)end.Y, (f32)end.Z);

	if (!FeatureAvailable[IRR_ARB_vertex_array_bgra] && !FeatureAvailable[IRR_EXT_vertex_array_bgra])
		getColorBuffer(Quad2DVertices, 2, EVT_STANDARD);

	CacheHandler->setClientState(true, false, true, false);

	glVertexPointer(3, GL_FLOAT, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Pos);

#ifdef GL_BGRA
	const GLint colorSize = (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra]) ? GL_BGRA : 4;
#else
	const GLint colorSize = 4;
#endif
	if (FeatureAvailable[IRR_ARB_vertex_array_bgra] || FeatureAvailable[IRR_EXT_vertex_array_bgra])
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, sizeof(S3DVertex), &(static_cast<const S3DVertex *>(Quad2DVertices))[0].Color);
	else {
		_IRR_DEBUG_BREAK_IF(ColorBuffer.size() == 0);
		glColorPointer(colorSize, GL_UNSIGNED_BYTE, 0, &ColorBuffer[0]);
	}

	glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, Quad2DIndices);
}

//! Removes a texture from the texture cache and deletes it, freeing lot of memory.
void COpenGLDriver::removeTexture(ITexture *texture)
{
	CacheHandler->getTextureCache().remove(texture);
	CNullDriver::removeTexture(texture);
}

//! Check if the driver supports creating textures with the given color format
bool COpenGLDriver::queryTextureFormat(ECOLOR_FORMAT format) const
{
	GLint dummyInternalFormat;
	GLenum dummyPixelFormat;
	GLenum dummyPixelType;
	void (*dummyConverter)(const void *, s32, void *);
	return getColorFormatParameters(format, dummyInternalFormat, dummyPixelFormat, dummyPixelType, &dummyConverter);
}

bool COpenGLDriver::needsTransparentRenderPass(const irr::video::SMaterial &material) const
{
	return CNullDriver::needsTransparentRenderPass(material) || material.isAlphaBlendOperation();
}

//! Only used by the internal engine. Used to notify the driver that
//! the window was resized.
void COpenGLDriver::OnResize(const core::dimension2d<u32> &size)
{
	CNullDriver::OnResize(size);
	CacheHandler->setViewport(0, 0, size.Width, size.Height);
	Transformation3DChanged = true;
}

//! Returns type of video driver
E_DRIVER_TYPE COpenGLDriver::getDriverType() const
{
	return EDT_OPENGL;
}

//! returns color format
ECOLOR_FORMAT COpenGLDriver::getColorFormat() const
{
	return ColorFormat;
}

//! Get a vertex shader constant index.
s32 COpenGLDriver::getVertexShaderConstantID(const c8 *name)
{
	return getPixelShaderConstantID(name);
}

//! Get a pixel shader constant index.
s32 COpenGLDriver::getPixelShaderConstantID(const c8 *name)
{
	os::Printer::log("Error: Please call services->getPixelShaderConstantID(), not VideoDriver->getPixelShaderConstantID().");
	return -1;
}

//! Sets a constant for the vertex shader based on an index.
bool COpenGLDriver::setVertexShaderConstant(s32 index, const f32 *floats, int count)
{
	// pass this along, as in GLSL the same routine is used for both vertex and fragment shaders
	return setPixelShaderConstant(index, floats, count);
}

//! Int interface for the above.
bool COpenGLDriver::setVertexShaderConstant(s32 index, const s32 *ints, int count)
{
	return setPixelShaderConstant(index, ints, count);
}

//! Uint interface for the above.
bool COpenGLDriver::setVertexShaderConstant(s32 index, const u32 *ints, int count)
{
	return setPixelShaderConstant(index, ints, count);
}

//! Sets a constant for the pixel shader based on an index.
bool COpenGLDriver::setPixelShaderConstant(s32 index, const f32 *floats, int count)
{
	os::Printer::log("Error: Please call services->setPixelShaderConstant(), not VideoDriver->setPixelShaderConstant().");
	return false;
}

//! Int interface for the above.
bool COpenGLDriver::setPixelShaderConstant(s32 index, const s32 *ints, int count)
{
	os::Printer::log("Error: Please call services->setPixelShaderConstant(), not VideoDriver->setPixelShaderConstant().");
	return false;
}

bool COpenGLDriver::setPixelShaderConstant(s32 index, const u32 *ints, int count)
{
	os::Printer::log("Error: Please call services->setPixelShaderConstant(), not VideoDriver->setPixelShaderConstant().");
	return false;
}

//! Adds a new material renderer to the VideoDriver, using GLSL to render geometry.
s32 COpenGLDriver::addHighLevelShaderMaterial(
		const c8 *vertexShaderProgram,
		const c8 *vertexShaderEntryPointName,
		E_VERTEX_SHADER_TYPE vsCompileTarget,
		const c8 *pixelShaderProgram,
		const c8 *pixelShaderEntryPointName,
		E_PIXEL_SHADER_TYPE psCompileTarget,
		const c8 *geometryShaderProgram,
		const c8 *geometryShaderEntryPointName,
		E_GEOMETRY_SHADER_TYPE gsCompileTarget,
		scene::E_PRIMITIVE_TYPE inType,
		scene::E_PRIMITIVE_TYPE outType,
		u32 verticesOut,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial,
		s32 userData)
{
	s32 nr = -1;

	COpenGLSLMaterialRenderer *r = new COpenGLSLMaterialRenderer(
			this, nr,
			vertexShaderProgram, vertexShaderEntryPointName, vsCompileTarget,
			pixelShaderProgram, pixelShaderEntryPointName, psCompileTarget,
			geometryShaderProgram, geometryShaderEntryPointName, gsCompileTarget,
			inType, outType, verticesOut,
			callback, baseMaterial, userData);

	r->drop();

	return nr;
}

//! Returns a pointer to the IVideoDriver interface. (Implementation for
//! IMaterialRendererServices)
IVideoDriver *COpenGLDriver::getVideoDriver()
{
	return this;
}

ITexture *COpenGLDriver::addRenderTargetTexture(const core::dimension2d<u32> &size,
		const io::path &name, const ECOLOR_FORMAT format)
{
	if (IImage::isCompressedFormat(format))
		return 0;

	// disable mip-mapping
	bool generateMipLevels = getTextureCreationFlag(ETCF_CREATE_MIP_MAPS);
	setTextureCreationFlag(ETCF_CREATE_MIP_MAPS, false);

	bool supportForFBO = (Feature.ColorAttachment > 0);

	core::dimension2du destSize(size);

	if (!supportForFBO) {
		destSize = core::dimension2d<u32>(core::min_(size.Width, ScreenSize.Width), core::min_(size.Height, ScreenSize.Height));
		destSize = destSize.getOptimalSize((size == size.getOptimalSize()), false, false);
	}

	COpenGLTexture *renderTargetTexture = new COpenGLTexture(name, destSize, ETT_2D, format, this);
	addTexture(renderTargetTexture);
	renderTargetTexture->drop();

	// restore mip-mapping
	setTextureCreationFlag(ETCF_CREATE_MIP_MAPS, generateMipLevels);

	return renderTargetTexture;
}

//! Creates a render target texture for a cubemap
ITexture *COpenGLDriver::addRenderTargetTextureCubemap(const irr::u32 sideLen, const io::path &name, const ECOLOR_FORMAT format)
{
	if (IImage::isCompressedFormat(format))
		return 0;

	// disable mip-mapping
	bool generateMipLevels = getTextureCreationFlag(ETCF_CREATE_MIP_MAPS);
	setTextureCreationFlag(ETCF_CREATE_MIP_MAPS, false);

	bool supportForFBO = (Feature.ColorAttachment > 0);

	const core::dimension2d<u32> size(sideLen, sideLen);
	core::dimension2du destSize(size);

	if (!supportForFBO) {
		destSize = core::dimension2d<u32>(core::min_(size.Width, ScreenSize.Width), core::min_(size.Height, ScreenSize.Height));
		destSize = destSize.getOptimalSize((size == size.getOptimalSize()), false, false);
	}

	COpenGLTexture *renderTargetTexture = new COpenGLTexture(name, destSize, ETT_CUBEMAP, format, this);
	addTexture(renderTargetTexture);
	renderTargetTexture->drop();

	// restore mip-mapping
	setTextureCreationFlag(ETCF_CREATE_MIP_MAPS, generateMipLevels);

	return renderTargetTexture;
}

//! Returns the maximum amount of primitives (mostly vertices) which
//! the device is able to render with one drawIndexedTriangleList
//! call.
u32 COpenGLDriver::getMaximalPrimitiveCount() const
{
	return 0x7fffffff;
}

bool COpenGLDriver::setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil)
{
	if (target && target->getDriverType() != EDT_OPENGL) {
		os::Printer::log("Fatal Error: Tried to set a render target not owned by this driver.", ELL_ERROR);
		return false;
	}

	bool supportForFBO = (Feature.ColorAttachment > 0);

	core::dimension2d<u32> destRenderTargetSize(0, 0);

	if (target) {
		COpenGLRenderTarget *renderTarget = static_cast<COpenGLRenderTarget *>(target);

		if (supportForFBO) {
			CacheHandler->setFBO(renderTarget->getBufferID());
			renderTarget->update();
		}

		destRenderTargetSize = renderTarget->getSize();

		setViewPortRaw(destRenderTargetSize.Width, destRenderTargetSize.Height);
	} else {
		if (supportForFBO)
			CacheHandler->setFBO(0);
		else {
			COpenGLRenderTarget *prevRenderTarget = static_cast<COpenGLRenderTarget *>(CurrentRenderTarget);
			COpenGLTexture *renderTargetTexture = static_cast<COpenGLTexture *>(prevRenderTarget->getTexture());

			if (renderTargetTexture) {
				const COpenGLTexture *prevTexture = CacheHandler->getTextureCache()[0];

				CacheHandler->getTextureCache().set(0, renderTargetTexture);

				const core::dimension2d<u32> size = renderTargetTexture->getSize();
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size.Width, size.Height);

				CacheHandler->getTextureCache().set(0, prevTexture);
			}
		}

		destRenderTargetSize = core::dimension2d<u32>(0, 0);

		setViewPortRaw(ScreenSize.Width, ScreenSize.Height);
	}

	if (CurrentRenderTargetSize != destRenderTargetSize) {
		CurrentRenderTargetSize = destRenderTargetSize;

		Transformation3DChanged = true;
	}

	CurrentRenderTarget = target;

	if (!supportForFBO) {
		clearFlag |= ECBF_COLOR;
		clearFlag |= ECBF_DEPTH;
	}

	clearBuffers(clearFlag, clearColor, clearDepth, clearStencil);

	return true;
}

void COpenGLDriver::clearBuffers(u16 flag, SColor color, f32 depth, u8 stencil)
{
	GLbitfield mask = 0;
	u8 colorMask = 0;
	bool depthMask = false;

	CacheHandler->getColorMask(colorMask);
	CacheHandler->getDepthMask(depthMask);

	if (flag & ECBF_COLOR) {
		CacheHandler->setColorMask(ECP_ALL);

		const f32 inv = 1.0f / 255.0f;
		glClearColor(color.getRed() * inv, color.getGreen() * inv,
				color.getBlue() * inv, color.getAlpha() * inv);

		mask |= GL_COLOR_BUFFER_BIT;
	}

	if (flag & ECBF_DEPTH) {
		CacheHandler->setDepthMask(true);
		glClearDepth(depth);
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	if (flag & ECBF_STENCIL) {
		glClearStencil(stencil);
		mask |= GL_STENCIL_BUFFER_BIT;
	}

	if (mask)
		glClear(mask);

	CacheHandler->setColorMask(colorMask);
	CacheHandler->setDepthMask(depthMask);
}

//! Returns an image created from the last rendered frame.
IImage *COpenGLDriver::createScreenShot(video::ECOLOR_FORMAT format, video::E_RENDER_TARGET target)
{
	if (target != video::ERT_FRAME_BUFFER)
		return 0;

	if (format == video::ECF_UNKNOWN)
		format = getColorFormat();

	// TODO: Maybe we could support more formats (floating point and some of those beyond ECF_R8), didn't really try yet
	if (IImage::isCompressedFormat(format) || IImage::isDepthFormat(format) || IImage::isFloatingPointFormat(format) || format >= ECF_R8)
		return 0;

		// allows to read pixels in top-to-bottom order
#ifdef GL_MESA_pack_invert
	if (FeatureAvailable[IRR_MESA_pack_invert])
		glPixelStorei(GL_PACK_INVERT_MESA, GL_TRUE);
#endif

	GLenum fmt;
	GLenum type;

	switch (format) {
	case ECF_A1R5G5B5:
		fmt = GL_BGRA;
		type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		break;
	case ECF_R5G6B5:
		fmt = GL_RGB;
		type = GL_UNSIGNED_SHORT_5_6_5;
		break;
	case ECF_R8G8B8:
		fmt = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case ECF_A8R8G8B8:
		fmt = GL_BGRA;
		if (Version > 101)
			type = GL_UNSIGNED_INT_8_8_8_8_REV;
		else
			type = GL_UNSIGNED_BYTE;
		break;
	default:
		fmt = GL_BGRA;
		type = GL_UNSIGNED_BYTE;
		break;
	}
	IImage *newImage = createImage(format, ScreenSize);

	u8 *pixels = 0;
	if (newImage)
		pixels = static_cast<u8 *>(newImage->getData());
	if (pixels) {
		glReadBuffer(Params.Doublebuffer ? GL_BACK : GL_FRONT);
		glReadPixels(0, 0, ScreenSize.Width, ScreenSize.Height, fmt, type, pixels);
		testGLError(__LINE__);
		glReadBuffer(GL_BACK);
	}

#ifdef GL_MESA_pack_invert
	if (FeatureAvailable[IRR_MESA_pack_invert])
		glPixelStorei(GL_PACK_INVERT_MESA, GL_FALSE);
	else
#endif
			if (pixels && newImage) {
		// opengl images are horizontally flipped, so we have to fix that here.
		const s32 pitch = newImage->getPitch();
		u8 *p2 = pixels + (ScreenSize.Height - 1) * pitch;
		u8 *tmpBuffer = new u8[pitch];
		for (u32 i = 0; i < ScreenSize.Height; i += 2) {
			memcpy(tmpBuffer, pixels, pitch);
			//			for (u32 j=0; j<pitch; ++j)
			//			{
			//				pixels[j]=(u8)(p2[j]*255.f);
			//			}
			memcpy(pixels, p2, pitch);
			//			for (u32 j=0; j<pitch; ++j)
			//			{
			//				p2[j]=(u8)(tmpBuffer[j]*255.f);
			//			}
			memcpy(p2, tmpBuffer, pitch);
			pixels += pitch;
			p2 -= pitch;
		}
		delete[] tmpBuffer;
	}

	if (newImage) {
		if (testGLError(__LINE__) || !pixels) {
			os::Printer::log("createScreenShot failed", ELL_ERROR);
			newImage->drop();
			return 0;
		}
	}
	return newImage;
}

//! Set/unset a clipping plane.
bool COpenGLDriver::setClipPlane(u32 index, const core::plane3df &plane, bool enable)
{
	if (index >= MaxUserClipPlanes)
		return false;

	UserClipPlanes[index].Plane = plane;
	enableClipPlane(index, enable);
	return true;
}

void COpenGLDriver::uploadClipPlane(u32 index)
{
	// opengl needs an array of doubles for the plane equation
	GLdouble clip_plane[4];
	clip_plane[0] = UserClipPlanes[index].Plane.Normal.X;
	clip_plane[1] = UserClipPlanes[index].Plane.Normal.Y;
	clip_plane[2] = UserClipPlanes[index].Plane.Normal.Z;
	clip_plane[3] = UserClipPlanes[index].Plane.D;
	glClipPlane(GL_CLIP_PLANE0 + index, clip_plane);
}

//! Enable/disable a clipping plane.
void COpenGLDriver::enableClipPlane(u32 index, bool enable)
{
	if (index >= MaxUserClipPlanes)
		return;
	if (enable) {
		if (!UserClipPlanes[index].Enabled) {
			uploadClipPlane(index);
			glEnable(GL_CLIP_PLANE0 + index);
		}
	} else
		glDisable(GL_CLIP_PLANE0 + index);

	UserClipPlanes[index].Enabled = enable;
}

core::dimension2du COpenGLDriver::getMaxTextureSize() const
{
	return core::dimension2du(MaxTextureSize, MaxTextureSize);
}

//! Convert E_PRIMITIVE_TYPE to OpenGL equivalent
GLenum COpenGLDriver::primitiveTypeToGL(scene::E_PRIMITIVE_TYPE type) const
{
	switch (type) {
	case scene::EPT_POINTS:
		return GL_POINTS;
	case scene::EPT_LINE_STRIP:
		return GL_LINE_STRIP;
	case scene::EPT_LINE_LOOP:
		return GL_LINE_LOOP;
	case scene::EPT_LINES:
		return GL_LINES;
	case scene::EPT_TRIANGLE_STRIP:
		return GL_TRIANGLE_STRIP;
	case scene::EPT_TRIANGLE_FAN:
		return GL_TRIANGLE_FAN;
	case scene::EPT_TRIANGLES:
		return GL_TRIANGLES;
	case scene::EPT_POINT_SPRITES:
#ifdef GL_ARB_point_sprite
		return GL_POINT_SPRITE_ARB;
#else
		return GL_POINTS;
#endif
	}
	return GL_TRIANGLES;
}

GLenum COpenGLDriver::getGLBlend(E_BLEND_FACTOR factor) const
{
	GLenum r = 0;
	switch (factor) {
	case EBF_ZERO:
		r = GL_ZERO;
		break;
	case EBF_ONE:
		r = GL_ONE;
		break;
	case EBF_DST_COLOR:
		r = GL_DST_COLOR;
		break;
	case EBF_ONE_MINUS_DST_COLOR:
		r = GL_ONE_MINUS_DST_COLOR;
		break;
	case EBF_SRC_COLOR:
		r = GL_SRC_COLOR;
		break;
	case EBF_ONE_MINUS_SRC_COLOR:
		r = GL_ONE_MINUS_SRC_COLOR;
		break;
	case EBF_SRC_ALPHA:
		r = GL_SRC_ALPHA;
		break;
	case EBF_ONE_MINUS_SRC_ALPHA:
		r = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case EBF_DST_ALPHA:
		r = GL_DST_ALPHA;
		break;
	case EBF_ONE_MINUS_DST_ALPHA:
		r = GL_ONE_MINUS_DST_ALPHA;
		break;
	case EBF_SRC_ALPHA_SATURATE:
		r = GL_SRC_ALPHA_SATURATE;
		break;
	}
	return r;
}

GLenum COpenGLDriver::getZBufferBits() const
{
	GLenum bits = 0;
	switch (Params.ZBufferBits) {
	case 16:
		bits = GL_DEPTH_COMPONENT16;
		break;
	case 24:
		bits = GL_DEPTH_COMPONENT24;
		break;
	case 32:
		bits = GL_DEPTH_COMPONENT32;
		break;
	default:
		bits = GL_DEPTH_COMPONENT;
		break;
	}
	return bits;
}

bool COpenGLDriver::getColorFormatParameters(ECOLOR_FORMAT format, GLint &internalFormat, GLenum &pixelFormat,
		GLenum &pixelType, void (**converter)(const void *, s32, void *)) const
{
	// NOTE: Converter variable not used here, but don't remove, it's used in the OGL-ES drivers.

	bool supported = false;
	internalFormat = GL_RGBA;
	pixelFormat = GL_RGBA;
	pixelType = GL_UNSIGNED_BYTE;

	switch (format) {
	case ECF_A1R5G5B5:
		supported = true;
		internalFormat = GL_RGBA;
		pixelFormat = GL_BGRA_EXT;
		pixelType = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		break;
	case ECF_R5G6B5:
		supported = true;
		internalFormat = GL_RGB;
		pixelFormat = GL_RGB;
		pixelType = GL_UNSIGNED_SHORT_5_6_5;
		break;
	case ECF_R8G8B8:
		supported = true;
		internalFormat = GL_RGB;
		pixelFormat = GL_RGB;
		pixelType = GL_UNSIGNED_BYTE;
		break;
	case ECF_A8R8G8B8:
		supported = true;
		internalFormat = GL_RGBA;
		pixelFormat = GL_BGRA_EXT;
		if (Version > 101)
			pixelType = GL_UNSIGNED_INT_8_8_8_8_REV;
		break;
	case ECF_D16:
		supported = true;
		internalFormat = GL_DEPTH_COMPONENT16;
		pixelFormat = GL_DEPTH_COMPONENT;
		pixelType = GL_UNSIGNED_SHORT;
		break;
	case ECF_D32:
		supported = true;
		internalFormat = GL_DEPTH_COMPONENT32;
		pixelFormat = GL_DEPTH_COMPONENT;
		pixelType = GL_UNSIGNED_INT;
		break;
	case ECF_D24S8:
#ifdef GL_VERSION_3_0
		if (Version >= 300) {
			supported = true;
			internalFormat = GL_DEPTH_STENCIL;
			pixelFormat = GL_DEPTH_STENCIL;
			pixelType = GL_UNSIGNED_INT_24_8;
		} else
#endif
#ifdef GL_EXT_packed_depth_stencil
				if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_EXT_packed_depth_stencil)) {
			supported = true;
			internalFormat = GL_DEPTH_STENCIL_EXT;
			pixelFormat = GL_DEPTH_STENCIL_EXT;
			pixelType = GL_UNSIGNED_INT_24_8_EXT;
		}
#endif
		break;
	case ECF_R8:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_R8;
			pixelFormat = GL_RED;
			pixelType = GL_UNSIGNED_BYTE;
		}
		break;
	case ECF_R8G8:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_RG8;
			pixelFormat = GL_RG;
			pixelType = GL_UNSIGNED_BYTE;
		}
		break;
	case ECF_R16:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_R16;
			pixelFormat = GL_RED;
			pixelType = GL_UNSIGNED_SHORT;
		}
		break;
	case ECF_R16G16:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_RG16;
			pixelFormat = GL_RG;
			pixelType = GL_UNSIGNED_SHORT;
		}
		break;
	case ECF_R16F:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_R16F;
			pixelFormat = GL_RED;
#ifdef GL_ARB_half_float_pixel
			if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_half_float_pixel))
				pixelType = GL_HALF_FLOAT_ARB;
			else
#endif
				pixelType = GL_FLOAT;
		}
		break;
	case ECF_G16R16F:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_RG16F;
			pixelFormat = GL_RG;
#ifdef GL_ARB_half_float_pixel
			if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_half_float_pixel))
				pixelType = GL_HALF_FLOAT_ARB;
			else
#endif
				pixelType = GL_FLOAT;
		}
		break;
	case ECF_A16B16G16R16F:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_float)) {
			supported = true;
			internalFormat = GL_RGBA16F_ARB;
			pixelFormat = GL_RGBA;
#ifdef GL_ARB_half_float_pixel
			if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_half_float_pixel))
				pixelType = GL_HALF_FLOAT_ARB;
			else
#endif
				pixelType = GL_FLOAT;
		}
		break;
	case ECF_R32F:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_R32F;
			pixelFormat = GL_RED;
			pixelType = GL_FLOAT;
		}
		break;
	case ECF_G32R32F:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_rg)) {
			supported = true;
			internalFormat = GL_RG32F;
			pixelFormat = GL_RG;
			pixelType = GL_FLOAT;
		}
		break;
	case ECF_A32B32G32R32F:
		if (queryOpenGLFeature(COpenGLExtensionHandler::IRR_ARB_texture_float)) {
			supported = true;
			internalFormat = GL_RGBA32F_ARB;
			pixelFormat = GL_RGBA;
			pixelType = GL_FLOAT;
		}
		break;
	default:
		break;
	}

	return supported;
}

COpenGLDriver::E_OPENGL_FIXED_PIPELINE_STATE COpenGLDriver::getFixedPipelineState() const
{
	return FixedPipelineState;
}

void COpenGLDriver::setFixedPipelineState(COpenGLDriver::E_OPENGL_FIXED_PIPELINE_STATE state)
{
	FixedPipelineState = state;
}

const SMaterial &COpenGLDriver::getCurrentMaterial() const
{
	return Material;
}

COpenGLCacheHandler *COpenGLDriver::getCacheHandler() const
{
	return CacheHandler;
}

} // end namespace
} // end namespace

#endif // _IRR_COMPILE_WITH_OPENGL_

namespace irr
{
namespace video
{

IVideoDriver *createOpenGLDriver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
#ifdef _IRR_COMPILE_WITH_OPENGL_
	COpenGLDriver *ogl = new COpenGLDriver(params, io, contextManager);

	if (!ogl->initDriver()) {
		ogl->drop();
		ogl = 0;
	}

	return ogl;
#else
	return 0;
#endif
}

} // end namespace
} // end namespace
