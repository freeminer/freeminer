/*
tile.h
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

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include <ITexture.h>
#include <set>
#include <string>
#include <vector>
#include <SMaterial.h>
#include "util/numeric.h"
#include "config.h"

class IGameDef;
struct TileSpec;
struct TileDef;

namespace irr::video { class IVideoDriver; }

typedef std::vector<video::SColor> Palette;

/*
	tile.{h,cpp}: Texture handling stuff.
*/

/*
	Find out the full path of an image by trying different filename
	extensions.

	If failed, return "".

	TODO: Should probably be moved out from here, because things needing
	      this function do not need anything else from this header
*/
std::string getImagePath(std::string path);

/*
	Gets the path to a texture by first checking if the texture exists
	in texture_path and if not, using the data path.

	Checks all supported extensions by replacing the original extension.

	If not found, returns "".

	Utilizes a thread-safe cache.
*/
std::string getTexturePath(const std::string &filename, bool *is_base_pack = nullptr);

void clearTextureNameCache();

/*
	Stores internal information about a texture.
*/
struct TextureInfo
{
	std::string name;
	video::ITexture *texture;
	std::set<std::string> sourceImages;

	TextureInfo(
			const std::string &name_,
			video::ITexture *texture_=NULL
		):
		name(name_),
		texture(texture_)
	{
	}

	TextureInfo(
			const std::string &name_,
			video::ITexture *texture_,
			std::set<std::string> &sourceImages_
		):
		name(name_),
		texture(texture_),
		sourceImages(sourceImages_)
	{
	}
};

/*
	TextureSource creates and caches textures.
*/

class ISimpleTextureSource
{
public:
	ISimpleTextureSource() = default;

	virtual ~ISimpleTextureSource() = default;

	virtual video::ITexture* getTexture(
			const std::string &name, u32 *id = nullptr) = 0;
};

class ITextureSource : public ISimpleTextureSource
{
public:
	ITextureSource() = default;

	virtual ~ITextureSource() = default;

	virtual u32 getTextureId(const std::string &name)=0;
	virtual std::string getTextureName(u32 id)=0;
	virtual video::ITexture* getTexture(u32 id)=0;
	virtual TextureInfo* getTextureInfo(u32 id)=0;
	virtual video::ITexture* getTexture(
			const std::string &name, u32 *id = nullptr)=0;
	virtual video::ITexture* getTextureForMesh(
			const std::string &name, u32 *id = nullptr) = 0;
	/*!
	 * Returns a palette from the given texture name.
	 * The pointer is valid until the texture source is
	 * destructed.
	 * Should be called from the main thread.
	 */
	virtual Palette* getPalette(const std::string &name) = 0;
	virtual bool isKnownSourceImage(const std::string &name)=0;
	virtual video::ITexture* getNormalTexture(const std::string &name)=0;
	virtual video::SColor getTextureAverageColor(const std::string &name)=0;
	virtual video::ITexture *getShaderFlagsTexture(bool normalmap_present)=0;
};

class IWritableTextureSource : public ITextureSource
{
public:
	IWritableTextureSource() = default;

	virtual ~IWritableTextureSource() = default;

	virtual u32 getTextureId(const std::string &name)=0;
	virtual std::string getTextureName(u32 id)=0;
	virtual video::ITexture* getTexture(u32 id)=0;
	virtual video::ITexture* getTexture(
			const std::string &name, u32 *id = nullptr)=0;
	virtual bool isKnownSourceImage(const std::string &name)=0;

	virtual void processQueue()=0;
	virtual void insertSourceImage(const std::string &name, video::IImage *img)=0;
	virtual void rebuildImagesAndTextures()=0;
	virtual video::ITexture* getNormalTexture(const std::string &name)=0;
	virtual video::SColor getTextureAverageColor(const std::string &name)=0;
	virtual video::ITexture *getShaderFlagsTexture(bool normalmap_present)=0;
};

IWritableTextureSource *createTextureSource();

video::IImage *Align2Npot2(video::IImage *image, video::IVideoDriver *driver);

enum MaterialType{
	TILE_MATERIAL_BASIC,
	TILE_MATERIAL_ALPHA,
	TILE_MATERIAL_LIQUID_TRANSPARENT,
	TILE_MATERIAL_LIQUID_OPAQUE,
	TILE_MATERIAL_WAVING_LEAVES,
	TILE_MATERIAL_WAVING_PLANTS,
	TILE_MATERIAL_OPAQUE,
	TILE_MATERIAL_WAVING_LIQUID_BASIC,
	TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT,
	TILE_MATERIAL_WAVING_LIQUID_OPAQUE,
	TILE_MATERIAL_PLAIN,
	TILE_MATERIAL_PLAIN_ALPHA
};

// Material flags
// Should backface culling be enabled?
#define MATERIAL_FLAG_BACKFACE_CULLING 0x01
// Should a crack be drawn?
#define MATERIAL_FLAG_CRACK 0x02
// Should the crack be drawn on transparent pixels (unset) or not (set)?
// Ignored if MATERIAL_FLAG_CRACK is not set.
#define MATERIAL_FLAG_CRACK_OVERLAY 0x04
#define MATERIAL_FLAG_ANIMATION 0x08
//#define MATERIAL_FLAG_HIGHLIGHTED 0x10
#define MATERIAL_FLAG_TILEABLE_HORIZONTAL 0x20
#define MATERIAL_FLAG_TILEABLE_VERTICAL 0x40

/*
	This fully defines the looks of a tile.
	The SMaterial of a tile is constructed according to this.
*/
struct FrameSpec
{
	FrameSpec() = default;

	u32 texture_id = 0;
	video::ITexture *texture = nullptr;
	video::ITexture *normal_texture = nullptr;
	video::ITexture *flags_texture = nullptr;
};

#define MAX_TILE_LAYERS 2

//! Defines a layer of a tile.
struct TileLayer
{
	TileLayer() = default;

	/*!
	 * Two layers are equal if they can be merged.
	 */
	bool operator==(const TileLayer &other) const
	{
		return
			texture_id == other.texture_id &&
			material_type == other.material_type &&
			material_flags == other.material_flags &&
			has_color == other.has_color &&
			color == other.color &&
			scale == other.scale;
	}

	/*!
	 * Two tiles are not equal if they must have different vertices.
	 */
	bool operator!=(const TileLayer &other) const
	{
		return !(*this == other);
	}

	// Sets everything else except the texture in the material
	void applyMaterialOptions(video::SMaterial &material) const
	{
		switch (material_type) {
		case TILE_MATERIAL_OPAQUE:
		case TILE_MATERIAL_LIQUID_OPAQUE:
		case TILE_MATERIAL_WAVING_LIQUID_OPAQUE:
			material.MaterialType = video::EMT_SOLID;
			break;
		case TILE_MATERIAL_BASIC:
		case TILE_MATERIAL_WAVING_LEAVES:
		case TILE_MATERIAL_WAVING_PLANTS:
		case TILE_MATERIAL_WAVING_LIQUID_BASIC:
			material.MaterialTypeParam = 0.5;
			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		case TILE_MATERIAL_ALPHA:
		case TILE_MATERIAL_LIQUID_TRANSPARENT:
		case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			break;
		default:
			break;
		}
		material.BackfaceCulling = (material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)) {
			material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		}
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL)) {
			material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		}
	}

	void applyMaterialOptionsWithShaders(video::SMaterial &material) const
	{
		material.BackfaceCulling = (material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)) {
			material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
			material.TextureLayers[1].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		}
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL)) {
			material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
			material.TextureLayers[1].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		}
	}

	bool isTransparent() const
	{
		switch (material_type) {
		case TILE_MATERIAL_ALPHA:
		case TILE_MATERIAL_LIQUID_TRANSPARENT:
		case TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT:
			return true;
		}
		return false;
	}

	// Ordered for size, please do not reorder

	video::ITexture *texture = nullptr;
	video::ITexture *normal_texture = nullptr;
	video::ITexture *flags_texture = nullptr;

	u32 shader_id = 0;

	u32 texture_id = 0;

	u16 animation_frame_length_ms = 0;
	u16 animation_frame_count = 1;

	u8 material_type = TILE_MATERIAL_BASIC;
	u8 material_flags =
		//0 // <- DEBUG, Use the one below
		MATERIAL_FLAG_BACKFACE_CULLING |
		MATERIAL_FLAG_TILEABLE_HORIZONTAL|
		MATERIAL_FLAG_TILEABLE_VERTICAL;

	//! If true, the tile has its own color.
	bool has_color = false;

	std::vector<FrameSpec> *frames = nullptr;

	/*!
	 * The color of the tile, or if the tile does not own
	 * a color then the color of the node owning this tile.
	 */
	video::SColor color = video::SColor(0, 0, 0, 0);

	u8 scale = 1;
};

enum class TileRotation: u8 {
	None,
	R90,
	R180,
	R270,
};

/*!
 * Defines a face of a node. May have up to two layers.
 */
struct TileSpec
{
	TileSpec() = default;

	//! If true, the tile rotation is ignored.
	bool world_aligned = false;
	//! Tile rotation.
	TileRotation rotation = TileRotation::None;
	//! This much light does the tile emit.
	u8 emissive_light = 0;
	//! The first is base texture, the second is overlay.
	TileLayer layers[MAX_TILE_LAYERS];
};

std::vector<std::string> getTextureDirs();
