/*
itemdef.cpp
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

#include "itemdef.h"
#include "gamedef.h"
#include "nodedef.h"
#include "tool.h"
#include "inventory.h"
#ifndef SERVER
#include "mapblock_mesh.h"
#include "mesh.h"
#include "wieldmesh.h"
#include "clientmap.h"
#include "mapblock.h"
#include "client/tile.h"
#endif
#include "log.h"
#include "settings.h"
#include "util/serialize.h"
#include "util/container.h"
#include "util/thread.h"
#include <map>
#include <set>

#ifdef __ANDROID__
#include <GLES/gl.h>
#endif

/*
	ItemDefinition
*/
ItemDefinition::ItemDefinition()
{
	resetInitial();
}

ItemDefinition::ItemDefinition(const ItemDefinition &def)
{
	resetInitial();
	*this = def;
}

ItemDefinition& ItemDefinition::operator=(const ItemDefinition &def)
{
	if(this == &def)
		return *this;

	reset();

	type = def.type;
	name = def.name;
	description = def.description;
	inventory_image = def.inventory_image;
	wield_image = def.wield_image;
	wield_scale = def.wield_scale;
	stack_max = def.stack_max;
	usable = def.usable;
	liquids_pointable = def.liquids_pointable;
	if(def.tool_capabilities)
	{
		tool_capabilities = new ToolCapabilities(
				*def.tool_capabilities);
	}
	groups = def.groups;
	node_placement_prediction = def.node_placement_prediction;
	sound_place = def.sound_place;
	range = def.range;
	return *this;
}

ItemDefinition::~ItemDefinition()
{
	reset();
}

void ItemDefinition::resetInitial()
{
	// Initialize pointers to NULL so reset() does not delete undefined pointers
	tool_capabilities = NULL;
	reset();
}

void ItemDefinition::reset()
{
	type = ITEM_NONE;
	name = "";
	description = "";
	inventory_image = "";
	wield_image = "";
	wield_scale = v3f(1.0, 1.0, 1.0);
	stack_max = 99;
	usable = false;
	liquids_pointable = false;
	if(tool_capabilities)
	{
		delete tool_capabilities;
		tool_capabilities = NULL;
	}
	groups.clear();
	sound_place = SimpleSoundSpec();
	range = -1;

	node_placement_prediction = "";
}

void ItemDefinition::serialize(std::ostream &os, u16 protocol_version) const
{
	if(protocol_version <= 17)
		writeU8(os, 1); // version
	else if(protocol_version <= 20)
		writeU8(os, 2); // version
	else
		writeU8(os, 3); // version
	writeU8(os, type);
	os<<serializeString(name);
	os<<serializeString(description);
	os<<serializeString(inventory_image);
	os<<serializeString(wield_image);
	writeV3F1000(os, wield_scale);
	writeS16(os, stack_max);
	writeU8(os, usable);
	writeU8(os, liquids_pointable);
	std::string tool_capabilities_s = "";
	if(tool_capabilities){
		std::ostringstream tmp_os(std::ios::binary);
		tool_capabilities->serialize(tmp_os, protocol_version);
		tool_capabilities_s = tmp_os.str();
	}
	os<<serializeString(tool_capabilities_s);
	writeU16(os, groups.size());
	for(std::map<std::string, int>::const_iterator
			i = groups.begin(); i != groups.end(); ++i){
		os<<serializeString(i->first);
		writeS16(os, i->second);
	}
	os<<serializeString(node_placement_prediction);
	if(protocol_version > 17){
		//serializeSimpleSoundSpec(sound_place, os);
		os<<serializeString(sound_place.name);
		writeF1000(os, sound_place.gain);
	}
	if(protocol_version > 20){
		writeF1000(os, range);
	}
}

void ItemDefinition::deSerialize(std::istream &is)
{
	// Reset everything
	reset();

	// Deserialize
	int version = readU8(is);
	if(version < 1 || version > 3)
		throw SerializationError("unsupported ItemDefinition version");
	type = (enum ItemType)readU8(is);
	name = deSerializeString(is);
	description = deSerializeString(is);
	inventory_image = deSerializeString(is);
	wield_image = deSerializeString(is);
	wield_scale = readV3F1000(is);
	stack_max = readS16(is);
	usable = readU8(is);
	liquids_pointable = readU8(is);
	std::string tool_capabilities_s = deSerializeString(is);
	if(!tool_capabilities_s.empty())
	{
		std::istringstream tmp_is(tool_capabilities_s, std::ios::binary);
		tool_capabilities = new ToolCapabilities;
		tool_capabilities->deSerialize(tmp_is);
	}
	groups.clear();
	u32 groups_size = readU16(is);
	for(u32 i=0; i<groups_size; i++){
		std::string name = deSerializeString(is);
		int value = readS16(is);
		groups[name] = value;
	}
	if(version == 1){
		// We cant be sure that node_placement_prediction is send in version 1
		try{
			node_placement_prediction = deSerializeString(is);
		}catch(SerializationError &e) {};
		// Set the old default sound
		sound_place.name = "default_place_node";
		sound_place.gain = 0.5;
	} else if(version >= 2) {
		node_placement_prediction = deSerializeString(is);
		//deserializeSimpleSoundSpec(sound_place, is);
		sound_place.name = deSerializeString(is);
		sound_place.gain = readF1000(is);
	}
	if(version == 3) {
		range = readF1000(is);
	}
	// If you add anything here, insert it primarily inside the try-catch
	// block to not need to increase the version.
	try{
	}catch(SerializationError &e) {};
}

void ItemDefinition::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	pk.pack_map(tool_capabilities ? 15 : 14);
	PACK(ITEMDEF_TYPE, (int)type);
	PACK(ITEMDEF_NAME, name);
	PACK(ITEMDEF_DESCRIPTION, description);
	PACK(ITEMDEF_INVENTORY_IMAGE, inventory_image);
	PACK(ITEMDEF_WIELD_IMAGE, wield_image);
	PACK(ITEMDEF_WIELD_SCALE, wield_scale);
	PACK(ITEMDEF_STACK_MAX, stack_max);
	PACK(ITEMDEF_USABLE, usable);
	PACK(ITEMDEF_LIQUIDS_POINTABLE, liquids_pointable);

	if(tool_capabilities)
		PACK(ITEMDEF_TOOL_CAPABILITIES, *tool_capabilities);

	PACK(ITEMDEF_GROUPS, groups);
	PACK(ITEMDEF_NODE_PLACEMENT_PREDICTION, node_placement_prediction);
	PACK(ITEMDEF_SOUND_PLACE_NAME, sound_place.name);
	PACK(ITEMDEF_SOUND_PLACE_GAIN, sound_place.gain);
	PACK(ITEMDEF_RANGE, range);
}

void ItemDefinition::msgpack_unpack(msgpack::object o)
{
	// Reset everything
	reset();

	MsgpackPacket packet = o.as<MsgpackPacket>();
	int type_tmp;
	packet[ITEMDEF_TYPE].convert(&type_tmp);
	type = (ItemType)type_tmp;
	packet[ITEMDEF_NAME].convert(&name);
	packet[ITEMDEF_DESCRIPTION].convert(&description);
	packet[ITEMDEF_INVENTORY_IMAGE].convert(&inventory_image);
	packet[ITEMDEF_WIELD_IMAGE].convert(&wield_image);
	packet[ITEMDEF_WIELD_SCALE].convert(&wield_scale);
	packet[ITEMDEF_STACK_MAX].convert(&stack_max);
	packet[ITEMDEF_USABLE].convert(&usable);
	packet[ITEMDEF_LIQUIDS_POINTABLE].convert(&liquids_pointable);

	if (packet.find(ITEMDEF_TOOL_CAPABILITIES) != packet.end()) {
		tool_capabilities = new ToolCapabilities;
		packet[ITEMDEF_TOOL_CAPABILITIES].convert(tool_capabilities);
	}

	packet[ITEMDEF_GROUPS].convert(&groups);
	packet[ITEMDEF_NODE_PLACEMENT_PREDICTION].convert(&node_placement_prediction);
	packet[ITEMDEF_SOUND_PLACE_NAME].convert(&sound_place.name);
	packet[ITEMDEF_SOUND_PLACE_GAIN].convert(&sound_place.gain);
	packet[ITEMDEF_RANGE].convert(&range);
}

/*
	CItemDefManager
*/

enum {
	ITEMDEFMANAGER_ITEMDEFS,
	ITEMDEFMANAGER_ALIASES
};

// SUGG: Support chains of aliases?

class CItemDefManager: public IWritableItemDefManager
{
#ifndef SERVER
	struct ClientCached
	{
		video::ITexture *inventory_texture;
		scene::IMesh *wield_mesh;

		ClientCached():
			inventory_texture(NULL),
			wield_mesh(NULL)
		{}
	};
#endif

public:
	CItemDefManager()
	{

#ifndef SERVER
		m_main_thread = get_current_thread_id();
#endif
		clear();
	}
	virtual ~CItemDefManager()
	{
#ifndef SERVER
		const std::vector<ClientCached*> &values = m_clientcached.getValues();
		for(std::vector<ClientCached*>::const_iterator
				i = values.begin(); i != values.end(); ++i)
		{
			ClientCached *cc = *i;
			if (cc->wield_mesh)
				cc->wield_mesh->drop();
			delete cc;
		}

#endif
		for (std::map<std::string, ItemDefinition*>::iterator iter =
				m_item_definitions.begin(); iter != m_item_definitions.end();
				++iter) {
			delete iter->second;
		}
		m_item_definitions.clear();
	}
	virtual const ItemDefinition& get(const std::string &name_) const
	{
		// Convert name according to possible alias
		std::string name = getAlias(name_);
		// Get the definition
		std::map<std::string, ItemDefinition*>::const_iterator i;
		i = m_item_definitions.find(name);
		if(i == m_item_definitions.end())
			i = m_item_definitions.find("unknown");
		assert(i != m_item_definitions.end());
		return *(i->second);
	}
	virtual std::string getAlias(const std::string &name) const
	{
		StringMap::const_iterator it = m_aliases.find(name);
		if (it != m_aliases.end())
			return it->second;
		return name;
	}
	virtual std::set<std::string> getAll() const
	{
		std::set<std::string> result;
		for(std::map<std::string, ItemDefinition *>::const_iterator
				it = m_item_definitions.begin();
				it != m_item_definitions.end(); ++it) {
			result.insert(it->first);
		}
		for (StringMap::const_iterator
				it = m_aliases.begin();
				it != m_aliases.end(); ++it) {
			result.insert(it->first);
		}
		return result;
	}
	virtual bool isKnown(const std::string &name_) const
	{
		// Convert name according to possible alias
		std::string name = getAlias(name_);
		// Get the definition
		std::map<std::string, ItemDefinition*>::const_iterator i;
		return m_item_definitions.find(name) != m_item_definitions.end();
	}
#ifndef SERVER
public:
	ClientCached* createClientCachedDirect(const std::string &name,
			IGameDef *gamedef) const
	{
		infostream<<"Lazily creating item texture and mesh for \""
				<<name<<"\""<<std::endl;

		// This is not thread-safe
		sanity_check(get_current_thread_id() == m_main_thread);

		// Skip if already in cache
		ClientCached *cc = NULL;
		m_clientcached.get(name, &cc);
		if(cc)
			return cc;

		ITextureSource *tsrc = gamedef->getTextureSource();
		INodeDefManager *nodedef = gamedef->getNodeDefManager();
		const ItemDefinition &def = get(name);

		// Create new ClientCached
		cc = new ClientCached();

		// Create an inventory texture
		cc->inventory_texture = NULL;
		if(def.inventory_image != "")
			cc->inventory_texture = tsrc->getTexture(def.inventory_image);

		// Additional processing for nodes:
		// - Create a wield mesh if WieldMeshSceneNode can't render
		//   the node on its own.
		// - If inventory_texture isn't set yet, create one using
		//   render-to-texture.
		if (def.type == ITEM_NODE) {
			// Get node properties
			content_t id = nodedef->getId(name);
			const ContentFeatures &f = nodedef->get(id);

			bool need_rtt_mesh = cc->inventory_texture == NULL;

			// Keep this in sync with WieldMeshSceneNode::setItem()
			bool need_wield_mesh =
				!(f.mesh_ptr[0] ||
				  f.drawtype == NDT_NORMAL ||
				  f.drawtype == NDT_ALLFACES ||
				  f.drawtype == NDT_AIRLIKE);

			scene::IMesh *node_mesh = NULL;

			if (need_rtt_mesh || need_wield_mesh) {
				u8 param1 = 0;
				if (f.param_type == CPT_LIGHT)
					param1 = 0xee;

				/*
					Make a mesh from the node
				*/
				Map map(gamedef);
				MapDrawControl map_draw_control;
				MeshMakeData mesh_make_data(gamedef, false, map, map_draw_control);
				v3POS bp = v3POS(32000, 32000, 32000-id);
				auto block = map.createBlankBlockNoInsert(bp);
				auto air_node = MapNode(CONTENT_AIR, LIGHT_MAX);
				for(s16 z0=0; z0<=2; ++z0)
				for(s16 y0=0; y0<=2; ++y0)
				for(s16 x0=0; x0<=2; ++x0) {
					v3s16 p(x0,y0,z0);
					block->setNode(p, air_node);
				}
				u8 param2 = 0;
				if (f.param_type_2 == CPT2_WALLMOUNTED)
					param2 = 1;
				MapNode mesh_make_node(id, param1, param2);
				mesh_make_data.fillSingleNode(&mesh_make_node, bp);
				block->setNode(v3s16(1,1,1), mesh_make_node);
				map.insertBlock(block);
				MapBlockMesh mapblock_mesh(&mesh_make_data, bp*MAP_BLOCKSIZE);

/* MT
				MeshMakeData mesh_make_data(gamedef, false);
				u8 param2 = 0;
				if (f.param_type_2 == CPT2_WALLMOUNTED)
					param2 = 1;
				MapNode mesh_make_node(id, param1, param2);
				mesh_make_data.fillSingleNode(&mesh_make_node);
				MapBlockMesh mapblock_mesh(&mesh_make_data, v3s16(0, 0, 0));
*/

				node_mesh = mapblock_mesh.getMesh();
				node_mesh->grab();
				video::SColor c(255, 255, 255, 255);
				setMeshColor(node_mesh, c);

				// scale and translate the mesh so it's a
				// unit cube centered on the origin
				scaleMesh(node_mesh, v3f(1.0/BS, 1.0/BS, 1.0/BS));
				translateMesh(node_mesh, v3f(-1.0, -1.0, -1.0));
			}

			/*
				Draw node mesh into a render target texture
			*/
			if (need_rtt_mesh) {
				TextureFromMeshParams params;
				params.mesh = node_mesh;
				params.dim.set(64, 64);
				params.rtt_texture_name = "INVENTORY_"
					+ def.name + "_RTT";
				params.delete_texture_on_shutdown = true;
				params.camera_position.set(0, 1.0, -1.5);
				params.camera_position.rotateXZBy(45);
				params.camera_lookat.set(0, 0, 0);
				// Set orthogonal projection
				params.camera_projection_matrix.buildProjectionMatrixOrthoLH(
						1.65, 1.65, 0, 100);
				params.ambient_light.set(1.0, 0.2, 0.2, 0.2);
				params.light_position.set(10, 100, -50);
				params.light_color.set(1.0, 0.5, 0.5, 0.5);
				params.light_radius = 1000;

#ifdef __ANDROID__
				params.camera_position.set(0, -1.0, -1.5);
				params.camera_position.rotateXZBy(45);
				params.light_position.set(10, -100, -50);
#endif
				cc->inventory_texture =
					tsrc->generateTextureFromMesh(params);

				// render-to-target didn't work
				if (cc->inventory_texture == NULL) {
					cc->inventory_texture =
						tsrc->getTexture(f.tiledef[0].name);
				}
			}

			/*
				Use the node mesh as the wield mesh
			*/
			if (need_wield_mesh) {
				cc->wield_mesh = node_mesh;
				cc->wield_mesh->grab();

				// no way reference count can be smaller than 2 in this place!
				assert(cc->wield_mesh->getReferenceCount() >= 2);
			}

			if (node_mesh)
				node_mesh->drop();
		}

		// Put in cache
		m_clientcached.set(name, cc);

		return cc;
	}
	ClientCached* getClientCached(const std::string &name,
			IGameDef *gamedef) const
	{
		ClientCached *cc = NULL;
		m_clientcached.get(name, &cc);
		if(cc)
			return cc;

		if(get_current_thread_id() == m_main_thread)
		{
			return createClientCachedDirect(name, gamedef);
		}
		else
		{
			// We're gonna ask the result to be put into here
			static ResultQueue<std::string, ClientCached*, u8, u8> result_queue;

			// Throw a request in
			m_get_clientcached_queue.add(name, 0, 0, &result_queue);
			try{
				while(true) {
					// Wait result for a second
					GetResult<std::string, ClientCached*, u8, u8>
						result = result_queue.pop_front(1000);

					if (result.key == name) {
						return result.item;
					}
				}
			}
			catch(ItemNotFoundException &e)
			{
				errorstream<<"Waiting for clientcached " << name << " timed out."<<std::endl;
				return &m_dummy_clientcached;
			}
		}
	}
	// Get item inventory texture
	virtual video::ITexture* getInventoryTexture(const std::string &name,
			IGameDef *gamedef) const
	{
		ClientCached *cc = getClientCached(name, gamedef);
		if(!cc)
			return NULL;
		return cc->inventory_texture;
	}
	// Get item wield mesh
	virtual scene::IMesh* getWieldMesh(const std::string &name,
			IGameDef *gamedef) const
	{
		ClientCached *cc = getClientCached(name, gamedef);
		if(!cc)
			return NULL;
		return cc->wield_mesh;
	}
#endif
	void clear()
	{
		for(std::map<std::string, ItemDefinition*>::const_iterator
				i = m_item_definitions.begin();
				i != m_item_definitions.end(); ++i)
		{
			delete i->second;
		}
		m_item_definitions.clear();
		m_aliases.clear();

		// Add the four builtin items:
		//   "" is the hand
		//   "unknown" is returned whenever an undefined item
		//     is accessed (is also the unknown node)
		//   "air" is the air node
		//   "ignore" is the ignore node

		ItemDefinition* hand_def = new ItemDefinition;
		hand_def->name = "";
		hand_def->wield_image = "wieldhand.png";
		hand_def->tool_capabilities = new ToolCapabilities;
		m_item_definitions.insert(std::make_pair("", hand_def));

		ItemDefinition* unknown_def = new ItemDefinition;
		unknown_def->type = ITEM_NODE;
		unknown_def->name = "unknown";
		m_item_definitions.insert(std::make_pair("unknown", unknown_def));

		ItemDefinition* air_def = new ItemDefinition;
		air_def->type = ITEM_NODE;
		air_def->name = "air";
		m_item_definitions.insert(std::make_pair("air", air_def));

		ItemDefinition* ignore_def = new ItemDefinition;
		ignore_def->type = ITEM_NODE;
		ignore_def->name = "ignore";
		m_item_definitions.insert(std::make_pair("ignore", ignore_def));
	}
	virtual void registerItem(const ItemDefinition &def)
	{
		verbosestream<<"ItemDefManager: registering \""<<def.name<<"\""<<std::endl;
		// Ensure that the "" item (the hand) always has ToolCapabilities
		if(def.name == "")
			FATAL_ERROR_IF(!def.tool_capabilities, "Hand does not have ToolCapabilities");

		if(m_item_definitions.count(def.name) == 0)
			m_item_definitions[def.name] = new ItemDefinition(def);
		else
			*(m_item_definitions[def.name]) = def;

		// Remove conflicting alias if it exists
		bool alias_removed = (m_aliases.erase(def.name) != 0);
		if(alias_removed)
			infostream<<"ItemDefManager: erased alias "<<def.name
					<<" because item was defined"<<std::endl;
	}
	virtual void registerAlias(const std::string &name,
			const std::string &convert_to)
	{
		if(m_item_definitions.find(name) == m_item_definitions.end())
		{
			verbosestream<<"ItemDefManager: setting alias "<<name
				<<" -> "<<convert_to<<std::endl;
			m_aliases[name] = convert_to;
		}
	}
	void serialize(std::ostream &os, u16 protocol_version)
	{
		writeU8(os, 0); // version
		u16 count = m_item_definitions.size();
		writeU16(os, count);

		for (std::map<std::string, ItemDefinition *>::const_iterator
				it = m_item_definitions.begin();
				it != m_item_definitions.end(); ++it) {
			ItemDefinition *def = it->second;
			// Serialize ItemDefinition and write wrapped in a string
			std::ostringstream tmp_os(std::ios::binary);
			def->serialize(tmp_os, protocol_version);
			os << serializeString(tmp_os.str());
		}

		writeU16(os, m_aliases.size());

		for (StringMap::const_iterator
				it = m_aliases.begin();
				it != m_aliases.end(); ++it) {
			os << serializeString(it->first);
			os << serializeString(it->second);
		}
	}
	void deSerialize(std::istream &is)
	{
		// Clear everything
		clear();
		// Deserialize
		int version = readU8(is);
		if(version != 0)
			throw SerializationError("unsupported ItemDefManager version");
		u16 count = readU16(is);
		for(u16 i=0; i<count; i++)
		{
			// Deserialize a string and grab an ItemDefinition from it
			std::istringstream tmp_is(deSerializeString(is), std::ios::binary);
			ItemDefinition def;
			def.deSerialize(tmp_is);
			// Register
			registerItem(def);
		}
		u16 num_aliases = readU16(is);
		for(u16 i=0; i<num_aliases; i++)
		{
			std::string name = deSerializeString(is);
			std::string convert_to = deSerializeString(is);
			registerAlias(name, convert_to);
		}
	}
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const {
		pk.pack_map(2);
		pk.pack((int)ITEMDEFMANAGER_ITEMDEFS);
		pk.pack_map(m_item_definitions.size());
		for (std::map<std::string, ItemDefinition*>::const_iterator i = m_item_definitions.begin();
				i != m_item_definitions.end(); ++i) {
			pk.pack(i->first);
			pk.pack(*(i->second));
		}
		PACK(ITEMDEFMANAGER_ALIASES, m_aliases);
	}
	void msgpack_unpack(msgpack::object o) {
		clear();
		MsgpackPacket packet = o.as<MsgpackPacket>();

		std::map<std::string, ItemDefinition> itemdefs_tmp;
		packet[ITEMDEFMANAGER_ITEMDEFS].convert(&itemdefs_tmp);
		for (std::map<std::string, ItemDefinition>::iterator i = itemdefs_tmp.begin();
				i != itemdefs_tmp.end(); ++i) {
			registerItem(i->second);
		}
		packet[ITEMDEFMANAGER_ALIASES].convert(&m_aliases);
	}
	void processQueue(IGameDef *gamedef)
	{
#ifndef SERVER
		//NOTE this is only thread safe for ONE consumer thread!
		while(!m_get_clientcached_queue.empty())
		{
			GetRequest<std::string, ClientCached*, u8, u8>
					request = m_get_clientcached_queue.pop();

			m_get_clientcached_queue.pushResult(request,
					createClientCachedDirect(request.key, gamedef));
		}
#endif
	}
private:
	// Key is name
	std::map<std::string, ItemDefinition*> m_item_definitions;
	// Aliases
	StringMap m_aliases;
#ifndef SERVER
	// The id of the thread that is allowed to use irrlicht directly
	threadid_t m_main_thread;
	// A reference to this can be returned when nothing is found, to avoid NULLs
	mutable ClientCached m_dummy_clientcached;
	// Cached textures and meshes
	mutable MutexedMap<std::string, ClientCached*> m_clientcached;
	// Queued clientcached fetches (to be processed by the main thread)
	mutable RequestQueue<std::string, ClientCached*, u8, u8> m_get_clientcached_queue;
#endif
};

IWritableItemDefManager* createItemDefManager()
{
	return new CItemDefManager();
}
