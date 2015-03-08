/*
nodedef.cpp
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "nodedef.h"

#include "main.h" // For g_settings
#include "itemdef.h"
#ifndef SERVER
#include "client/tile.h"
#include "mesh.h"
#include <IMeshManipulator.h>
#endif
#include "log.h"
#include "settings.h"
#include "nameidmapping.h"
#include "util/numeric.h"
#include "util/serialize.h"
//#include "profiler.h" // For TimeTaker
#include "network/connection.h"
#include "shader.h"
#include "exceptions.h"
#include "debug.h"
#include "gamedef.h"

/*
	NodeBox
*/

void NodeBox::reset()
{
	type = NODEBOX_REGULAR;
	// default is empty
	fixed.clear();
	// default is sign/ladder-like
	wall_top = aabb3f(-BS/2, BS/2-BS/16., -BS/2, BS/2, BS/2, BS/2);
	wall_bottom = aabb3f(-BS/2, -BS/2, -BS/2, BS/2, -BS/2+BS/16., BS/2);
	wall_side = aabb3f(-BS/2, -BS/2, -BS/2, -BS/2+BS/16., BS/2, BS/2);
}

void NodeBox::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	int map_size = 1;
	if (type == NODEBOX_FIXED || type == NODEBOX_LEVELED)
		map_size = 2;
	else if (type == NODEBOX_WALLMOUNTED)
		map_size = 4;

	pk.pack_map(map_size);
	PACK(NODEBOX_S_TYPE, (int)type);

	if(type == NODEBOX_FIXED || type == NODEBOX_LEVELED)
		PACK(NODEBOX_S_FIXED, fixed)
	else if(type == NODEBOX_WALLMOUNTED) {
		PACK(NODEBOX_S_WALL_TOP, wall_top);
		PACK(NODEBOX_S_WALL_BOTTOM, wall_bottom);
		PACK(NODEBOX_S_WALL_SIDE, wall_side);
	}
}

void NodeBox::msgpack_unpack(msgpack::object o)
{
	reset();

	MsgpackPacket packet = o.as<MsgpackPacket>();

	int type_tmp = packet[NODEBOX_S_TYPE].as<int>();
	type = (NodeBoxType)type_tmp;

	if(type == NODEBOX_FIXED || type == NODEBOX_LEVELED)
		packet[NODEBOX_S_FIXED].convert(&fixed);
	else if(type == NODEBOX_WALLMOUNTED) {
		packet[NODEBOX_S_WALL_TOP].convert(&wall_top);
		packet[NODEBOX_S_WALL_BOTTOM].convert(&wall_bottom);
		packet[NODEBOX_S_WALL_SIDE].convert(&wall_side);
	}
}

/*
	TileDef
*/

void TileDef::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	pk.pack_map(6);
	PACK(TILEDEF_NAME, name);
	PACK(TILEDEF_ANIMATION_TYPE, (int)animation.type);
	PACK(TILEDEF_ANIMATION_ASPECT_W, animation.aspect_w);
	PACK(TILEDEF_ANIMATION_ASPECT_H, animation.aspect_h);
	PACK(TILEDEF_ANIMATION_LENGTH, animation.length);
	PACK(TILEDEF_BACKFACE_CULLING, backface_culling);
}

void TileDef::msgpack_unpack(msgpack::object o)
{
	MsgpackPacket packet = o.as<MsgpackPacket>();
	packet[TILEDEF_NAME].convert(&name);

	int type_tmp;
	packet[TILEDEF_ANIMATION_TYPE].convert(&type_tmp);
	animation.type = (TileAnimationType)type_tmp;

	packet[TILEDEF_ANIMATION_ASPECT_W].convert(&animation.aspect_w);
	packet[TILEDEF_ANIMATION_ASPECT_H].convert(&animation.aspect_h);
	packet[TILEDEF_ANIMATION_LENGTH].convert(&animation.length);
	packet[TILEDEF_BACKFACE_CULLING].convert(&backface_culling);
}

/*
	SimpleSoundSpec serialization
*/

static void serializeSimpleSoundSpec(const SimpleSoundSpec &ss,
		std::ostream &os)
{
	os<<serializeString(ss.name);
	writeF1000(os, ss.gain);
}
static void deSerializeSimpleSoundSpec(SimpleSoundSpec &ss, std::istream &is)
{
	ss.name = deSerializeString(is);
	ss.gain = readF1000(is);
}

/*
	ContentFeatures
*/

ContentFeatures::ContentFeatures()
{
	reset();
}

ContentFeatures::~ContentFeatures()
{
}

void ContentFeatures::reset()
{
	/*
		Cached stuff
	*/
//#ifndef SERVER
	solidness = 2;
	visual_solidness = 0;
	backface_culling = true;
//#endif
	has_on_construct = false;
	has_on_destruct = false;
	has_after_destruct = false;
	has_on_activate = false;
	has_on_deactivate = false;
	/*
		Actual data

		NOTE: Most of this is always overridden by the default values given
		      in builtin.lua
	*/
	name = "";
	groups.clear();
	// Unknown nodes can be dug
	groups["dig_immediate"] = 2;
	drawtype = NDT_NORMAL;
	mesh = "";
#ifndef SERVER
	for(u32 i = 0; i < 24; i++)
		mesh_ptr[i] = NULL;
#endif
	visual_scale = 1.0;
	for(u32 i = 0; i < 6; i++)
		tiledef[i] = TileDef();
	for(u16 j = 0; j < CF_SPECIAL_COUNT; j++)
		tiledef_special[j] = TileDef();
	alpha = 255;
	post_effect_color = video::SColor(0, 0, 0, 0);
	param_type = CPT_NONE;
	param_type_2 = CPT2_NONE;
	is_ground_content = false;
	light_propagates = false;
	sunlight_propagates = false;
	walkable = true;
	pointable = true;
	diggable = true;
	climbable = false;
	buildable_to = false;
	rightclickable = true;
	leveled = 0;
	liquid_type = LIQUID_NONE;
	liquid_alternative_flowing = "";
	liquid_alternative_source = "";
	liquid_viscosity = 0;
	liquid_renewable = true;
	liquid_range = LIQUID_LEVEL_MAX+1;
	drowning = 0;
	light_source = 0;
	damage_per_second = 0;
	node_box = NodeBox();
	selection_box = NodeBox();
	collision_box = NodeBox();
	waving = 0;
	legacy_facedir_simple = false;
	legacy_wallmounted = false;
	sound_footstep = SimpleSoundSpec();
	sound_dig = SimpleSoundSpec("__group");
	sound_dug = SimpleSoundSpec();

	freeze = "";
	melt = "";
	is_circuit_element = false;
	is_wire = false;
	is_wire_connector = false;
	for(int i = 0; i < 6; ++i)
	{
		wire_connections[i] = 0;
	}
	for(int i = 0; i < 64; ++i)
	{
		circuit_element_func[i] = 0;
	}
	circuit_element_delay = 0;
}

void ContentFeatures::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	pk.pack_map(38);
	PACK(CONTENTFEATURES_NAME, name);
	PACK(CONTENTFEATURES_GROUPS, groups);
	PACK(CONTENTFEATURES_DRAWTYPE, (int)drawtype);
	PACK(CONTENTFEATURES_VISUAL_SCALE, visual_scale);

	pk.pack((int)CONTENTFEATURES_TILEDEF);
	pk.pack_array(6);
	for (size_t i = 0; i < 6; ++i)
		pk.pack(tiledef[i]);

	pk.pack((int)CONTENTFEATURES_TILEDEF_SPECIAL);
	pk.pack_array(CF_SPECIAL_COUNT);
	for (size_t i = 0; i < CF_SPECIAL_COUNT; ++i)
		pk.pack(tiledef_special[i]);

	PACK(CONTENTFEATURES_ALPHA, alpha);
	PACK(CONTENTFEATURES_POST_EFFECT_COLOR, post_effect_color);
	PACK(CONTENTFEATURES_PARAM_TYPE, (int)param_type);
	PACK(CONTENTFEATURES_PARAM_TYPE_2, (int)param_type_2);
	PACK(CONTENTFEATURES_IS_GROUND_CONTENT, is_ground_content);
	PACK(CONTENTFEATURES_LIGHT_PROPAGATES, light_propagates);
	PACK(CONTENTFEATURES_SUNLIGHT_PROPAGATES, sunlight_propagates);
	PACK(CONTENTFEATURES_WALKABLE, walkable);
	PACK(CONTENTFEATURES_POINTABLE, pointable);
	PACK(CONTENTFEATURES_DIGGABLE, diggable);
	PACK(CONTENTFEATURES_CLIMBABLE, climbable);
	PACK(CONTENTFEATURES_BUILDABLE_TO, buildable_to);
	PACK(CONTENTFEATURES_LIQUID_TYPE, (int)liquid_type);
	PACK(CONTENTFEATURES_LIQUID_ALTERNATIVE_FLOWING, liquid_alternative_flowing);
	PACK(CONTENTFEATURES_LIQUID_ALTERNATIVE_SOURCE, liquid_alternative_source);
	PACK(CONTENTFEATURES_LIQUID_VISCOSITY, liquid_viscosity);
	PACK(CONTENTFEATURES_LIQUID_RENEWABLE, liquid_renewable);
	PACK(CONTENTFEATURES_LIGHT_SOURCE, light_source);
	PACK(CONTENTFEATURES_DAMAGE_PER_SECOND, damage_per_second);
	PACK(CONTENTFEATURES_NODE_BOX, node_box);
	PACK(CONTENTFEATURES_SELECTION_BOX, selection_box);
	PACK(CONTENTFEATURES_LEGACY_FACEDIR_SIMPLE, legacy_facedir_simple);
	PACK(CONTENTFEATURES_LEGACY_WALLMOUNTED, legacy_wallmounted);
	PACK(CONTENTFEATURES_SOUND_FOOTSTEP, sound_footstep);
	PACK(CONTENTFEATURES_SOUND_DIG, sound_dig);
	PACK(CONTENTFEATURES_SOUND_DUG, sound_dug);
	PACK(CONTENTFEATURES_RIGHTCLICKABLE, rightclickable);
	PACK(CONTENTFEATURES_DROWNING, drowning);
	PACK(CONTENTFEATURES_LEVELED, leveled);
	PACK(CONTENTFEATURES_WAVING, waving);
	PACK(CONTENTFEATURES_MESH, mesh);
	PACK(CONTENTFEATURES_COLLISION_BOX, collision_box);
}

void ContentFeatures::msgpack_unpack(msgpack::object o)
{
	MsgpackPacket packet = o.as<MsgpackPacket>();
	packet[CONTENTFEATURES_NAME].convert(&name);
	groups.clear();
	packet[CONTENTFEATURES_GROUPS].convert(&groups);

	int drawtype_tmp;
	packet[CONTENTFEATURES_DRAWTYPE].convert(&drawtype_tmp);
	drawtype = (NodeDrawType)drawtype_tmp;

	packet[CONTENTFEATURES_VISUAL_SCALE].convert(&visual_scale);

	std::vector<TileDef> tiledef_received;
	packet[CONTENTFEATURES_TILEDEF].convert(&tiledef_received);
	if (tiledef_received.size() != 6)
		throw SerializationError("unsupported tile count");
	for(size_t i = 0; i < 6; ++i)
		tiledef[i] = tiledef_received[i];

	std::vector<TileDef> tiledef_special_received;
	packet[CONTENTFEATURES_TILEDEF_SPECIAL].convert(&tiledef_special_received);
	if(tiledef_special_received.size() != CF_SPECIAL_COUNT)
		throw SerializationError("unsupported CF_SPECIAL_COUNT");
	for (size_t i = 0; i < CF_SPECIAL_COUNT; ++i)
		tiledef_special[i] = tiledef_special_received[i];

	packet[CONTENTFEATURES_ALPHA].convert(&alpha);
	packet[CONTENTFEATURES_POST_EFFECT_COLOR].convert(&post_effect_color);

	int param_type_tmp;
	packet[CONTENTFEATURES_PARAM_TYPE].convert(&param_type_tmp);
	param_type = (ContentParamType)param_type_tmp;
	packet[CONTENTFEATURES_PARAM_TYPE_2].convert(&param_type_tmp);
	param_type_2 = (ContentParamType2)param_type_tmp;

	packet[CONTENTFEATURES_IS_GROUND_CONTENT].convert(&is_ground_content);
	packet[CONTENTFEATURES_LIGHT_PROPAGATES].convert(&light_propagates);
	packet[CONTENTFEATURES_SUNLIGHT_PROPAGATES].convert(&sunlight_propagates);
	packet[CONTENTFEATURES_WALKABLE].convert(&walkable);
	packet[CONTENTFEATURES_POINTABLE].convert(&pointable);
	packet[CONTENTFEATURES_DIGGABLE].convert(&diggable);
	packet[CONTENTFEATURES_CLIMBABLE].convert(&climbable);
	packet[CONTENTFEATURES_BUILDABLE_TO].convert(&buildable_to);

	int liquid_type_tmp;
	packet[CONTENTFEATURES_LIQUID_TYPE].convert(&liquid_type_tmp);
	liquid_type = (LiquidType)liquid_type_tmp;

	packet[CONTENTFEATURES_LIQUID_ALTERNATIVE_FLOWING].convert(&liquid_alternative_flowing);
	packet[CONTENTFEATURES_LIQUID_ALTERNATIVE_SOURCE].convert(&liquid_alternative_source);
	packet[CONTENTFEATURES_LIQUID_VISCOSITY].convert(&liquid_viscosity);
	packet[CONTENTFEATURES_LIGHT_SOURCE].convert(&light_source);
	packet[CONTENTFEATURES_DAMAGE_PER_SECOND].convert(&damage_per_second);
	packet[CONTENTFEATURES_NODE_BOX].convert(&node_box);
	packet[CONTENTFEATURES_SELECTION_BOX].convert(&selection_box);
	packet[CONTENTFEATURES_LEGACY_FACEDIR_SIMPLE].convert(&legacy_facedir_simple);
	packet[CONTENTFEATURES_LEGACY_WALLMOUNTED].convert(&legacy_wallmounted);
	packet[CONTENTFEATURES_SOUND_FOOTSTEP].convert(&sound_footstep);
	packet[CONTENTFEATURES_SOUND_DIG].convert(&sound_dig);
	packet[CONTENTFEATURES_SOUND_DUG].convert(&sound_dug);
	packet[CONTENTFEATURES_RIGHTCLICKABLE].convert(&rightclickable);
	packet[CONTENTFEATURES_DROWNING].convert(&drowning);
	packet[CONTENTFEATURES_LEVELED].convert(&leveled);
	packet[CONTENTFEATURES_WAVING].convert(&waving);
	packet[CONTENTFEATURES_MESH].convert(&mesh);
	packet[CONTENTFEATURES_COLLISION_BOX].convert(&collision_box);
}

/*
	CNodeDefManager
*/

class CNodeDefManager: public IWritableNodeDefManager {
public:
	CNodeDefManager();
	virtual ~CNodeDefManager();
	void clear();
	virtual IWritableNodeDefManager *clone();
	inline virtual const ContentFeatures& get(content_t c) const;
	inline virtual const ContentFeatures& get(const MapNode &n) const;
	virtual bool getId(const std::string &name, content_t &result) const;
	virtual content_t getId(const std::string &name) const;
	virtual void getIds(const std::string &name, std::unordered_set<content_t> &result) const;
	virtual void getIds(const std::string &name, FMBitset &result) const;
	virtual const ContentFeatures& get(const std::string &name) const;
	content_t allocateId();
	virtual content_t set(const std::string &name, const ContentFeatures &def);
	virtual content_t allocateDummy(const std::string &name);
	virtual void updateAliases(IItemDefManager *idef);
	virtual void updateTextures(IGameDef *gamedef);
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const;
	void msgpack_unpack(msgpack::object o);

	inline virtual bool getNodeRegistrationStatus() const;
	inline virtual void setNodeRegistrationStatus(bool completed);

	virtual void pendNodeResolve(NodeResolveInfo *nri);
	virtual void cancelNodeResolve(NodeResolver *resolver);
	virtual void runNodeResolverCallbacks();

	virtual bool getIdFromResolveInfo(NodeResolveInfo *nri,
		const std::string &node_alt, content_t c_fallback, content_t &result);
	virtual bool getIdsFromResolveInfo(NodeResolveInfo *nri,
		std::vector<content_t> &result);

private:
	void addNameIdMapping(content_t i, std::string name);
#ifndef SERVER
	void fillTileAttribs(ITextureSource *tsrc, TileSpec *tile, TileDef *tiledef,
		u32 shader_id, bool use_normal_texture, bool backface_culling,
		u8 alpha, u8 material_type);
#endif

	// Features indexed by id
	std::vector<ContentFeatures> m_content_features;

	// A mapping for fast converting back and forth between names and ids
	NameIdMapping m_name_id_mapping;

	// Like m_name_id_mapping, but only from names to ids, and includes
	// item aliases too. Updated by updateAliases()
	// Note: Not serialized.

	std::map<std::string, content_t> m_name_id_mapping_with_aliases;

	// A mapping from groups to a list of content_ts (and their levels)
	// that belong to it.  Necessary for a direct lookup in getIds().
	// Note: Not serialized.
	std::map<std::string, GroupItems> m_group_to_items;

	// Next possibly free id
	content_t m_next_id;

	// List of node strings and node resolver callbacks to perform
	std::vector<NodeResolveInfo *> m_pending_node_lookups;

	// True when all nodes have been registered
	bool m_node_registration_complete;
};


CNodeDefManager::CNodeDefManager()
{
	clear();
}


CNodeDefManager::~CNodeDefManager()
{
#ifndef SERVER
	for (u32 i = 0; i < m_content_features.size(); i++) {
		ContentFeatures *f = &m_content_features[i];
		for (u32 j = 0; j < 24; j++) {
			if (f->mesh_ptr[j])
				f->mesh_ptr[j]->drop();
		}
	}
#endif
}


void CNodeDefManager::clear()
{
	m_content_features.clear();
	m_name_id_mapping.clear();
	m_name_id_mapping_with_aliases.clear();
	m_group_to_items.clear();
	m_next_id = 0;

	m_node_registration_complete = false;
	for (std::vector<NodeResolveInfo *>::iterator
			it = m_pending_node_lookups.begin();
			it != m_pending_node_lookups.end();
			++it)
		delete *it;
	m_pending_node_lookups.clear();

	u32 initial_length = 0;
	initial_length = MYMAX(initial_length, CONTENT_UNKNOWN + 1);
	initial_length = MYMAX(initial_length, CONTENT_AIR + 1);
	initial_length = MYMAX(initial_length, CONTENT_IGNORE + 1);
	m_content_features.resize(initial_length);

	// Set CONTENT_UNKNOWN
	{
		ContentFeatures f;
		f.name = "unknown";
		// Insert directly into containers
		content_t c = CONTENT_UNKNOWN;
		m_content_features[c] = f;
		addNameIdMapping(c, f.name);
	}

	// Set CONTENT_AIR
	{
		ContentFeatures f;
		f.name                = "air";
		f.drawtype            = NDT_AIRLIKE;
		f.param_type          = CPT_LIGHT;
		f.light_propagates    = true;
		f.sunlight_propagates = true;
		f.walkable            = false;
		f.pointable           = false;
		f.diggable            = false;
		f.buildable_to        = true;
		f.is_ground_content   = true;
#ifndef SERVER
		f.color_avg = video::SColor(0,255,255,255);
#endif
		// Insert directly into containers
		content_t c = CONTENT_AIR;
		m_content_features[c] = f;
		addNameIdMapping(c, f.name);
	}

	// Set CONTENT_IGNORE
	{
		ContentFeatures f;
		f.name                = "ignore";
		f.drawtype            = NDT_AIRLIKE;
		f.param_type          = CPT_NONE;
		f.light_propagates    = false;
		f.sunlight_propagates = false;
		f.walkable            = false;
		f.pointable           = false;
		f.diggable            = false;
		f.buildable_to        = true; // A way to remove accidental CONTENT_IGNOREs
		f.is_ground_content   = true;
#ifndef SERVER
		f.color_avg = video::SColor(0,255,255,255);
#endif
		// Insert directly into containers
		content_t c = CONTENT_IGNORE;
		m_content_features[c] = f;
		addNameIdMapping(c, f.name);
	}
}


IWritableNodeDefManager *CNodeDefManager::clone()
{
	CNodeDefManager *mgr = new CNodeDefManager();
	*mgr = *this;
	return mgr;
}


inline const ContentFeatures& CNodeDefManager::get(content_t c) const
{
	return c < m_content_features.size()
			? m_content_features[c] : m_content_features[CONTENT_UNKNOWN];
}


inline const ContentFeatures& CNodeDefManager::get(const MapNode &n) const
{
	return get(n.getContent());
}


bool CNodeDefManager::getId(const std::string &name, content_t &result) const
{
	std::map<std::string, content_t>::const_iterator
		i = m_name_id_mapping_with_aliases.find(name);
	if(i == m_name_id_mapping_with_aliases.end())
		return false;
	result = i->second;
	return true;
}


content_t CNodeDefManager::getId(const std::string &name) const
{
	content_t id = CONTENT_IGNORE;
	getId(name, id);
	return id;
}


void CNodeDefManager::getIds(const std::string &name,
		std::unordered_set<content_t> &result) const
{
	//TimeTaker t("getIds", NULL, PRECISION_MICRO);
	if (name.substr(0,6) != "group:") {
		content_t id = CONTENT_IGNORE;
		if(getId(name, id))
			result.insert(id);
		return;
	}
	std::string group = name.substr(6);

	std::map<std::string, GroupItems>::const_iterator
		i = m_group_to_items.find(group);
	if (i == m_group_to_items.end())
		return;

	const GroupItems &items = i->second;
	for (GroupItems::const_iterator j = items.begin();
		j != items.end(); ++j) {
		if ((*j).second != 0)
			result.insert((*j).first);
	}
	//printf("getIds: %dus\n", t.stop());
}

	void CNodeDefManager::getIds(const std::string &name, FMBitset &result) const {
		if(name.substr(0,6) != "group:"){
			content_t id = CONTENT_IGNORE;
			if(getId(name, id))
				result.set(id, true);
			return;
		}
		std::string group = name.substr(6);

		std::map<std::string, GroupItems>::const_iterator
			i = m_group_to_items.find(group);
		if (i == m_group_to_items.end())
			return;

		const GroupItems &items = i->second;
		for (GroupItems::const_iterator j = items.begin();
			j != items.end(); ++j) {
			if ((*j).second != 0)
				result.set((*j).first, true);
		}
	}


const ContentFeatures& CNodeDefManager::get(const std::string &name) const
{
	content_t id = CONTENT_UNKNOWN;
	getId(name, id);
	return get(id);
}


// returns CONTENT_IGNORE if no free ID found
content_t CNodeDefManager::allocateId()
{
	for (content_t id = m_next_id;
			id >= m_next_id; // overflow?
			++id) {
		while (id >= m_content_features.size()) {
			m_content_features.push_back(ContentFeatures());
		}
		const ContentFeatures &f = m_content_features[id];
		if (f.name == "") {
			m_next_id = id + 1;
			return id;
		}
	}
	// If we arrive here, an overflow occurred in id.
	// That means no ID was found
	return CONTENT_IGNORE;
}


// IWritableNodeDefManager
content_t CNodeDefManager::set(const std::string &name, const ContentFeatures &def)
{
	// Pre-conditions
	assert(name != "");
	assert(name == def.name);

	// Don't allow redefining ignore (but allow air and unknown)
	if (name == "ignore") {
		infostream << "NodeDefManager: WARNING: Ignoring "
			"CONTENT_IGNORE redefinition"<<std::endl;
		return CONTENT_IGNORE;
	}

	content_t id = CONTENT_IGNORE;
	if (!m_name_id_mapping.getId(name, id)) { // ignore aliases
		// Get new id
		id = allocateId();
		if (id == CONTENT_IGNORE) {
			infostream << "NodeDefManager: WARNING: Absolute "
				"limit reached" << std::endl;
			return CONTENT_IGNORE;
		}
		assert(id != CONTENT_IGNORE);
		addNameIdMapping(id, name);
	}
	m_content_features[id] = def;
	verbosestream << "NodeDefManager: registering content id \"" << id
		<< "\": name=\"" << def.name << "\""<<std::endl;

	// Add this content to the list of all groups it belongs to
	// FIXME: This should remove a node from groups it no longer
	// belongs to when a node is re-registered
	for (ItemGroupList::const_iterator i = def.groups.begin();
		i != def.groups.end(); ++i) {
		std::string group_name = i->first;

		std::map<std::string, GroupItems>::iterator
			j = m_group_to_items.find(group_name);
		if (j == m_group_to_items.end()) {
			m_group_to_items[group_name].push_back(
					std::make_pair(id, i->second));
		} else {
			GroupItems &items = j->second;
			items.push_back(std::make_pair(id, i->second));
		}
	}
	return id;
}


content_t CNodeDefManager::allocateDummy(const std::string &name)
{
	assert(name != "");	// Pre-condition
	ContentFeatures f;
	f.name = name;
	return set(name, f);
}


void CNodeDefManager::updateAliases(IItemDefManager *idef)
{
	std::set<std::string> all = idef->getAll();
	m_name_id_mapping_with_aliases.clear();
	for (std::set<std::string>::iterator
			i = all.begin(); i != all.end(); i++) {
		std::string name = *i;
		std::string convert_to = idef->getAlias(name);
		content_t id;
		if (m_name_id_mapping.getId(convert_to, id)) {
			m_name_id_mapping_with_aliases.insert(
					std::make_pair(name, id));
		}
	}
}


void CNodeDefManager::updateTextures(IGameDef *gamedef)
{
	infostream << "CNodeDefManager::updateTextures(): Updating "
		"textures in node definitions" << std::endl;

	ITextureSource *tsrc = !gamedef ? nullptr : gamedef->tsrc();
	IShaderSource *shdsrc = !gamedef ? nullptr : gamedef->getShaderSource();
	scene::ISceneManager* smgr = !gamedef ? nullptr : gamedef->getSceneManager();
	scene::IMeshManipulator* meshmanip = !smgr ? nullptr :smgr->getMeshManipulator();

	bool new_style_water           = g_settings->getBool("new_style_water");
	bool new_style_leaves          = g_settings->getBool("new_style_leaves");
	bool connected_glass           = g_settings->getBool("connected_glass");
	bool opaque_water              = g_settings->getBool("opaque_water");
	bool enable_shaders            = g_settings->getBool("enable_shaders");
	bool enable_bumpmapping        = g_settings->getBool("enable_bumpmapping");
	bool enable_parallax_occlusion = g_settings->getBool("enable_parallax_occlusion");
	bool enable_mesh_cache         = g_settings->getBool("enable_mesh_cache");

	bool use_normal_texture = enable_shaders &&
		(enable_bumpmapping || enable_parallax_occlusion);

	for (u32 i = 0; i < m_content_features.size(); i++) {
		ContentFeatures *f = &m_content_features[i];

		// Figure out the actual tiles to use
		TileDef tiledef[6];
		for (u32 j = 0; j < 6; j++) {
			tiledef[j] = f->tiledef[j];
			if (tiledef[j].name == "")
				tiledef[j].name = "unknown_node.png";
		}

		bool is_liquid = false;
		bool is_water_surface = false;

		u8 material_type = (f->alpha == 255) ?
			TILE_MATERIAL_BASIC : TILE_MATERIAL_ALPHA;

		switch (f->drawtype) {
		default:
		case NDT_NORMAL:
			f->solidness = 2;
			break;
		case NDT_AIRLIKE:
			f->solidness = 0;
			break;
		case NDT_LIQUID:
			assert(f->liquid_type == LIQUID_SOURCE);
			if (opaque_water)
				f->alpha = 255;
			if (new_style_water){
				f->solidness = 0;
			} else {
				f->solidness = 1;
				f->backface_culling = false;
			}
			is_liquid = true;
			break;
		case NDT_FLOWINGLIQUID:
			assert(f->liquid_type == LIQUID_FLOWING);
			f->solidness = 0;
			if (opaque_water)
				f->alpha = 255;
			is_liquid = true;
			break;
		case NDT_GLASSLIKE:
			f->solidness = 0;
			f->visual_solidness = 1;
			break;
		case NDT_GLASSLIKE_FRAMED:
			f->solidness = 0;
			f->visual_solidness = 1;
			break;
		case NDT_GLASSLIKE_FRAMED_OPTIONAL:
			f->solidness = 0;
			f->visual_solidness = 1;
			f->drawtype = connected_glass ? NDT_GLASSLIKE_FRAMED : NDT_GLASSLIKE;
			break;
		case NDT_ALLFACES:
			f->solidness = 0;
			f->visual_solidness = 1;
			break;
		case NDT_ALLFACES_OPTIONAL:
			if (new_style_leaves) {
				f->drawtype = NDT_ALLFACES;
				f->solidness = 0;
				f->visual_solidness = 1;
			} else {
				f->drawtype = NDT_NORMAL;
				f->solidness = 2;
				for (u32 i = 0; i < 6; i++)
					tiledef[i].name += std::string("^[noalpha");
			}
			if (f->waving == 1)
				material_type = TILE_MATERIAL_WAVING_LEAVES;
			break;
		case NDT_PLANTLIKE:
			f->solidness = 0;
			f->backface_culling = false;
			if (f->waving == 1)
				material_type = TILE_MATERIAL_WAVING_PLANTS;
			break;
		case NDT_FIRELIKE:
			f->backface_culling = false;
			f->solidness = 0;
			break;
		case NDT_MESH:
			f->solidness = 0;
			f->backface_culling = false;
			break;
		case NDT_TORCHLIKE:
		case NDT_SIGNLIKE:
		case NDT_FENCELIKE:
		case NDT_RAILLIKE:
		case NDT_NODEBOX:
			f->solidness = 0;
			break;
		}

#ifndef SERVER

		if (is_liquid) {
			material_type = (f->alpha == 255) ?
				TILE_MATERIAL_LIQUID_OPAQUE : TILE_MATERIAL_LIQUID_TRANSPARENT;
			if (f->name == "default:water_source")
				is_water_surface = true;
		}

		u32 tile_shader[6];
		if (shdsrc) {
		for (u16 j = 0; j < 6; j++) {
			tile_shader[j] = shdsrc->getShader("nodes_shader",
				material_type, f->drawtype);
		}

		if (is_water_surface) {
			tile_shader[0] = shdsrc->getShader("water_surface_shader",
				material_type, f->drawtype);
		}
		}
		if (tsrc) {
		// Tiles (fill in f->tiles[])
		for (u16 j = 0; j < 6; j++) {
			fillTileAttribs(tsrc, &f->tiles[j], &tiledef[j], tile_shader[j],
				use_normal_texture, f->backface_culling, f->alpha, material_type);
		}

		// Special tiles (fill in f->special_tiles[])
		for (u16 j = 0; j < CF_SPECIAL_COUNT; j++) {
			fillTileAttribs(tsrc, &f->special_tiles[j], &f->tiledef_special[j],
				tile_shader[j], use_normal_texture,
				f->tiledef_special[j].backface_culling, f->alpha, material_type);
		}

		if ((f->drawtype == NDT_MESH) && (f->mesh != "")) {
			// Meshnode drawtype
			// Read the mesh and apply scale
			f->mesh_ptr[0] = gamedef->getMesh(f->mesh);
			if (f->mesh_ptr[0]){
				v3f scale = v3f(1.0, 1.0, 1.0) * BS * f->visual_scale;
				scaleMesh(f->mesh_ptr[0], scale);
				recalculateBoundingBox(f->mesh_ptr[0]);
				meshmanip->recalculateNormals(f->mesh_ptr[0], true, false);
			}
		} else if ((f->drawtype == NDT_NODEBOX) &&
				((f->node_box.type == NODEBOX_REGULAR) ||
				(f->node_box.type == NODEBOX_FIXED)) &&
				(!f->node_box.fixed.empty())) {
			//Convert regular nodebox nodes to meshnodes
			//Change the drawtype and apply scale
			f->drawtype = NDT_MESH;
			f->mesh_ptr[0] = convertNodeboxNodeToMesh(f);
			v3f scale = v3f(1.0, 1.0, 1.0) * f->visual_scale;
			scaleMesh(f->mesh_ptr[0], scale);
			recalculateBoundingBox(f->mesh_ptr[0]);
			meshmanip->recalculateNormals(f->mesh_ptr[0], true, false);
		}

		//Cache 6dfacedir and wallmounted rotated clones of meshes
		if (enable_mesh_cache && f->mesh_ptr[0] && (f->param_type_2 == CPT2_FACEDIR)) {
			for (u16 j = 1; j < 24; j++) {
				f->mesh_ptr[j] = cloneMesh(f->mesh_ptr[0]);
				rotateMeshBy6dFacedir(f->mesh_ptr[j], j);
				recalculateBoundingBox(f->mesh_ptr[j]);
				meshmanip->recalculateNormals(f->mesh_ptr[j], true, false);
			}
		} else if (enable_mesh_cache && f->mesh_ptr[0] && (f->param_type_2 == CPT2_WALLMOUNTED)) {
			static const u8 wm_to_6d[6] = {20, 0, 16+1, 12+3, 8, 4+2};
			for (u16 j = 1; j < 6; j++) {
				f->mesh_ptr[j] = cloneMesh(f->mesh_ptr[0]);
				rotateMeshBy6dFacedir(f->mesh_ptr[j], wm_to_6d[j]);
				recalculateBoundingBox(f->mesh_ptr[j]);
				meshmanip->recalculateNormals(f->mesh_ptr[j], true, false);
			}
			rotateMeshBy6dFacedir(f->mesh_ptr[0], wm_to_6d[0]);
			recalculateBoundingBox(f->mesh_ptr[0]);
			meshmanip->recalculateNormals(f->mesh_ptr[0], true, false);
		}
		f->color_avg = tsrc->getTextureInfo(f->tiles[0].texture_id)->color; // TODO: make average
		}
#endif
	}
}


#ifndef SERVER
void CNodeDefManager::fillTileAttribs(ITextureSource *tsrc, TileSpec *tile,
		TileDef *tiledef, u32 shader_id, bool use_normal_texture,
		bool backface_culling, u8 alpha, u8 material_type)
{
	tile->shader_id     = shader_id;
	tile->texture       = tsrc->getTexture(tiledef->name, &tile->texture_id);
	tile->alpha         = alpha;
	tile->material_type = material_type;

	// Normal texture
	if (use_normal_texture)
		tile->normal_texture = tsrc->getNormalTexture(tiledef->name);

	// Material flags
	tile->material_flags = 0;
	if (backface_culling)
		tile->material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;
	if (tiledef->animation.type == TAT_VERTICAL_FRAMES)
		tile->material_flags |= MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES;

	// Animation parameters
	int frame_count = 1;
	if (tile->material_flags & MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES) {
		// Get texture size to determine frame count by aspect ratio
		v2u32 size = tile->texture->getOriginalSize();
		int frame_height = (float)size.X /
				(tiledef->animation.aspect_w ? (float)tiledef->animation.aspect_w : 1) *
				(tiledef->animation.aspect_h ? (float)tiledef->animation.aspect_h : 1);
		frame_count = size.Y / (frame_height ? frame_height : size.Y ? size.Y : 1);
		int frame_length_ms = 1000.0 * tiledef->animation.length / frame_count;
		tile->animation_frame_count = frame_count;
		tile->animation_frame_length_ms = frame_length_ms;
	}

	if (frame_count == 1) {
		tile->material_flags &= ~MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES;
	} else {
		std::ostringstream os(std::ios::binary);
		tile->frames.resize(frame_count);

		for (int i = 0; i < frame_count; i++) {

			FrameSpec frame;

			os.str("");
			os << tiledef->name << "^[verticalframe:"
				<< frame_count << ":" << i;

			frame.texture = tsrc->getTexture(os.str(), &frame.texture_id);
			if (tile->normal_texture)
				frame.normal_texture = tsrc->getNormalTexture(os.str());
			tile->frames[i] = frame;
		}
	}
}
#endif

// map of content features, key = id, value = ContentFeatures
void CNodeDefManager::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	std::vector<std::pair<int, const ContentFeatures*> > features_to_pack;
	for (size_t i = 0; i < m_content_features.size(); ++i) {
		if (i == CONTENT_IGNORE || i == CONTENT_AIR || i == CONTENT_UNKNOWN || m_content_features[i].name == "")
			continue;
		features_to_pack.push_back(std::make_pair(i, &m_content_features[i]));
	}
	pk.pack_map(features_to_pack.size());
	for (size_t i = 0; i < features_to_pack.size(); ++i)
		PACK(features_to_pack[i].first, *features_to_pack[i].second);
}
void CNodeDefManager::msgpack_unpack(msgpack::object o)
{
	clear();

	std::map<int, ContentFeatures> unpacked_features;
	o.convert(&unpacked_features);

	for (std::map<int, ContentFeatures>::iterator it = unpacked_features.begin();
			it != unpacked_features.end(); ++it) {
		unsigned int i = it->first;
		ContentFeatures f = it->second;

		if(i == CONTENT_IGNORE || i == CONTENT_AIR
				|| i == CONTENT_UNKNOWN){
			infostream<<"NodeDefManager::deSerialize(): WARNING: "
				<<"not changing builtin node "<<i
				<<std::endl;
			continue;
		}
		if(f.name == ""){
			infostream<<"NodeDefManager::deSerialize(): WARNING: "
				<<"received empty name"<<std::endl;
			continue;
		}
		u16 existing_id;
		bool found = m_name_id_mapping.getId(f.name, existing_id);  // ignore aliases
		if(found && i != existing_id){
			infostream<<"NodeDefManager::deSerialize(): WARNING: "
				<<"already defined with different ID: "
				<<f.name<<std::endl;
			continue;
		}

		// All is ok, add node definition with the requested ID
		if(i >= m_content_features.size())
			m_content_features.resize((u32)(i) + 1);
		m_content_features[i] = f;
		addNameIdMapping(i, f.name);
		verbosestream<<"deserialized "<<f.name<<std::endl;
	}
}


void CNodeDefManager::addNameIdMapping(content_t i, std::string name)
{
	m_name_id_mapping.set(i, name);
	m_name_id_mapping_with_aliases.insert(std::make_pair(name, i));
}


IWritableNodeDefManager *createNodeDefManager()
{
	return new CNodeDefManager();
}


inline bool CNodeDefManager::getNodeRegistrationStatus() const
{
	return m_node_registration_complete;
}


inline void CNodeDefManager::setNodeRegistrationStatus(bool completed)
{
	m_node_registration_complete = completed;
}


void CNodeDefManager::pendNodeResolve(NodeResolveInfo *nri)
{
	nri->resolver->m_ndef = this;
	if (m_node_registration_complete) {
		nri->resolver->resolveNodeNames(nri);
		nri->resolver->m_lookup_done = true;
		delete nri;
	} else {
		m_pending_node_lookups.push_back(nri);
	}
}


void CNodeDefManager::cancelNodeResolve(NodeResolver *resolver)
{
	for (std::vector<NodeResolveInfo *>::iterator
			it = m_pending_node_lookups.begin();
			it != m_pending_node_lookups.end();
			++it) {
		NodeResolveInfo *nri = *it;
		if (resolver == nri->resolver) {
			it = m_pending_node_lookups.erase(it);
			delete nri;
		}
	}
}


void CNodeDefManager::runNodeResolverCallbacks()
{
	while (!m_pending_node_lookups.empty()) {
		NodeResolveInfo *nri = m_pending_node_lookups.front();
		m_pending_node_lookups.erase(m_pending_node_lookups.begin());
		nri->resolver->resolveNodeNames(nri);
		nri->resolver->m_lookup_done = true;
		delete nri;
	}
}


bool CNodeDefManager::getIdFromResolveInfo(NodeResolveInfo *nri,
	const std::string &node_alt, content_t c_fallback, content_t &result)
{
	if (nri->nodenames.empty()) {
		result = c_fallback;
		infostream << "Resolver empty nodename list" << std::endl;
		return false;
	}

	content_t c;
	std::string name = nri->nodenames.front();
	nri->nodenames.erase(nri->nodenames.begin());

	bool success = getId(name, c);
	if (!success && node_alt != "") {
		name = node_alt;
		success = getId(name, c);
	}

	if (!success) {
		errorstream << "Resolver: Failed to resolve node name '" << name
			<< "'." << std::endl;
		c = c_fallback;
	}

	result = c;
	return success;
}


bool CNodeDefManager::getIdsFromResolveInfo(NodeResolveInfo *nri,
	std::vector<content_t> &result)
{
	bool success = true;

	if (nri->nodelistinfo.empty()) {
		errorstream << "Resolver: Empty nodelistinfo list" << std::endl;
		return false;
	}

	NodeListInfo listinfo = nri->nodelistinfo.front();
	nri->nodelistinfo.pop_front();

	while (listinfo.length--) {
		if (nri->nodenames.empty()) {
			infostream << "Resolver: Empty nodename list" << std::endl;
			return false;
		}

		content_t c;
		std::string name = nri->nodenames.front();
		nri->nodenames.erase(nri->nodenames.begin());

		if (name.substr(0,6) != "group:") {
			if (getId(name, c)) {
				result.push_back(c);
			} else if (listinfo.all_required) {
				errorstream << "Resolver: Failed to resolve node name '" << name
					<< "'." << std::endl;
				result.push_back(listinfo.c_fallback);
				success = false;
			}
		} else {
			std::unordered_set<content_t> cids;
			getIds(name, cids);
			for (auto it = cids.begin(); it != cids.end(); ++it)
				result.push_back(*it);
		}
	}

	return success;
}
