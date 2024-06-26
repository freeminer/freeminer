/*
itemdef.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Kahrl <kahrl@gmx.net>
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

#include "irrlichttypes_extrabloated.h"
#include <string>
#include <iostream>
#include <optional>
#include <set>
#include "itemgroup.h"
#include "sound.h"

// fm:
#include "msgpack_fix.h"

#include "texture_override.h" // TextureOverride
class IGameDef;
class Client;
struct ToolCapabilities;
#ifndef SERVER
#include "client/tile.h"
struct ItemMesh;
struct ItemStack;
#endif

/*
	Base item definition
*/

enum ItemType
{
	ITEM_NONE,
	ITEM_NODE,
	ITEM_CRAFT,
	ITEM_TOOL,
};

enum {
	ITEMDEF_TYPE,
	ITEMDEF_NAME,
	ITEMDEF_DESCRIPTION,
	ITEMDEF_INVENTORY_IMAGE,
	ITEMDEF_WIELD_IMAGE,
	ITEMDEF_WIELD_SCALE,
	ITEMDEF_STACK_MAX,
	ITEMDEF_USABLE,
	ITEMDEF_LIQUIDS_POINTABLE,
	ITEMDEF_TOOL_CAPABILITIES,
	ITEMDEF_GROUPS,
	ITEMDEF_NODE_PLACEMENT_PREDICTION,
	ITEMDEF_SOUND_PLACE_NAME,
	ITEMDEF_SOUND_PLACE_GAIN,
	ITEMDEF_RANGE
};

struct ItemDefinition
{
	/*
		Basic item properties
	*/
	ItemType type;
	std::string name; // "" = hand
	std::string description; // Shown in tooltip.
	std::string short_description;

	/*
		Visual properties
	*/
	std::string inventory_image; // Optional for nodes, mandatory for tools/craftitems
	std::string inventory_overlay; // Overlay of inventory_image.
	std::string wield_image; // If empty, inventory_image or mesh (only nodes) is used
	std::string wield_overlay; // Overlay of wield_image.
	std::string palette_image; // If specified, the item will be colorized based on this
	video::SColor color; // The fallback color of the node.
	v3f wield_scale;

	/*
		Item stack and interaction properties
	*/
	u16 stack_max;
	bool usable;
	bool liquids_pointable;
	// May be NULL. If non-NULL, deleted by destructor
	ToolCapabilities *tool_capabilities;
	ItemGroupList groups;
	SoundSpec sound_place;
	SoundSpec sound_place_failed;
	SoundSpec sound_use, sound_use_air;
	f32 range;

	// Client shall immediately place this node when player places the item.
	// Server will update the precise end result a moment later.
	// "" = no prediction
	std::string node_placement_prediction;
	std::optional<u8> place_param2;

	/*
		Some helpful methods
	*/
	ItemDefinition();
	ItemDefinition(const ItemDefinition &def);
	ItemDefinition& operator=(const ItemDefinition &def);
	~ItemDefinition();
	void reset();
	void serialize(std::ostream &os, u16 protocol_version) const;
	void deSerialize(std::istream &is, u16 protocol_version);

//fm:
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const;
	void msgpack_unpack(msgpack::object o);

private:
	void resetInitial();
};

class IItemDefManager
{
public:
	IItemDefManager() = default;

	virtual ~IItemDefManager() = default;

	// Get item definition
	virtual const ItemDefinition& get(const std::string &name) const=0;
	// Get alias definition
	virtual const std::string &getAlias(const std::string &name) const=0;
	// Get set of all defined item names and aliases
	virtual void getAll(std::set<std::string> &result) const=0;
	// Check if item is known
	virtual bool isKnown(const std::string &name) const=0;
#ifndef SERVER
	// Get item inventory texture
	virtual video::ITexture* getInventoryTexture(const ItemStack &item, Client *client) const=0;

	/**
	 * Get wield mesh
	 *
	 * Returns nullptr if there is an inventory image
	 */
	virtual ItemMesh* getWieldMesh(const ItemStack &item, Client *client) const = 0;
	// Get item palette
	virtual Palette* getPalette(const ItemStack &item, Client *client) const = 0;
	// Returns the base color of an item stack: the color of all
	// tiles that do not define their own color.
	virtual video::SColor getItemstackColor(const ItemStack &stack,
		Client *client) const = 0;
#endif

	virtual void serialize(std::ostream &os, u16 protocol_version)=0;

	virtual void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const=0;
	virtual void msgpack_unpack(msgpack::object o)=0;
};

class IWritableItemDefManager : public IItemDefManager
{
public:
	IWritableItemDefManager() = default;

	virtual ~IWritableItemDefManager() = default;

	// Replace the textures of registered nodes with the ones specified in
	// the texture pack's override.txt files
	virtual void applyTextureOverrides(const std::vector<TextureOverride> &overrides)=0;

	// Remove all registered item and node definitions and aliases
	// Then re-add the builtin item definitions
	virtual void clear()=0;
	// Register item definition
	virtual void registerItem(const ItemDefinition &def)=0;
	virtual void unregisterItem(const std::string &name)=0;
	// Set an alias so that items named <name> will load as <convert_to>.
	// Alias is not set if <name> has already been defined.
	// Alias will be removed if <name> is defined at a later point of time.
	virtual void registerAlias(const std::string &name,
			const std::string &convert_to)=0;

	virtual void deSerialize(std::istream &is, u16 protocol_version)=0;
};

IWritableItemDefManager* createItemDefManager();
