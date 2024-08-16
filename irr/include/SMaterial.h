// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "SColor.h"
#include "matrix4.h"
#include "irrArray.h"
#include "irrMath.h"
#include "EMaterialTypes.h"
#include "EMaterialProps.h"
#include "SMaterialLayer.h"
#include "IrrCompileConfig.h" // for IRRLICHT_API

namespace irr
{
namespace video
{
class ITexture;

//! Flag for MaterialTypeParam (in combination with EMT_ONETEXTURE_BLEND) or for BlendFactor
//! BlendFunc = source * sourceFactor + dest * destFactor
enum E_BLEND_FACTOR
{
	EBF_ZERO = 0,            //!< src & dest	(0, 0, 0, 0)
	EBF_ONE,                 //!< src & dest	(1, 1, 1, 1)
	EBF_DST_COLOR,           //!< src	(destR, destG, destB, destA)
	EBF_ONE_MINUS_DST_COLOR, //!< src	(1-destR, 1-destG, 1-destB, 1-destA)
	EBF_SRC_COLOR,           //!< dest	(srcR, srcG, srcB, srcA)
	EBF_ONE_MINUS_SRC_COLOR, //!< dest	(1-srcR, 1-srcG, 1-srcB, 1-srcA)
	EBF_SRC_ALPHA,           //!< src & dest	(srcA, srcA, srcA, srcA)
	EBF_ONE_MINUS_SRC_ALPHA, //!< src & dest	(1-srcA, 1-srcA, 1-srcA, 1-srcA)
	EBF_DST_ALPHA,           //!< src & dest	(destA, destA, destA, destA)
	EBF_ONE_MINUS_DST_ALPHA, //!< src & dest	(1-destA, 1-destA, 1-destA, 1-destA)
	EBF_SRC_ALPHA_SATURATE   //!< src	(min(srcA, 1-destA), idem, ...)
};

//! Values defining the blend operation
enum E_BLEND_OPERATION
{
	EBO_NONE = 0,    //!< No blending happens
	EBO_ADD,         //!< Default blending adds the color values
	EBO_SUBTRACT,    //!< This mode subtracts the color values
	EBO_REVSUBTRACT, //!< This modes subtracts destination from source
	EBO_MIN,         //!< Choose minimum value of each color channel
	EBO_MAX,         //!< Choose maximum value of each color channel
	EBO_MIN_FACTOR,  //!< Choose minimum value of each color channel after applying blend factors, not widely supported
	EBO_MAX_FACTOR,  //!< Choose maximum value of each color channel after applying blend factors, not widely supported
	EBO_MIN_ALPHA,   //!< Choose minimum value of each color channel based on alpha value, not widely supported
	EBO_MAX_ALPHA    //!< Choose maximum value of each color channel based on alpha value, not widely supported
};

//! MaterialTypeParam: e.g. DirectX: D3DTOP_MODULATE, D3DTOP_MODULATE2X, D3DTOP_MODULATE4X
enum E_MODULATE_FUNC
{
	EMFN_MODULATE_1X = 1,
	EMFN_MODULATE_2X = 2,
	EMFN_MODULATE_4X = 4
};

//! Comparison function, e.g. for depth buffer test
enum E_COMPARISON_FUNC
{
	//! Depth test disabled (disable also write to depth buffer)
	ECFN_DISABLED = 0,
	//! <= test, default for e.g. depth test
	ECFN_LESSEQUAL = 1,
	//! Exact equality
	ECFN_EQUAL = 2,
	//! exclusive less comparison, i.e. <
	ECFN_LESS,
	//! Succeeds almost always, except for exact equality
	ECFN_NOTEQUAL,
	//! >= test
	ECFN_GREATEREQUAL,
	//! inverse of <=
	ECFN_GREATER,
	//! test succeeds always
	ECFN_ALWAYS,
	//! Test never succeeds
	ECFN_NEVER
};

//! Enum values for enabling/disabling color planes for rendering
enum E_COLOR_PLANE
{
	//! No color enabled
	ECP_NONE = 0,
	//! Alpha enabled
	ECP_ALPHA = 1,
	//! Red enabled
	ECP_RED = 2,
	//! Green enabled
	ECP_GREEN = 4,
	//! Blue enabled
	ECP_BLUE = 8,
	//! All colors, no alpha
	ECP_RGB = 14,
	//! All planes enabled
	ECP_ALL = 15
};

//! Source of the alpha value to take
/** This is currently only supported in EMT_ONETEXTURE_BLEND. You can use an
or'ed combination of values. Alpha values are modulated (multiplied). */
enum E_ALPHA_SOURCE
{
	//! Use no alpha, somewhat redundant with other settings
	EAS_NONE = 0,
	//! Use vertex color alpha
	EAS_VERTEX_COLOR,
	//! Use texture alpha channel
	EAS_TEXTURE
};

//! Pack srcFact, dstFact, Modulate and alpha source to MaterialTypeParam or BlendFactor
/** alpha source can be an OR'ed combination of E_ALPHA_SOURCE values. */
inline f32 pack_textureBlendFunc(const E_BLEND_FACTOR srcFact, const E_BLEND_FACTOR dstFact,
		const E_MODULATE_FUNC modulate = EMFN_MODULATE_1X, const u32 alphaSource = EAS_TEXTURE)
{
	const u32 tmp = (alphaSource << 20) | (modulate << 16) | (srcFact << 12) | (dstFact << 8) | (srcFact << 4) | dstFact;
	return FR(tmp);
}

//! Pack srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, Modulate and alpha source to MaterialTypeParam or BlendFactor
/** alpha source can be an OR'ed combination of E_ALPHA_SOURCE values. */
inline f32 pack_textureBlendFuncSeparate(const E_BLEND_FACTOR srcRGBFact, const E_BLEND_FACTOR dstRGBFact,
		const E_BLEND_FACTOR srcAlphaFact, const E_BLEND_FACTOR dstAlphaFact,
		const E_MODULATE_FUNC modulate = EMFN_MODULATE_1X, const u32 alphaSource = EAS_TEXTURE)
{
	const u32 tmp = (alphaSource << 20) | (modulate << 16) | (srcAlphaFact << 12) | (dstAlphaFact << 8) | (srcRGBFact << 4) | dstRGBFact;
	return FR(tmp);
}

//! Unpack srcFact, dstFact, modulo and alphaSource factors
/** The fields don't use the full byte range, so we could pack even more... */
inline void unpack_textureBlendFunc(E_BLEND_FACTOR &srcFact, E_BLEND_FACTOR &dstFact,
		E_MODULATE_FUNC &modulo, u32 &alphaSource, const f32 param)
{
	const u32 state = IR(param);
	alphaSource = (state & 0x00F00000) >> 20;
	modulo = E_MODULATE_FUNC((state & 0x000F0000) >> 16);
	srcFact = E_BLEND_FACTOR((state & 0x000000F0) >> 4);
	dstFact = E_BLEND_FACTOR((state & 0x0000000F));
}

//! Unpack srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulo and alphaSource factors
/** The fields don't use the full byte range, so we could pack even more... */
inline void unpack_textureBlendFuncSeparate(E_BLEND_FACTOR &srcRGBFact, E_BLEND_FACTOR &dstRGBFact,
		E_BLEND_FACTOR &srcAlphaFact, E_BLEND_FACTOR &dstAlphaFact,
		E_MODULATE_FUNC &modulo, u32 &alphaSource, const f32 param)
{
	const u32 state = IR(param);
	alphaSource = (state & 0x00F00000) >> 20;
	modulo = E_MODULATE_FUNC((state & 0x000F0000) >> 16);
	srcAlphaFact = E_BLEND_FACTOR((state & 0x0000F000) >> 12);
	dstAlphaFact = E_BLEND_FACTOR((state & 0x00000F00) >> 8);
	srcRGBFact = E_BLEND_FACTOR((state & 0x000000F0) >> 4);
	dstRGBFact = E_BLEND_FACTOR((state & 0x0000000F));
}

//! has blend factor alphablending
inline bool textureBlendFunc_hasAlpha(const E_BLEND_FACTOR factor)
{
	switch (factor) {
	case EBF_SRC_ALPHA:
	case EBF_ONE_MINUS_SRC_ALPHA:
	case EBF_DST_ALPHA:
	case EBF_ONE_MINUS_DST_ALPHA:
	case EBF_SRC_ALPHA_SATURATE:
		return true;
	default:
		return false;
	}
}

//! These flags are used to specify the anti-aliasing and smoothing modes
/** Techniques supported are multisampling, geometry smoothing, and alpha
to coverage.
Some drivers don't support a per-material setting of the anti-aliasing
modes. In those cases, FSAA/multisampling is defined by the device mode
chosen upon creation via irr::SIrrCreationParameters.
*/
enum E_ANTI_ALIASING_MODE
{
	//! Use to turn off anti-aliasing for this material
	EAAM_OFF = 0,
	//! Default anti-aliasing mode
	EAAM_SIMPLE = 1,
	//! High-quality anti-aliasing, not always supported, automatically enables SIMPLE mode
	EAAM_QUALITY = 3,
	//! Enhanced anti-aliasing for transparent materials
	/** Usually used with EMT_TRANSPARENT_ALPHA_CHANNEL_REF and multisampling. */
	EAAM_ALPHA_TO_COVERAGE = 4
};

//! These flags allow to define the interpretation of vertex color when lighting is enabled
/** Without lighting being enabled the vertex color is the only value defining the fragment color.
Once lighting is enabled, the four values for diffuse, ambient, emissive, and specular take over.
With these flags it is possible to define which lighting factor shall be defined by the vertex color
instead of the lighting factor which is the same for all faces of that material.
The default is to use vertex color for the diffuse value, another pretty common value is to use
vertex color for both diffuse and ambient factor. */
enum E_COLOR_MATERIAL
{
	//! Don't use vertex color for lighting
	ECM_NONE = 0,
	//! Use vertex color for diffuse light, this is default
	ECM_DIFFUSE,
	//! Use vertex color for ambient light
	ECM_AMBIENT,
	//! Use vertex color for emissive light
	ECM_EMISSIVE,
	//! Use vertex color for specular light
	ECM_SPECULAR,
	//! Use vertex color for both diffuse and ambient light
	ECM_DIFFUSE_AND_AMBIENT
};

//! Names for polygon offset direction
const c8 *const PolygonOffsetDirectionNames[] = {
		"Back",
		"Front",
		0,
	};

//! For SMaterial.ZWriteEnable
enum E_ZWRITE
{
	//! zwrite always disabled for this material
	EZW_OFF = 0,

	//! This is the default setting for SMaterial and tries to handle things automatically.
	//! This is what you want to set to enable zwriting.
	//! Usually zwriting is enabled non-transparent materials - as far as Irrlicht can recognize those.
	//! Basically Irrlicht tries to handle the zwriting for you and assumes transparent materials don't need it.
	//! This is addionally affected by IVideoDriver::setAllowZWriteOnTransparent
	EZW_AUTO,

	//! zwrite always enabled for this material
	EZW_ON
};

//! Names for E_ZWRITE
const c8 *const ZWriteNames[] = {
		"Off",
		"Auto",
		"On",
		0,
	};

//! Maximum number of texture an SMaterial can have.
/** SMaterial might ignore some textures in most function, like assignment and comparison,
	when SIrrlichtCreationParameters::MaxTextureUnits is set to a lower number.
*/
const u32 MATERIAL_MAX_TEXTURES = 4;

//! Struct for holding parameters for a material renderer
// Note for implementors: Serialization is in CNullDriver
class SMaterial
{
public:
	//! Default constructor. Creates a solid, lit material with white colors
	SMaterial() :
			MaterialType(EMT_SOLID), AmbientColor(255, 255, 255, 255),
			DiffuseColor(255, 255, 255, 255), EmissiveColor(0, 0, 0, 0),
			SpecularColor(255, 255, 255, 255), Shininess(0.0f),
			MaterialTypeParam(0.0f), Thickness(1.0f), ZBuffer(ECFN_LESSEQUAL),
			AntiAliasing(EAAM_SIMPLE), ColorMask(ECP_ALL), ColorMaterial(ECM_DIFFUSE),
			BlendOperation(EBO_NONE), BlendFactor(0.0f), PolygonOffsetDepthBias(0.f),
			PolygonOffsetSlopeScale(0.f), Wireframe(false), PointCloud(false),
			GouraudShading(true), Lighting(true), ZWriteEnable(EZW_AUTO),
			BackfaceCulling(true), FrontfaceCulling(false), FogEnable(false),
			NormalizeNormals(false), UseMipMaps(true)
	{
	}

	//! Texture layer array.
	SMaterialLayer TextureLayers[MATERIAL_MAX_TEXTURES];

	//! Type of the material. Specifies how everything is blended together
	E_MATERIAL_TYPE MaterialType;

	//! How much ambient light (a global light) is reflected by this material.
	/** The default is full white, meaning objects are completely
	globally illuminated. Reduce this if you want to see diffuse
	or specular light effects. */
	SColor AmbientColor;

	//! How much diffuse light coming from a light source is reflected by this material.
	/** The default is full white. */
	SColor DiffuseColor;

	//! Light emitted by this material. Default is to emit no light.
	SColor EmissiveColor;

	//! How much specular light (highlights from a light) is reflected.
	/** The default is to reflect white specular light. See
	SMaterial::Shininess on how to enable specular lights. */
	SColor SpecularColor;

	//! Value affecting the size of specular highlights.
	/** A value of 20 is common. If set to 0, no specular
	highlights are being used. To activate, simply set the
	shininess of a material to a value in the range [0.5;128]:
	\code
	sceneNode->getMaterial(0).Shininess = 20.0f;
	\endcode

	You can change the color of the highlights using
	\code
	sceneNode->getMaterial(0).SpecularColor.set(255,255,255,255);
	\endcode

	The specular color of the dynamic lights
	(SLight::SpecularColor) will influence the the highlight color
	too, but they are set to a useful value by default when
	creating the light scene node.*/
	f32 Shininess;

	//! Free parameter, dependent on the material type.
	/** Mostly ignored, used for example in
	EMT_TRANSPARENT_ALPHA_CHANNEL and EMT_ONETEXTURE_BLEND. */
	f32 MaterialTypeParam;

	//! Thickness of non-3dimensional elements such as lines and points.
	f32 Thickness;

	//! Is the ZBuffer enabled? Default: ECFN_LESSEQUAL
	/** If you want to disable depth test for this material
	just set this parameter to ECFN_DISABLED.
	Values are from E_COMPARISON_FUNC. */
	u8 ZBuffer;

	//! Sets the antialiasing mode
	/** Values are chosen from E_ANTI_ALIASING_MODE. Default is
	EAAM_SIMPLE, i.e. simple multi-sample anti-aliasing. */
	u8 AntiAliasing;

	//! Defines the enabled color planes
	/** Values are defined as or'ed values of the E_COLOR_PLANE enum.
	Only enabled color planes will be rendered to the current render
	target. Typical use is to disable all colors when rendering only to
	depth or stencil buffer, or using Red and Green for Stereo rendering. */
	u8 ColorMask : 4;

	//! Defines the interpretation of vertex color in the lighting equation
	/** Values should be chosen from E_COLOR_MATERIAL.
	When lighting is enabled, vertex color can be used instead of the
	material values for light modulation. This allows to easily change e.g. the
	diffuse light behavior of each face. The default, ECM_DIFFUSE, will result in
	a very similar rendering as with lighting turned off, just with light shading. */
	u8 ColorMaterial : 3;

	//! Store the blend operation of choice
	/** Values to be chosen from E_BLEND_OPERATION. */
	E_BLEND_OPERATION BlendOperation : 4;

	//! Store the blend factors
	/** textureBlendFunc/textureBlendFuncSeparate functions should be used to write
	properly blending factors to this parameter.
	Due to historical reasons this parameter is not used for material type
	EMT_ONETEXTURE_BLEND which uses MaterialTypeParam instead for the blend factor.
	It's generally used only for materials without any blending otherwise (like EMT_SOLID).
	It's main use is to allow having shader materials which can enable/disable
	blending after they have been created.
	When you set this you usually also have to set BlendOperation to a value != EBO_NONE
	(setting it to EBO_ADD is probably the most common one value). */
	f32 BlendFactor;

	//! A constant z-buffer offset for a polygon/line/point
	/** The range of the value is driver specific.
	On OpenGL you get units which are multiplied by the smallest value that is guaranteed to produce a resolvable offset.
	On D3D9 you can pass a range between -1 and 1. But you should likely divide it by the range of the depthbuffer.
	Like dividing by 65535.0 for a 16 bit depthbuffer. Thought it still might produce too large of a bias.
	Some article (https://aras-p.info/blog/2008/06/12/depth-bias-and-the-power-of-deceiving-yourself/)
	recommends multiplying by 2.0*4.8e-7 (and strangely on both 16 bit and 24 bit).	*/
	f32 PolygonOffsetDepthBias;

	//! Variable Z-Buffer offset based on the slope of the polygon.
	/** For polygons looking flat at a camera you could use 0 (for example in a 2D game)
	But in most cases you will have polygons rendered at a certain slope.
	The driver will calculate the slope for you and this value allows to scale that slope.
	The complete polygon offset is: PolygonOffsetSlopeScale*slope + PolygonOffsetDepthBias
	A good default here is to use 1.f if you want to push the polygons away from the camera
	and -1.f to pull them towards the camera.  */
	f32 PolygonOffsetSlopeScale;

	//! Draw as wireframe or filled triangles? Default: false
	bool Wireframe : 1;

	//! Draw as point cloud or filled triangles? Default: false
	bool PointCloud : 1;

	//! Flat or Gouraud shading? Default: true
	bool GouraudShading : 1;

	//! Will this material be lighted? Default: true
	bool Lighting : 1;

	//! Is the zbuffer writable or is it read-only. Default: EZW_AUTO.
	/** If this parameter is not EZW_OFF, you probably also want to set ZBuffer
	to values other than ECFN_DISABLED */
	E_ZWRITE ZWriteEnable : 2;

	//! Is backface culling enabled? Default: true
	bool BackfaceCulling : 1;

	//! Is frontface culling enabled? Default: false
	bool FrontfaceCulling : 1;

	//! Is fog enabled? Default: false
	bool FogEnable : 1;

	//! Should normals be normalized?
	/** Always use this if the mesh lit and scaled. Default: false */
	bool NormalizeNormals : 1;

	//! Shall mipmaps be used if available
	/** Sometimes, disabling mipmap usage can be useful. Default: true */
	bool UseMipMaps : 1;

	//! Execute a function on all texture layers.
	/** Useful for setting properties which are not per material, but per
	texture layer, e.g. bilinear filtering. */
	template <typename F>
	void forEachTexture(F &&fn)
	{
		for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; i++) {
			fn(TextureLayers[i]);
		}
	}

	//! Gets the texture transformation matrix for level i
	/** \param i The desired level. Must not be larger than MATERIAL_MAX_TEXTURES
	\return Texture matrix for texture level i. */
	core::matrix4 &getTextureMatrix(u32 i)
	{
		return TextureLayers[i].getTextureMatrix();
	}

	//! Gets the immutable texture transformation matrix for level i
	/** \param i The desired level.
	\return Texture matrix for texture level i, or identity matrix for levels larger than MATERIAL_MAX_TEXTURES. */
	const core::matrix4 &getTextureMatrix(u32 i) const
	{
		if (i < MATERIAL_MAX_TEXTURES)
			return TextureLayers[i].getTextureMatrix();
		else
			return core::IdentityMatrix;
	}

	//! Sets the i-th texture transformation matrix
	/** \param i The desired level.
	\param mat Texture matrix for texture level i. */
	void setTextureMatrix(u32 i, const core::matrix4 &mat)
	{
		if (i >= MATERIAL_MAX_TEXTURES)
			return;
		TextureLayers[i].setTextureMatrix(mat);
	}

	//! Gets the i-th texture
	/** \param i The desired level.
	\return Texture for texture level i, if defined, else 0. */
	ITexture *getTexture(u32 i) const
	{
		return i < MATERIAL_MAX_TEXTURES ? TextureLayers[i].Texture : 0;
	}

	//! Sets the i-th texture
	/** If i>=MATERIAL_MAX_TEXTURES this setting will be ignored.
	\param i The desired level.
	\param tex Texture for texture level i. */
	void setTexture(u32 i, ITexture *tex)
	{
		if (i >= MATERIAL_MAX_TEXTURES)
			return;
		TextureLayers[i].Texture = tex;
	}

	//! Inequality operator
	/** \param b Material to compare to.
	\return True if the materials differ, else false. */
	inline bool operator!=(const SMaterial &b) const
	{
		bool different =
				MaterialType != b.MaterialType ||
				AmbientColor != b.AmbientColor ||
				DiffuseColor != b.DiffuseColor ||
				EmissiveColor != b.EmissiveColor ||
				SpecularColor != b.SpecularColor ||
				Shininess != b.Shininess ||
				MaterialTypeParam != b.MaterialTypeParam ||
				Thickness != b.Thickness ||
				Wireframe != b.Wireframe ||
				PointCloud != b.PointCloud ||
				GouraudShading != b.GouraudShading ||
				Lighting != b.Lighting ||
				ZBuffer != b.ZBuffer ||
				ZWriteEnable != b.ZWriteEnable ||
				BackfaceCulling != b.BackfaceCulling ||
				FrontfaceCulling != b.FrontfaceCulling ||
				FogEnable != b.FogEnable ||
				NormalizeNormals != b.NormalizeNormals ||
				AntiAliasing != b.AntiAliasing ||
				ColorMask != b.ColorMask ||
				ColorMaterial != b.ColorMaterial ||
				BlendOperation != b.BlendOperation ||
				BlendFactor != b.BlendFactor ||
				PolygonOffsetDepthBias != b.PolygonOffsetDepthBias ||
				PolygonOffsetSlopeScale != b.PolygonOffsetSlopeScale ||
				UseMipMaps != b.UseMipMaps;
		for (u32 i = 0; (i < MATERIAL_MAX_TEXTURES) && !different; ++i) {
			different |= (TextureLayers[i] != b.TextureLayers[i]);
		}
		return different;
	}

	//! Equality operator
	/** \param b Material to compare to.
	\return True if the materials are equal, else false. */
	inline bool operator==(const SMaterial &b) const
	{
		return !(b != *this);
	}

	//! Check if material needs alpha blending
	bool isAlphaBlendOperation() const
	{
		if (BlendOperation != EBO_NONE && BlendFactor != 0.f) {
			E_BLEND_FACTOR srcRGBFact = EBF_ZERO;
			E_BLEND_FACTOR dstRGBFact = EBF_ZERO;
			E_BLEND_FACTOR srcAlphaFact = EBF_ZERO;
			E_BLEND_FACTOR dstAlphaFact = EBF_ZERO;
			E_MODULATE_FUNC modulo = EMFN_MODULATE_1X;
			u32 alphaSource = 0;

			unpack_textureBlendFuncSeparate(srcRGBFact, dstRGBFact, srcAlphaFact, dstAlphaFact, modulo, alphaSource, BlendFactor);

			if (textureBlendFunc_hasAlpha(srcRGBFact) || textureBlendFunc_hasAlpha(dstRGBFact) ||
					textureBlendFunc_hasAlpha(srcAlphaFact) || textureBlendFunc_hasAlpha(dstAlphaFact)) {
				return true;
			}
		}
		return false;
	}

	//! Check for some fixed-function transparent types. Still used internally, but might be deprecated soon.
	//! You probably should not use this anymore, IVideoDriver::needsTransparentRenderPass is more useful in most situations
	//! as it asks the material renders directly what they do with the material.
	bool isTransparent() const
	{
		if (MaterialType == EMT_TRANSPARENT_ALPHA_CHANNEL ||
				MaterialType == EMT_TRANSPARENT_VERTEX_ALPHA)
			return true;

		return false;
	}
};

//! global const identity Material
IRRLICHT_API extern SMaterial IdentityMaterial;
} // end namespace video
} // end namespace irr
