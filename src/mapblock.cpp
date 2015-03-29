/*
mapblock.cpp
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

#include "mapblock.h"

#include <sstream>
#include "map.h"
#include "light.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "gamedef.h"
#include "log_types.h"
#include "nameidmapping.h"
#include "content_mapnode.h" // For legacy name-id mapping
#include "content_nodemeta.h" // For legacy deserialization
#include "serialization.h"
#ifndef SERVER
#include "mapblock_mesh.h"
#endif
#include "util/string.h"
#include "util/serialize.h"
#include "circuit.h"
#include "profiler.h"
#include <mutex>

#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"

/*
	MapBlock
*/

MapBlock::MapBlock(Map *parent, v3s16 pos, IGameDef *gamedef, bool dummy):
		heat_last_update(0),
		humidity_last_update(0),
		m_uptime_timer_last(0),
		m_parent(parent),
		m_pos(pos),
		m_gamedef(gamedef),
		m_modified(MOD_STATE_CLEAN),
		is_underground(false),
		m_day_night_differs(false),
		m_generated(false),
		m_disk_timestamp(BLOCK_TIMESTAMP_UNDEFINED),
		m_usage_timer(0)
{
	heat = 0;
	humidity = 0;
	m_timestamp = BLOCK_TIMESTAMP_UNDEFINED;
	m_changed_timestamp = 0;
	m_day_night_differs_expired = true;
	m_lighting_expired = true;
	m_refcount = 0;
	data = NULL;
	//if(dummy == false)
		reallocate();

#ifndef SERVER
	mesh = NULL;
	mesh2 = mesh4 = mesh8 = mesh16 = nullptr;
	mesh_size = 0;
#endif
	m_next_analyze_timestamp = 0;
	m_abm_timestamp = 0;
	content_only = CONTENT_IGNORE;
}

MapBlock::~MapBlock()
{
	//auto lock = lock_unique_rec();
#ifndef SERVER
	//delMesh();
#endif

	if(data)
		delete data;
	data = nullptr;
}

bool MapBlock::isValidPositionParent(v3s16 p)
{
	if(isValidPosition(p))
	{
		return true;
	}
	else{
		return m_parent->isValidPosition(getPosRelative() + p);
	}
}

MapNode MapBlock::getNodeParent(v3s16 p, bool *is_valid_position)
{
	if (isValidPosition(p) == false)
		return m_parent->getNodeTry(getPosRelative() + p);

	if (data == NULL) {
		if (is_valid_position)
			*is_valid_position = false;
		return MapNode(CONTENT_IGNORE);
	}
	auto lock = try_lock_shared_rec();
	if (!lock->owns_lock()) {
		if (is_valid_position)
			*is_valid_position = false;
		return MapNode(CONTENT_IGNORE);
	}

	if (is_valid_position)
		*is_valid_position = true;
	return data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X];
}

/*
	Propagates sunlight down through the block.
	Doesn't modify nodes that are not affected by sunlight.

	Returns false if sunlight at bottom block is invalid.
	Returns true if sunlight at bottom block is valid.
	Returns true if bottom block doesn't exist.

	If there is a block above, continues from it.
	If there is no block above, assumes there is sunlight, unless
	is_underground is set or highest node is water.

	All sunlighted nodes are added to light_sources.

	if remove_light==true, sets non-sunlighted nodes black.

	if black_air_left!=NULL, it is set to true if non-sunlighted
	air is left in block.
*/
bool MapBlock::propagateSunlight(std::set<v3s16> & light_sources,
		bool remove_light, bool *black_air_left)
{
	auto lock = lock_unique_rec();

	INodeDefManager *nodemgr = m_gamedef->ndef();

	// Whether the sunlight at the top of the bottom block is valid
	bool block_below_is_valid = true;

	v3s16 pos_relative = getPosRelative();

	for(s16 x=0; x<MAP_BLOCKSIZE; x++)
	{
		for(s16 z=0; z<MAP_BLOCKSIZE; z++)
		{
#if 1
			bool no_sunlight = false;
			//bool no_top_block = false;

			// Check if node above block has sunlight

			bool is_valid_position;
			MapNode n = getNodeParent(v3s16(x, MAP_BLOCKSIZE, z),
				&is_valid_position);
			if (n)
			{
				if(n.getLight(LIGHTBANK_DAY, m_gamedef->ndef()) != LIGHT_SUN)
				{
					no_sunlight = true;
				}
			}
			else
			{
				//no_top_block = true;

				// NOTE: This makes over-ground roofed places sunlighted
				// Assume sunlight, unless is_underground==true
				if(is_underground)
				{
					no_sunlight = true;
				}
				else
				{
					MapNode n = getNodeNoEx(v3s16(x, MAP_BLOCKSIZE-1, z));
					if(n && m_gamedef->ndef()->get(n).sunlight_propagates == false)
					{
						no_sunlight = true;
					}
				}
				// NOTE: As of now, this just would make everything dark.
				// No sunlight here
				//no_sunlight = true;
			}
#endif
#if 0 // Doesn't work; nothing gets light.
			bool no_sunlight = true;
			bool no_top_block = false;
			// Check if node above block has sunlight
			try{
				MapNode n = getNodeParent(v3s16(x, MAP_BLOCKSIZE, z));
				if(n.getLight(LIGHTBANK_DAY) == LIGHT_SUN)
				{
					no_sunlight = false;
				}
			}
			catch(InvalidPositionException &e)
			{
				no_top_block = true;
			}
#endif

			/*std::cout<<"("<<x<<","<<z<<"): "
					<<"no_top_block="<<no_top_block
					<<", is_underground="<<is_underground
					<<", no_sunlight="<<no_sunlight
					<<std::endl;*/

			s16 y = MAP_BLOCKSIZE-1;

			// This makes difference to diminishing in water.
			bool stopped_to_solid_object = false;

			u8 current_light = no_sunlight ? 0 : LIGHT_SUN;

			for(; y >= 0; y--)
			{
				v3s16 pos(x, y, z);
				MapNode &n = getNodeRef(pos);

				if(current_light == 0)
				{
					// Do nothing
				}
				else if(current_light == LIGHT_SUN && nodemgr->get(n).sunlight_propagates)
				{
					// Do nothing: Sunlight is continued
				}
				else if(nodemgr->get(n).light_propagates == false)
				{
					// A solid object is on the way.
					stopped_to_solid_object = true;

					// Light stops.
					current_light = 0;
				}
				else
				{
					// Diminish light
					current_light = diminish_light(current_light);
				}

				u8 old_light = n.getLight(LIGHTBANK_DAY, nodemgr);

				if(current_light > old_light || remove_light)
				{
					n.setLight(LIGHTBANK_DAY, current_light, nodemgr);
				}

				if(diminish_light(current_light) != 0)
				{
					light_sources.insert(pos_relative + pos);
				}

				if(current_light == 0 && stopped_to_solid_object)
				{
					if(black_air_left)
					{
						*black_air_left = true;
					}
				}
			}

			// Whether or not the block below should see LIGHT_SUN
			bool sunlight_should_go_down = (current_light == LIGHT_SUN);

			/*
				If the block below hasn't already been marked invalid:

				Check if the node below the block has proper sunlight at top.
				If not, the block below is invalid.

				Ignore non-transparent nodes as they always have no light
			*/

			if(block_below_is_valid)
			{
				MapNode n = getNodeParent(v3s16(x, -1, z), &is_valid_position);
				if (n) {
					if(nodemgr->get(n).light_propagates)
					{
						if(n.getLight(LIGHTBANK_DAY, nodemgr) == LIGHT_SUN
								&& sunlight_should_go_down == false)
							block_below_is_valid = false;
						else if(n.getLight(LIGHTBANK_DAY, nodemgr) != LIGHT_SUN
								&& sunlight_should_go_down == true)
							block_below_is_valid = false;
					}
				}
				else
				{
					/*std::cout<<"InvalidBlockException for bottom block node"
							<<std::endl;*/
					// Just no block below, no need to panic.
				}
			}
		}
	}

	return block_below_is_valid;
}


void MapBlock::copyTo(VoxelManipulator &dst)
{
	auto lock = lock_shared_rec();
	v3s16 data_size(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);
	VoxelArea data_area(v3s16(0,0,0), data_size - v3s16(1,1,1));

	// Copy from data to VoxelManipulator
	dst.copyFrom(data, data_area, v3s16(0,0,0),
			getPosRelative(), data_size);
}

void MapBlock::copyFrom(VoxelManipulator &dst)
{
	auto lock = lock_unique_rec();
	v3s16 data_size(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);
	VoxelArea data_area(v3s16(0,0,0), data_size - v3s16(1,1,1));

	// Copy from VoxelManipulator to data
	dst.copyTo(data, data_area, v3s16(0,0,0),
			getPosRelative(), data_size);
}

void MapBlock::actuallyUpdateDayNightDiff()
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	// Running this function un-expires m_day_night_differs
	m_day_night_differs_expired = false;

	if (data == NULL) {
		m_day_night_differs = false;
		return;
	}

	bool differs;

	/*
		Check if any lighting value differs
	*/
	auto lock = lock_shared_rec();
	for (u32 i = 0; i < MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE; i++) {
		MapNode &n = data[i];

		differs = !n.isLightDayNightEq(nodemgr);
		if (differs)
			break;
	}

	/*
		If some lighting values differ, check if the whole thing is
		just air. If it is just air, differs = false
	*/
	if (differs) {
		bool only_air = true;
		for (u32 i = 0; i < MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE; i++) {
			MapNode &n = data[i];
			if (n.getContent() != CONTENT_AIR) {
				only_air = false;
				break;
			}
		}
		if (only_air)
			differs = false;
	}

	// Set member variable
	m_day_night_differs = differs;
}

void MapBlock::expireDayNightDiff()
{
	//INodeDefManager *nodemgr = m_gamedef->ndef();

	if(data == NULL){
		m_day_night_differs = false;
		m_day_night_differs_expired = false;
		return;
	}

	m_day_night_differs_expired = true;
}

s16 MapBlock::getGroundLevel(v2s16 p2d)
{
	auto lock = lock_shared_rec();
	if(isDummy())
		return -3;
	try
	{
		s16 y = MAP_BLOCKSIZE-1;
		for(; y>=0; y--)
		{
			MapNode n = getNodeRef(p2d.X, y, p2d.Y);
			if(m_gamedef->ndef()->get(n).walkable)
			{
				if(y == MAP_BLOCKSIZE-1)
					return -2;
				else
					return y;
			}
		}
		return -1;
	}
	catch(InvalidPositionException &e)
	{
		return -3;
	}
}

/*
	Serialization
*/
// List relevant id-name pairs for ids in the block using nodedef
// Renumbers the content IDs (starting at 0 and incrementing
// use static memory requires about 65535 * sizeof(int) ram in order to be
// sure we can handle all content ids. But it's absolutely worth it as it's
// a speedup of 4 for one of the major time consuming functions on storing
// mapblocks.
static content_t getBlockNodeIdMapping_mapping[USHRT_MAX];
static void getBlockNodeIdMapping(NameIdMapping *nimap, MapNode *nodes,
		INodeDefManager *nodedef)
{
	memset(getBlockNodeIdMapping_mapping, 0xFF, USHRT_MAX * sizeof(content_t));

	std::set<content_t> unknown_contents;
	content_t id_counter = 0;
	for(u32 i=0; i<MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE; i++)
	{
		content_t global_id = nodes[i].getContent();
		content_t id = CONTENT_IGNORE;

		// Try to find an existing mapping
		if (getBlockNodeIdMapping_mapping[global_id] != 0xFFFF) {
			id = getBlockNodeIdMapping_mapping[global_id];
		}
		else
		{
			// We have to assign a new mapping
			id = id_counter++;
			getBlockNodeIdMapping_mapping[global_id] = id;

			const ContentFeatures &f = nodedef->get(global_id);
			const std::string &name = f.name;
			if(name == "")
				unknown_contents.insert(global_id);
			else
				nimap->set(id, name);
		}

		// Update the MapNode
		nodes[i].setContent(id);
	}
	for(std::set<content_t>::const_iterator
			i = unknown_contents.begin();
			i != unknown_contents.end(); i++){
		errorstream<<"getBlockNodeIdMapping(): IGNORING ERROR: "
				<<"Name for node id "<<(*i)<<" not known"<<std::endl;
	}
}
// Correct ids in the block to match nodedef based on names.
// Unknown ones are added to nodedef.
// Will not update itself to match id-name pairs in nodedef.
static std::mutex correctBlockNodeIds_mutex;
static void correctBlockNodeIds(const NameIdMapping *nimap, MapNode *nodes,
		IGameDef *gamedef)
{
	INodeDefManager *nodedef = gamedef->ndef();
	// This means the block contains incorrect ids, and we contain
	// the information to convert those to names.
	// nodedef contains information to convert our names to globally
	// correct ids.
	std::set<content_t> unnamed_contents;
	std::set<std::string> unallocatable_contents;
	std::lock_guard<std::mutex> lock(correctBlockNodeIds_mutex);
	for(u32 i=0; i<MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE; i++)
	{
		content_t local_id = nodes[i].getContent();
		std::string name;
		bool found = nimap->getName(local_id, name);
		if(!found){
			unnamed_contents.insert(local_id);
			continue;
		}
		content_t global_id;
		found = nodedef->getId(name, global_id);
		if(!found){
			global_id = gamedef->allocateUnknownNodeId(name);
			if(global_id == CONTENT_IGNORE){
				unallocatable_contents.insert(name);
				continue;
			}
		}
		nodes[i].setContent(global_id);
	}
	for(std::set<content_t>::const_iterator
			i = unnamed_contents.begin();
			i != unnamed_contents.end(); i++){
		errorstream<<"correctBlockNodeIds(): IGNORING ERROR: "
				<<"Block contains id "<<(*i)
				<<" with no name mapping"<<std::endl;
	}
	for(std::set<std::string>::const_iterator
			i = unallocatable_contents.begin();
			i != unallocatable_contents.end(); i++){
		errorstream<<"correctBlockNodeIds(): IGNORING ERROR: "
				<<"Could not allocate global id for node name \""
				<<(*i)<<"\""<<std::endl;
	}
}

void MapBlock::serialize(std::ostream &os, u8 version, bool disk)
{
	auto lock = lock_shared_rec();
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

	if(data == NULL)
	{
		throw SerializationError("ERROR: Not writing dummy block.");
	}

	FATAL_ERROR_IF(version < SER_FMT_CLIENT_VER_LOWEST, "Serialize version error");

	// First byte
	u8 flags = 0;
	if(is_underground)
		flags |= 0x01;
	if(getDayNightDiff())
		flags |= 0x02;
	if(m_lighting_expired)
		flags |= 0x04;
	if(m_generated == false)
	{
		flags |= 0x08;
		infostream<<" serialize not generated block"<<std::endl;
	}

	writeU8(os, flags);

	/*
		Bulk node data
	*/
	NameIdMapping nimap;
	u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;
	if(disk)
	{
		MapNode *tmp_nodes = new MapNode[nodecount];
		for(u32 i=0; i<nodecount; i++)
			tmp_nodes[i] = data[i];
		getBlockNodeIdMapping(&nimap, tmp_nodes, m_gamedef->ndef());

		u8 content_width = 2;
		u8 params_width = 2;
		writeU8(os, content_width);
		writeU8(os, params_width);
		MapNode::serializeBulk(os, version, tmp_nodes, nodecount,
				content_width, params_width, true);
		delete[] tmp_nodes;
	}
	else
	{
		u8 content_width = 2;
		u8 params_width = 2;
		writeU8(os, content_width);
		writeU8(os, params_width);
		MapNode::serializeBulk(os, version, data, nodecount,
				content_width, params_width, true);
	}

	/*
		Node metadata
	*/
	std::ostringstream oss(std::ios_base::binary);
	m_node_metadata.serialize(oss);
	compressZlib(oss.str(), os);

	/*
		Data that goes to disk, but not the network
	*/
	if(disk)
	{
		if(version <= 24){
			// Node timers
			m_node_timers.serialize(os, version);
		}

		// Static objects
		m_static_objects.serialize(os);

		// Timestamp
		writeU32(os, getTimestamp());

		// Write block-specific node definition id mapping
		nimap.serialize(os);

		if(version >= 25){
			// Node timers
			m_node_timers.serialize(os, version);
		}
	}
}

void MapBlock::serializeNetworkSpecific(std::ostream &os, u16 net_proto_version)
{
	if(data == NULL)
	{
		throw SerializationError("ERROR: Not writing dummy block.");
	}

	if(net_proto_version >= 21){
		int version = 1;
		writeU8(os, version);
		writeF1000(os, heat); // deprecated heat
		writeF1000(os, humidity); // deprecated humidity
	}
}


bool MapBlock::deSerialize(std::istream &is, u8 version, bool disk)
{
	auto lock = lock_unique_rec();
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

	TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())<<std::endl);

	m_day_night_differs_expired = false;

	if(version <= 21)
	{
		deSerialize_pre22(is, version, disk);
		return true;
	}

	u8 flags = readU8(is);
	is_underground = (flags & 0x01) ? true : false;
	m_day_night_differs = (flags & 0x02) ? true : false;
	m_lighting_expired = (flags & 0x04) ? true : false;
	m_generated = (flags & 0x08) ? false : true;

	if (!m_generated) {
		infostream<<"MapBlock::deSerialize(): deserialize not generated block "<<getPos()<<std::endl;
		//if (disk) m_generated = false; else // uncomment if you want convert old buggy map
		return false;
	}

	/*
		Bulk node data
	*/
	TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
			<<": Bulk node data"<<std::endl);
	u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;
	u8 content_width = readU8(is);
	u8 params_width = readU8(is);
	if(content_width != 1 && content_width != 2)
		throw SerializationError("MapBlock::deSerialize(): invalid content_width");
	if(params_width != 2)
		throw SerializationError("MapBlock::deSerialize(): invalid params_width");
	MapNode::deSerializeBulk(is, version, data, nodecount,
			content_width, params_width, true);

	/*
		NodeMetadata
	*/
	TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
			<<": Node metadata"<<std::endl);
	// Ignore errors
	try{
		std::ostringstream oss(std::ios_base::binary);
		decompressZlib(is, oss);
		std::istringstream iss(oss.str(), std::ios_base::binary);
		if(version >= 23)
			m_node_metadata.deSerialize(iss, m_gamedef);
		else
			content_nodemeta_deserialize_legacy(iss,
					&m_node_metadata, &m_node_timers,
					m_gamedef);
	}
	catch(SerializationError &e)
	{
		errorstream<<"WARNING: MapBlock::deSerialize(): Ignoring an error"
				<<" while deserializing node metadata at ("
				<<PP(getPos())<<": "<<e.what()<<std::endl;
	}

	/*
		Data that is only on disk
	*/
	if(disk)
	{
		// Node timers
		if(version == 23){
			// Read unused zero
			readU8(is);
		}
		if(version == 24){
			TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
					<<": Node timers (ver==24)"<<std::endl);
			m_node_timers.deSerialize(is, version);
		}

		// Static objects
		TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
				<<": Static objects"<<std::endl);
		m_static_objects.deSerialize(is);

		// Timestamp
		TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
				<<": Timestamp"<<std::endl);
		setTimestampNoChangedFlag(readU32(is));
		m_disk_timestamp = m_timestamp;
		m_changed_timestamp = (unsigned int)m_timestamp != BLOCK_TIMESTAMP_UNDEFINED ? (unsigned int)m_timestamp : 0;

		// Dynamically re-set ids based on node names
		TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
				<<": NameIdMapping"<<std::endl);
		NameIdMapping nimap;
		nimap.deSerialize(is);
		correctBlockNodeIds(&nimap, data, m_gamedef);

		if(version >= 25){
			TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
					<<": Node timers (ver>=25)"<<std::endl);
			m_node_timers.deSerialize(is, version);
		}

		analyzeContent();
	}

	TRACESTREAM(<<"MapBlock::deSerialize "<<PP(getPos())
			<<": Done."<<std::endl);
	return true;
}

void MapBlock::deSerializeNetworkSpecific(std::istream &is)
{
	try {
		int version = readU8(is);
		//if(version != 1)
		//	throw SerializationError("unsupported MapBlock version");
		if(version >= 1) {
			heat = readF1000(is); // deprecated heat
			humidity = readF1000(is); // deprecated humidity
		}
	}
	catch(SerializationError &e)
	{
		errorstream<<"WARNING: MapBlock::deSerializeNetworkSpecific(): Ignoring an error"
				<<": "<<e.what()<<std::endl;
	}
}

	MapNode MapBlock::getNodeNoEx(v3POS p) {
#ifndef NDEBUG
		ScopeProfiler sp(g_profiler, "Map: getNodeNoEx");
#endif
		auto lock = lock_shared_rec();
		return getNodeNoLock(p);
	}

	void MapBlock::setNode(v3POS p, MapNode & n)
	{
#ifndef NDEBUG
		g_profiler->add("Map: setNode", 1);
#endif
		if( (!data) ||   //todo: maybe one length check here:
			(p.X < 0 || p.X >= MAP_BLOCKSIZE) ||
			(p.Y < 0 || p.Y >= MAP_BLOCKSIZE) ||
			(p.Z < 0 || p.Z >= MAP_BLOCKSIZE) )
			return;
		auto lock = lock_unique_rec();
		data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED);
	}

	void MapBlock::setNodeNoCheck(v3s16 p, MapNode & n)
	{
		if(data == NULL)
			throw InvalidPositionException("setNodeNoCheck data=NULL");
		auto lock = lock_unique_rec();
		data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X] = n;
		raiseModified(MOD_STATE_WRITE_NEEDED/*, "setNodeNoCheck"*/);
	}

	void MapBlock::raiseModified(u32 mod)
	{
		if(mod >= MOD_STATE_WRITE_NEEDED /*&& m_timestamp != BLOCK_TIMESTAMP_UNDEFINED*/) {
			m_changed_timestamp = (unsigned int)m_parent->time_life;
		}
		if(mod > m_modified){
			m_modified = mod;
			if(m_modified >= MOD_STATE_WRITE_AT_UNLOAD)
				m_disk_timestamp = m_timestamp;
		}
	}

void MapBlock::pushElementsToCircuit(Circuit* circuit)
{
}

#ifndef SERVER
MapBlock::mesh_type MapBlock::getMesh(int step) {
	if (step >= 16 && mesh16) return mesh16;
	if (step >= 8  && mesh8)  return mesh8;
	if (step >= 4  && mesh4)  return mesh4;
	if (step >= 2  && mesh2)  return mesh2;
	if (step >= 1  && mesh)   return mesh;
	if (mesh2)  return mesh2;
	if (mesh4)  return mesh4;
	if (mesh8)  return mesh8;
	if (mesh16) return mesh16;
	return mesh;
}

void MapBlock::setMesh(MapBlock::mesh_type & rmesh) {
	if (rmesh && !mesh_size)
		mesh_size = rmesh->getMesh()->getMeshBufferCount();
	     if (rmesh->step == 16) {mesh_old = mesh16; mesh16 = rmesh;}
	else if (rmesh->step == 8 ) {mesh_old = mesh8;  mesh8  = rmesh;}
	else if (rmesh->step == 4 ) {mesh_old = mesh4;  mesh4  = rmesh;}
	else if (rmesh->step == 2 ) {mesh_old = mesh2;  mesh2  = rmesh;}
	else                        {mesh_old = mesh;   mesh   = rmesh;}
}

/*
void MapBlock::delMesh() {
	if (mesh16) {mesh16 = nullptr;}
	if (mesh8)  {mesh8  = nullptr;}
	if (mesh4)  {mesh4  = nullptr;}
	if (mesh2)  {mesh2  = nullptr;}
	if (mesh)   {mesh   = nullptr;}
}
*/
#endif


/*
	Legacy serialization
*/

void MapBlock::deSerialize_pre22(std::istream &is, u8 version, bool disk)
{
	u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;

	// Initialize default flags
	is_underground = false;
	m_day_night_differs = false;
	m_lighting_expired = false;
	m_generated = true;

	// Make a temporary buffer
	u32 ser_length = MapNode::serializedLength(version);
	SharedBuffer<u8> databuf_nodelist(nodecount * ser_length);

	// These have no compression
	if(version <= 3 || version == 5 || version == 6)
	{
		char tmp;
		is.read(&tmp, 1);
		if(is.gcount() != 1)
			throw SerializationError
					("MapBlock::deSerialize: no enough input data");
		is_underground = tmp;
		is.read((char*)*databuf_nodelist, nodecount * ser_length);
		if((u32)is.gcount() != nodecount * ser_length)
			throw SerializationError
					("MapBlock::deSerialize: no enough input data");
	}
	else if(version <= 10)
	{
		u8 t8;
		is.read((char*)&t8, 1);
		is_underground = t8;

		{
			// Uncompress and set material data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if(s.size() != nodecount)
				throw SerializationError
						("MapBlock::deSerialize: invalid format");
			for(u32 i=0; i<s.size(); i++)
			{
				databuf_nodelist[i*ser_length] = s[i];
			}
		}
		{
			// Uncompress and set param data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if(s.size() != nodecount)
				throw SerializationError
						("MapBlock::deSerialize: invalid format");
			for(u32 i=0; i<s.size(); i++)
			{
				databuf_nodelist[i*ser_length + 1] = s[i];
			}
		}

		if(version >= 10)
		{
			// Uncompress and set param2 data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if(s.size() != nodecount)
				throw SerializationError
						("MapBlock::deSerialize: invalid format");
			for(u32 i=0; i<s.size(); i++)
			{
				databuf_nodelist[i*ser_length + 2] = s[i];
			}
		}
	}
	// All other versions (newest)
	else
	{
		u8 flags;
		is.read((char*)&flags, 1);
		is_underground = (flags & 0x01) ? true : false;
		m_day_night_differs = (flags & 0x02) ? true : false;
		m_lighting_expired = (flags & 0x04) ? true : false;
		if(version >= 18)
			m_generated = (flags & 0x08) ? false : true;

		// Uncompress data
		std::ostringstream os(std::ios_base::binary);
		decompress(is, os, version);
		std::string s = os.str();
		if(s.size() != nodecount*3)
			throw SerializationError
					("MapBlock::deSerialize: decompress resulted in size"
					" other than nodecount*3");

		// deserialize nodes from buffer
		for(u32 i=0; i<nodecount; i++)
		{
			databuf_nodelist[i*ser_length] = s[i];
			databuf_nodelist[i*ser_length + 1] = s[i+nodecount];
			databuf_nodelist[i*ser_length + 2] = s[i+nodecount*2];
		}

		/*
			NodeMetadata
		*/
		if(version >= 14)
		{
			// Ignore errors
			try{
				if(version <= 15)
				{
					std::string data = deSerializeString(is);
					std::istringstream iss(data, std::ios_base::binary);
					content_nodemeta_deserialize_legacy(iss,
							&m_node_metadata, &m_node_timers,
							m_gamedef);
				}
				else
				{
					//std::string data = deSerializeLongString(is);
					std::ostringstream oss(std::ios_base::binary);
					decompressZlib(is, oss);
					std::istringstream iss(oss.str(), std::ios_base::binary);
					content_nodemeta_deserialize_legacy(iss,
							&m_node_metadata, &m_node_timers,
							m_gamedef);
				}
			}
			catch(SerializationError &e)
			{
				errorstream<<"WARNING: MapBlock::deSerialize(): Ignoring an error"
						<<" while deserializing node metadata"<<std::endl;
			}
		}
	}

	// Deserialize node data
	for(u32 i=0; i<nodecount; i++)
	{
		data[i].deSerialize(&databuf_nodelist[i*ser_length], version);
	}

	if(disk)
	{
		/*
			Versions up from 9 have block objects. (DEPRECATED)
		*/
		if(version >= 9){
			u16 count = readU16(is);
			// Not supported and length not known if count is not 0
			if(count != 0){
				errorstream<<"WARNING: MapBlock::deSerialize_pre22(): "
						<<"Ignoring stuff coming at and after MBOs"<<std::endl;
				return;
			}
		}

		/*
			Versions up from 15 have static objects.
		*/
		if(version >= 15)
			m_static_objects.deSerialize(is);

		// Timestamp
		if(version >= 17){
			setTimestamp(readU32(is));
			m_disk_timestamp = m_timestamp;
		} else {
			setTimestamp(BLOCK_TIMESTAMP_UNDEFINED);
		}

		// Dynamically re-set ids based on node names
		NameIdMapping nimap;
		// If supported, read node definition id mapping
		if(version >= 21){
			nimap.deSerialize(is);
		// Else set the legacy mapping
		} else {
			content_mapnode_get_name_id_mapping(&nimap);
		}
		correctBlockNodeIds(&nimap, data, m_gamedef);
	}


	// Legacy data changes
	// This code has to convert from pre-22 to post-22 format.
	INodeDefManager *nodedef = m_gamedef->ndef();
	for(u32 i=0; i<nodecount; i++)
	{
		const ContentFeatures &f = nodedef->get(data[i].getContent());
		// Mineral
		if(nodedef->getId("default:stone") == data[i].getContent()
				&& data[i].getParam1() == 1)
		{
			data[i].setContent(nodedef->getId("default:stone_with_coal"));
			data[i].setParam1(0);
		}
		else if(nodedef->getId("default:stone") == data[i].getContent()
				&& data[i].getParam1() == 2)
		{
			data[i].setContent(nodedef->getId("default:stone_with_iron"));
			data[i].setParam1(0);
		}
		// facedir_simple
		if(f.legacy_facedir_simple)
		{
			data[i].setParam2(data[i].getParam1());
			data[i].setParam1(0);
		}
		// wall_mounted
		if(f.legacy_wallmounted)
		{
			u8 wallmounted_new_to_old[8] = {0x04, 0x08, 0x01, 0x02, 0x10, 0x20, 0, 0};
			u8 dir_old_format = data[i].getParam2();
			u8 dir_new_format = 0;
			for(u8 j=0; j<8; j++)
			{
				if((dir_old_format & wallmounted_new_to_old[j]) != 0)
				{
					dir_new_format = j;
					break;
				}
			}
			data[i].setParam2(dir_new_format);
		}
	}

}

void MapBlock::incrementUsageTimer(float dtime)
{
	auto lock = lock_unique_rec();
	m_usage_timer += dtime;
/*
#ifndef SERVER
	if(mesh){
		if(mesh->getUsageTimer() > 10)
			mesh->setStatic();
		else
			mesh->incrementUsageTimer(dtime);
	}
#endif
*/
}

/* here for errorstream
	void MapBlock::setTimestamp(u32 time)
	{
//infostream<<"setTimestamp = "<< time <<std::endl;
		m_timestamp = time;
		raiseModified(MOD_STATE_WRITE_AT_UNLOAD, "setTimestamp");
	}

	void MapBlock::setTimestampNoChangedFlag(u32 time)
	{
//infostream<<"setTimestampNoChangedFlag = "<< time <<std::endl;
		m_timestamp = time;
	}

	void MapBlock::raiseModified(u32 mod)
	{
		if(mod >= m_modified){
			m_modified = mod;
			if(m_modified >= MOD_STATE_WRITE_AT_UNLOAD)
				m_disk_timestamp = m_timestamp;
			if(m_modified >= MOD_STATE_WRITE_NEEDED) {
//infostream<<"raiseModified = "<< m_changed_timestamp << "=> "<<m_timestamp<<std::endl;
				m_changed_timestamp = m_timestamp;
			}
		}
	}
*/

/*
	Get a quick string to describe what a block actually contains
*/
std::string analyze_block(MapBlock *block)
{
	if(block == NULL)
		return "NULL";

	auto lock = block->lock_shared_rec();
	std::ostringstream desc;

	v3s16 p = block->getPos();
	char spos[20];
	snprintf(spos, 20, "(%2d,%2d,%2d), ", p.X, p.Y, p.Z);
	desc<<spos;

	switch(block->getModified())
	{
	case MOD_STATE_CLEAN:
		desc<<"CLEAN,           ";
		break;
	case MOD_STATE_WRITE_AT_UNLOAD:
		desc<<"WRITE_AT_UNLOAD, ";
		break;
	case MOD_STATE_WRITE_NEEDED:
		desc<<"WRITE_NEEDED,    ";
		break;
	default:
		desc<<"unknown getModified()="+itos(block->getModified())+", ";
	}
	desc<<" changed_timestamp="<<block->m_changed_timestamp<<", ";
	if(block->isGenerated())
		desc<<"is_gen [X], ";
	else
		desc<<"is_gen [ ], ";

	if(block->getIsUnderground())
		desc<<"is_ug [X], ";
	else
		desc<<"is_ug [ ], ";

	if(block->getLightingExpired())
		desc<<"lighting_exp [X], ";
	else
		desc<<"lighting_exp [ ], ";

	if(block->isDummy())
	{
		desc<<"Dummy, ";
	}
	else
	{
		bool full_ignore = true;
		bool some_ignore = false;
		bool full_air = true;
		bool some_air = false;
		for(s16 z0=0; z0<MAP_BLOCKSIZE; z0++)
		for(s16 y0=0; y0<MAP_BLOCKSIZE; y0++)
		for(s16 x0=0; x0<MAP_BLOCKSIZE; x0++)
		{
			v3s16 p(x0,y0,z0);
			MapNode n = block->getNodeNoEx(p);
			content_t c = n.getContent();
			if(c == CONTENT_IGNORE)
				some_ignore = true;
			else
				full_ignore = false;
			if(c == CONTENT_AIR)
				some_air = true;
			else
				full_air = false;
		}

		desc<<"content {";

		std::ostringstream ss;

		if(full_ignore)
			ss<<"IGNORE (full), ";
		else if(some_ignore)
			ss<<"IGNORE, ";

		if(full_air)
			ss<<"AIR (full), ";
		else if(some_air)
			ss<<"AIR, ";

		if(ss.str().size()>=2)
			desc<<ss.str().substr(0, ss.str().size()-2);

		desc<<"}, ";
	}
	
	//desc<<" modifiedBy="<<block->getModifiedReason()<<"; "; // only with raiseModified(..., string)

	return desc.str().substr(0, desc.str().size()-2);
}


//END
