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
#include "tile.h"
#endif
#include "log.h"
#include "settings.h"
#include "nameidmapping.h"
#include "util/numeric.h"
#include "util/serialize.h"
//#include "profiler.h" // For TimeTaker
#include "connection.h"

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
#ifndef SERVER
	solidness = 2;
	visual_solidness = 0;
	backface_culling = true;
#endif
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
	visual_scale = 1.0;
	for(u32 i=0; i<6; i++)
		tiledef[i] = TileDef();
	for(u16 j=0; j<CF_SPECIAL_COUNT; j++)
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
	freeze = "";
	melt = "";
	drowning = 0;
	light_source = 0;
	damage_per_second = 0;
	node_box = NodeBox();
	selection_box = NodeBox();
	waving = 0;
	legacy_facedir_simple = false;
	legacy_wallmounted = false;
	sound_footstep = SimpleSoundSpec();
	sound_dig = SimpleSoundSpec("__group");
	sound_dug = SimpleSoundSpec();

	is_circuit_element = false;
	is_wire = false;
	is_connector = false;
	for(int i = 0; i < 6; ++i)
	{
		wire_connections[i] = 0;
	}
	for(int i = 0; i < 64; ++i)
	{
		circuit_element_states[i] = 0;
	}
	circuit_element_delay = 0;
}

void ContentFeatures::msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
{
	pk.pack_map(36);
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
}

void ContentFeatures::msgpack_unpack(msgpack::object o)
{
	MsgpackPacket packet = o.as<MsgpackPacket>();
	packet[CONTENTFEATURES_NAME].convert(&name);
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
}

/*
	CNodeDefManager
*/

class CNodeDefManager: public IWritableNodeDefManager
{
public:
	void clear()
	{
		m_content_features.clear();
		m_name_id_mapping.clear();
		m_name_id_mapping_with_aliases.clear();
		m_group_to_items.clear();
		m_next_id = 0;

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
	CNodeDefManager()
	{
		clear();
	}
	virtual ~CNodeDefManager()
	{
	}
	virtual IWritableNodeDefManager* clone()
	{
		CNodeDefManager *mgr = new CNodeDefManager();
		*mgr = *this;
		return mgr;
	}
	virtual const ContentFeatures& get(content_t c) const
	{
		if(c < m_content_features.size())
			return m_content_features[c];
		else
			return m_content_features[CONTENT_UNKNOWN];
	}
	virtual const ContentFeatures& get(const MapNode &n) const
	{
		return get(n.getContent());
	}
	virtual bool getId(const std::string &name, content_t &result) const
	{
		std::map<std::string, content_t>::const_iterator
			i = m_name_id_mapping_with_aliases.find(name);
		if(i == m_name_id_mapping_with_aliases.end())
			return false;
		result = i->second;
		return true;
	}
	virtual content_t getId(const std::string &name) const
	{
		content_t id = CONTENT_IGNORE;
		getId(name, id);
		return id;
	}
	virtual void getIds(const std::string &name, std::set<content_t> &result)
			const
	{
		//TimeTaker t("getIds", NULL, PRECISION_MICRO);
		if(name.substr(0,6) != "group:"){
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
	virtual void getIds(const std::string &name, FMBitset &result) const {
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
	virtual const ContentFeatures& get(const std::string &name) const
	{
		content_t id = CONTENT_UNKNOWN;
		getId(name, id);
		return get(id);
	}
	// returns CONTENT_IGNORE if no free ID found
	content_t allocateId()
	{
		for(content_t id = m_next_id;
				id >= m_next_id; // overflow?
				++id){
			while(id >= m_content_features.size()){
				m_content_features.push_back(ContentFeatures());
			}
			const ContentFeatures &f = m_content_features[id];
			if(f.name == ""){
				m_next_id = id + 1;
				return id;
			}
		}
		// If we arrive here, an overflow occurred in id.
		// That means no ID was found
		return CONTENT_IGNORE;
	}
	// IWritableNodeDefManager
	virtual content_t set(const std::string &name,
			const ContentFeatures &def)
	{
		assert(name != "");
		assert(name == def.name);

		// Don't allow redefining ignore (but allow air and unknown)
		if(name == "ignore"){
			infostream<<"NodeDefManager: WARNING: Ignoring "
					<<"CONTENT_IGNORE redefinition"<<std::endl;
			return CONTENT_IGNORE;
		}

		content_t id = CONTENT_IGNORE;
		bool found = m_name_id_mapping.getId(name, id);  // ignore aliases
		if(!found){
			// Get new id
			id = allocateId();
			if(id == CONTENT_IGNORE){
				infostream<<"NodeDefManager: WARNING: Absolute "
						<<"limit reached"<<std::endl;
				return CONTENT_IGNORE;
			}
			assert(id != CONTENT_IGNORE);
			addNameIdMapping(id, name);
		}
		m_content_features[id] = def;
		verbosestream<<"NodeDefManager: registering content id \""<<id
				<<"\": name=\""<<def.name<<"\""<<std::endl;

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
	virtual content_t allocateDummy(const std::string &name)
	{
		assert(name != "");
		ContentFeatures f;
		f.name = name;
		return set(name, f);
	}
	virtual void updateAliases(IItemDefManager *idef)
	{
		std::set<std::string> all = idef->getAll();
		m_name_id_mapping_with_aliases.clear();
		for(std::set<std::string>::iterator
				i = all.begin(); i != all.end(); i++)
		{
			std::string name = *i;
			std::string convert_to = idef->getAlias(name);
			content_t id;
			if(m_name_id_mapping.getId(convert_to, id))
			{
				m_name_id_mapping_with_aliases.insert(
						std::make_pair(name, id));
			}
		}
	}
	virtual void updateTextures(ITextureSource *tsrc,
		IShaderSource *shdsrc)
	{
#ifndef SERVER
		infostream<<"CNodeDefManager::updateTextures(): Updating "
				<<"textures in node definitions"<<std::endl;

		bool new_style_water = g_settings->getBool("new_style_water");
		bool new_style_leaves = g_settings->getBool("new_style_leaves");
		bool opaque_water = g_settings->getBool("opaque_water");

		for(u32 i=0; i<m_content_features.size(); i++)
		{
			ContentFeatures *f = &m_content_features[i];

			// Figure out the actual tiles to use
			TileDef tiledef[6];
			for(u32 j=0; j<6; j++)
			{
				tiledef[j] = f->tiledef[j];
				if(tiledef[j].name == "")
					tiledef[j].name = "unknown_node.png";
			}

			bool is_liquid = false;
			bool is_water_surface = false;

			u8 material_type;
			material_type = (f->alpha == 255) ? TILE_MATERIAL_BASIC : TILE_MATERIAL_ALPHA;

			switch(f->drawtype){
			default:
			case NDT_NORMAL:
				f->solidness = 2;
				break;
			case NDT_AIRLIKE:
				f->solidness = 0;
				break;
			case NDT_LIQUID:
				assert(f->liquid_type == LIQUID_SOURCE);
				if(opaque_water)
					f->alpha = 255;
				if(new_style_water){
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
				if(opaque_water)
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
			case NDT_ALLFACES:
				f->solidness = 0;
				f->visual_solidness = 1;
				break;
			case NDT_ALLFACES_OPTIONAL:
				if(new_style_leaves){
					f->drawtype = NDT_ALLFACES;
					f->solidness = 0;
					f->visual_solidness = 1;
				} else {
					f->drawtype = NDT_NORMAL;
					f->solidness = 2;
					for(u32 i=0; i<6; i++){
						tiledef[i].name += std::string("^[noalpha");
					}
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
			case NDT_TORCHLIKE:
			case NDT_SIGNLIKE:
			case NDT_FENCELIKE:
			case NDT_RAILLIKE:
			case NDT_NODEBOX:
				f->solidness = 0;
				break;
			}

			if (is_liquid){
				material_type = (f->alpha == 255) ? TILE_MATERIAL_LIQUID_OPAQUE : TILE_MATERIAL_LIQUID_TRANSPARENT;
				if (f->name == "default:water_source")
					is_water_surface = true;
			}
			u32 tile_shader[6];
			for(u16 j=0; j<6; j++)
				tile_shader[j] = shdsrc->getShader("nodes_shader",material_type, f->drawtype);

			if (is_water_surface)
				tile_shader[0] = shdsrc->getShader("water_surface_shader",material_type, f->drawtype);

			// Tiles (fill in f->tiles[])
			for(u16 j=0; j<6; j++){
				// Shader
				f->tiles[j].shader_id = tile_shader[j];
				// Texture
				f->tiles[j].texture = tsrc->getTexture(
						tiledef[j].name,
						&f->tiles[j].texture_id);
				// Alpha
				f->tiles[j].alpha = f->alpha;
				// Material type
				f->tiles[j].material_type = material_type;
				// Material flags
				f->tiles[j].material_flags = 0;
				if(f->backface_culling)
					f->tiles[j].material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;
				if(tiledef[j].animation.type == TAT_VERTICAL_FRAMES)
					f->tiles[j].material_flags |= MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES;
				// Animation parameters
				if(f->tiles[j].material_flags &
						MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES)
				{
					// Get texture size to determine frame count by
					// aspect ratio
					v2u32 size = f->tiles[j].texture->getOriginalSize();
					int frame_height = (float)size.X /
							(tiledef[j].animation.aspect_w ? (float)tiledef[j].animation.aspect_w : 1) *
							(tiledef[j].animation.aspect_h ? (float)tiledef[j].animation.aspect_h : 1);
					int frame_count = size.Y / (frame_height ? frame_height : size.Y ? size.Y : 1);
					int frame_length_ms = 1000.0 *
							tiledef[j].animation.length / frame_count;
					f->tiles[j].animation_frame_count = frame_count;
					f->tiles[j].animation_frame_length_ms = frame_length_ms;

					// If there are no frames for an animation, switch
					// animation off (so that having specified an animation
					// for something but not using it in the texture pack
					// gives no overhead)
					if(frame_count == 1){
						f->tiles[j].material_flags &=
								~MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES;
					}
				}
			}
			f->color_avg = tsrc->getTextureInfo(f->tiles[0].texture_id)->color; // TODO: make average
			// Special tiles (fill in f->special_tiles[])
			for(u16 j=0; j<CF_SPECIAL_COUNT; j++){
				// Shader
				f->special_tiles[j].shader_id = tile_shader[j];
				// Texture
				f->special_tiles[j].texture = tsrc->getTexture(
						f->tiledef_special[j].name,
						&f->special_tiles[j].texture_id);
				// Alpha
				f->special_tiles[j].alpha = f->alpha;
				// Material type
				f->special_tiles[j].material_type = material_type;
				// Material flags
				f->special_tiles[j].material_flags = 0;
				if(f->tiledef_special[j].backface_culling)
					f->special_tiles[j].material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;
				if(f->tiledef_special[j].animation.type == TAT_VERTICAL_FRAMES)
					f->special_tiles[j].material_flags |= MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES;
				// Animation parameters
				if(f->special_tiles[j].material_flags &
						MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES)
				{
					// Get texture size to determine frame count by
					// aspect ratio
					v2u32 size = f->special_tiles[j].texture->getOriginalSize();
					int frame_height = (float)size.X /
							(f->tiledef_special[j].animation.aspect_w ? (float)f->tiledef_special[j].animation.aspect_w : 1) *
							(f->tiledef_special[j].animation.aspect_h ? (float)f->tiledef_special[j].animation.aspect_h : 1);
					int frame_count = size.Y / (frame_height ? frame_height : size.Y ? size.Y : 1);
					int frame_length_ms = 1000.0 *
							f->tiledef_special[j].animation.length / frame_count;
					f->special_tiles[j].animation_frame_count = frame_count;
					f->special_tiles[j].animation_frame_length_ms = frame_length_ms;

					// If there are no frames for an animation, switch
					// animation off (so that having specified an animation
					// for something but not using it in the texture pack
					// gives no overhead)
					if(frame_count == 1){
						f->special_tiles[j].material_flags &=
								~MATERIAL_FLAG_ANIMATION_VERTICAL_FRAMES;
					}
				}
			}
		}
#endif
	}
	// map of content features, key = id, value = ContentFeatures
	void msgpack_pack(msgpack::packer<msgpack::sbuffer> &pk) const
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
	void msgpack_unpack(msgpack::object o)
	{
		clear();

		std::map<int, ContentFeatures> unpacked_features;
		o.convert(&unpacked_features);

		for (std::map<int, ContentFeatures>::iterator it = unpacked_features.begin();
				it != unpacked_features.end(); ++it) {
			int i = it->first;
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
private:
	void addNameIdMapping(content_t i, std::string name)
	{
		m_name_id_mapping.set(i, name);
		m_name_id_mapping_with_aliases.insert(std::make_pair(name, i));
	}
private:
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
};

IWritableNodeDefManager* createNodeDefManager()
{
	return new CNodeDefManager();
}
