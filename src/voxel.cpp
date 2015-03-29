/*
voxel.cpp
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

#include "voxel.h"
#include "map.h"
#include "gettime.h"
#include "nodedef.h"
#include "util/timetaker.h"
#include <string.h>  // memcpy, memset

/*
	Debug stuff
*/
u32 clearflag_time = 0;
//u32 getwaterpressure_time = 0;
//u32 spreadwaterpressure_time = 0;
u32 updateareawaterpressure_time = 0;
u32 flowwater_pre_time = 0;


VoxelManipulator::VoxelManipulator():
	m_data(NULL),
	m_flags(NULL)
{
}

VoxelManipulator::~VoxelManipulator()
{
	clear();
}

void VoxelManipulator::clear()
{
	// Reset area to volume=0
	m_area = VoxelArea();
	if(m_data)
		delete m_data;
	m_data = NULL;
	if(m_flags)
	delete[] m_flags;
	m_flags = NULL;
}

void VoxelManipulator::print(std::ostream &o, INodeDefManager *ndef,
		VoxelPrintMode mode)
{
	v3s16 em = m_area.getExtent();
	v3s16 of = m_area.MinEdge;
	o<<"size: "<<em.X<<"x"<<em.Y<<"x"<<em.Z
	 <<" offset: ("<<of.X<<","<<of.Y<<","<<of.Z<<")"<<std::endl;

	for(s32 y=m_area.MaxEdge.Y; y>=m_area.MinEdge.Y; y--)
	{
		if(em.X >= 3 && em.Y >= 3)
		{
			if     (y==m_area.MinEdge.Y+2) o<<"^     ";
			else if(y==m_area.MinEdge.Y+1) o<<"|     ";
			else if(y==m_area.MinEdge.Y+0) o<<"y x-> ";
			else                           o<<"      ";
		}

		for(s32 z=m_area.MinEdge.Z; z<=m_area.MaxEdge.Z; z++)
		{
			for(s32 x=m_area.MinEdge.X; x<=m_area.MaxEdge.X; x++)
			{
				u8 f = m_flags[m_area.index(x,y,z)];
				char c;
				if(f & VOXELFLAG_NO_DATA)
					c = 'N';
				else
				{
					c = 'X';
					MapNode n = m_data[m_area.index(x,y,z)];
					content_t m = n.getContent();
					u8 pr = n.param2;
					if(mode == VOXELPRINT_MATERIAL)
					{
						if(m <= 9)
							c = m + '0';
					}
					else if(mode == VOXELPRINT_WATERPRESSURE)
					{
						if(ndef->get(m).isLiquid())
						{
							c = 'w';
							if(pr <= 9)
								c = pr + '0';
						}
						else if(m == CONTENT_AIR)
						{
							c = ' ';
						}
						else
						{
							c = '#';
						}
					}
					else if(mode == VOXELPRINT_LIGHT_DAY)
					{
						if(ndef->get(m).light_source != 0)
							c = 'S';
						else if(ndef->get(m).light_propagates == false)
							c = 'X';
						else
						{
							u8 light = n.getLight(LIGHTBANK_DAY, ndef);
							if(light < 10)
								c = '0' + light;
							else
								c = 'a' + (light-10);
						}
					}
				}
				o<<c;
			}
			o<<' ';
		}
		o<<std::endl;
	}
}

void VoxelManipulator::addArea(const VoxelArea &area)
{
	// Cancel if requested area has zero volume
	if (area.hasEmptyExtent())
		return;

	// Cancel if m_area already contains the requested area
	if(m_area.contains(area))
		return;

	TimeTaker timer("addArea");

	// Calculate new area
	VoxelArea new_area;
	// New area is the requested area if m_area has zero volume
	if(m_area.hasEmptyExtent())
	{
		new_area = area;
	}
	// Else add requested area to m_area
	else
	{
		new_area = m_area;
		new_area.addArea(area);
	}

	s32 new_size = new_area.getVolume();

	/*dstream<<"adding area ";
	area.print(dstream);
	dstream<<", old area ";
	m_area.print(dstream);
	dstream<<", new area ";
	new_area.print(dstream);
	dstream<<", new_size="<<new_size;
	dstream<<std::endl;*/

	// Allocate new data and clear flags
	MapNode *new_data = reinterpret_cast<MapNode*>( ::operator new(new_size * sizeof(MapNode)));
	if (!CONTENT_IGNORE)
		memset(new_data, 0, new_size * sizeof(MapNode));
	else
		for(s32 i=0; i<new_size; i++)
			new_data[i] = MapNode(CONTENT_IGNORE);

	u8 *new_flags = new u8[new_size];
	memset(new_flags, VOXELFLAG_NO_DATA, new_size);

	// Copy old data
	s32 old_x_width = m_area.MaxEdge.X - m_area.MinEdge.X + 1;
	for(s32 z=m_area.MinEdge.Z; z<=m_area.MaxEdge.Z; z++)
	for(s32 y=m_area.MinEdge.Y; y<=m_area.MaxEdge.Y; y++)
	{
		unsigned int old_index = m_area.index(m_area.MinEdge.X,y,z);
		unsigned int new_index = new_area.index(m_area.MinEdge.X,y,z);

		memcpy(&new_data[new_index], &m_data[old_index],
				old_x_width * sizeof(MapNode));
		memcpy(&new_flags[new_index], &m_flags[old_index],
				old_x_width * sizeof(u8));
	}

	// Replace area, data and flags

	m_area = new_area;

	MapNode *old_data = m_data;
	u8 *old_flags = m_flags;

	/*dstream<<"old_data="<<(int)old_data<<", new_data="<<(int)new_data
	<<", old_flags="<<(int)m_flags<<", new_flags="<<(int)new_flags<<std::endl;*/

	m_data = new_data;
	m_flags = new_flags;

	if(old_data)
		delete old_data;
	if(old_flags)
		delete[] old_flags;

	//dstream<<"addArea done"<<std::endl;
}

void VoxelManipulator::copyFrom(MapNode *src, const VoxelArea& src_area,
		v3s16 from_pos, v3s16 to_pos, v3s16 size)
{
	/* The reason for this optimised code is that we're a member function
	 * and the data type/layout of m_data is know to us: it's stored as
	 * [z*h*w + y*h + x]. Therefore we can take the calls to m_area index
	 * (which performs the preceding mapping/indexing of m_data) out of the
	 * inner loop and calculate the next index as we're iterating to gain
	 * performance.
	 *
	 * src_step and dest_step is the amount required to be added to our index
	 * every time y increments. Because the destination area may be larger
	 * than the source area we need one additional variable (otherwise we could
	 * just continue adding dest_step as is done for the source data): dest_mod.
	 * dest_mod is the difference in size between a "row" in the source data
	 * and a "row" in the destination data (I am using the term row loosely
	 * and for illustrative purposes). E.g.
	 *
	 * src       <-------------------->|'''''' dest mod ''''''''
	 * dest      <--------------------------------------------->
	 *
	 * dest_mod (it's essentially a modulus) is added to the destination index
	 * after every full iteration of the y span.
	 *
	 * This method falls under the category "linear array and incrementing
	 * index".
	 */

	s32 src_step = src_area.getExtent().X;
	s32 dest_step = m_area.getExtent().X;
	s32 dest_mod = m_area.index(to_pos.X, to_pos.Y, to_pos.Z + 1)
			- m_area.index(to_pos.X, to_pos.Y, to_pos.Z)
			- dest_step * size.Y;

	s32 i_src = src_area.index(from_pos.X, from_pos.Y, from_pos.Z);
	s32 i_local = m_area.index(to_pos.X, to_pos.Y, to_pos.Z);

	for (s16 z = 0; z < size.Z; z++) {
		for (s16 y = 0; y < size.Y; y++) {
			memcpy(&m_data[i_local], &src[i_src], size.X * sizeof(*m_data));
			memset(&m_flags[i_local], 0, size.X);
			i_src += src_step;
			i_local += dest_step;
		}
		i_local += dest_mod;
	}
}

void VoxelManipulator::copyTo(MapNode *dst, const VoxelArea& dst_area,
		v3s16 dst_pos, v3s16 from_pos, v3s16 size)
{
	for(s16 z=0; z<size.Z; z++)
	for(s16 y=0; y<size.Y; y++)
	{
		s32 i_dst = dst_area.index(dst_pos.X, dst_pos.Y+y, dst_pos.Z+z);
		s32 i_local = m_area.index(from_pos.X, from_pos.Y+y, from_pos.Z+z);
		for (s16 x = 0; x < size.X; x++) {
			if (m_data[i_local].getContent() != CONTENT_IGNORE)
				dst[i_dst] = m_data[i_local];
			i_dst++;
			i_local++;
		}
	}
}

/*
	Algorithms
	-----------------------------------------------------
*/

void VoxelManipulator::clearFlag(u8 flags)
{
	// 0-1ms on moderate area
	TimeTaker timer("clearFlag", &clearflag_time);

	//v3s16 s = m_area.getExtent();

	/*dstream<<"clearFlag clearing area of size "
			<<""<<s.X<<"x"<<s.Y<<"x"<<s.Z<<""
			<<std::endl;*/

	//s32 count = 0;

	/*for(s32 z=m_area.MinEdge.Z; z<=m_area.MaxEdge.Z; z++)
	for(s32 y=m_area.MinEdge.Y; y<=m_area.MaxEdge.Y; y++)
	for(s32 x=m_area.MinEdge.X; x<=m_area.MaxEdge.X; x++)
	{
		u8 f = m_flags[m_area.index(x,y,z)];
		m_flags[m_area.index(x,y,z)] &= ~flags;
		if(m_flags[m_area.index(x,y,z)] != f)
			count++;
	}*/

	s32 volume = m_area.getVolume();
	for(s32 i=0; i<volume; i++)
	{
		m_flags[i] &= ~flags;
	}

	/*s32 volume = m_area.getVolume();
	for(s32 i=0; i<volume; i++)
	{
		u8 f = m_flags[i];
		m_flags[i] &= ~flags;
		if(m_flags[i] != f)
			count++;
	}

	dstream<<"clearFlag changed "<<count<<" flags out of "
			<<volume<<" nodes"<<std::endl;*/
}

void VoxelManipulator::unspreadLight(enum LightBank bank, v3s16 p, u8 oldlight,
		std::set<v3s16> & light_sources, INodeDefManager *nodemgr)
{
	v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	VoxelArea voxel_area(p - v3s16(1,1,1), p + v3s16(1,1,1));
	addArea(voxel_area);

	// Loop through 6 neighbors
	for(u16 i=0; i<6; i++)
	{
		// Get the position of the neighbor node
		v3s16 n2pos = p + dirs[i];

		u32 n2i = m_area.index(n2pos);

		if(m_flags[n2i] & VOXELFLAG_NO_DATA)
			continue;

		MapNode &n2 = m_data[n2i];

		/*
			If the neighbor is dimmer than what was specified
			as oldlight (the light of the previous node)
		*/
		u8 light2 = n2.getLight(bank, nodemgr);
		if(light2 < oldlight)
		{
			/*
				And the neighbor is transparent and it has some light
			*/
			if(nodemgr->get(n2).light_propagates && light2 != 0)
			{
				/*
					Set light to 0 and add to queue
				*/

				n2.setLight(bank, 0, nodemgr);

				unspreadLight(bank, n2pos, light2, light_sources, nodemgr);

				/*
					Remove from light_sources if it is there
					NOTE: This doesn't happen nearly at all
				*/
				/*if(light_sources.find(n2pos))
				{
					std::cout<<"Removed from light_sources"<<std::endl;
					light_sources.remove(n2pos);
				}*/
			}
		}
		else{
			light_sources.insert(n2pos);
		}
	}
}

#if 1
/*
	Goes recursively through the neighbours of the node.

	Alters only transparent nodes.

	If the lighting of the neighbour is lower than the lighting of
	the node was (before changing it to 0 at the step before), the
	lighting of the neighbour is set to 0 and then the same stuff
	repeats for the neighbour.

	The ending nodes of the routine are stored in light_sources.
	This is useful when a light is removed. In such case, this
	routine can be called for the light node and then again for
	light_sources to re-light the area without the removed light.

	values of from_nodes are lighting values.
*/
void VoxelManipulator::unspreadLight(enum LightBank bank,
		std::map<v3s16, u8> & from_nodes,
		std::set<v3s16> & light_sources, INodeDefManager *nodemgr)
{
	if(from_nodes.empty())
		return;

	for(std::map<v3s16, u8>::iterator j = from_nodes.begin();
		j != from_nodes.end(); ++j)
	{
		unspreadLight(bank, j->first, j->second, light_sources, nodemgr);
	}
}
#endif

#if 0
/*
	Goes recursively through the neighbours of the node.

	Alters only transparent nodes.

	If the lighting of the neighbour is lower than the lighting of
	the node was (before changing it to 0 at the step before), the
	lighting of the neighbour is set to 0 and then the same stuff
	repeats for the neighbour.

	The ending nodes of the routine are stored in light_sources.
	This is useful when a light is removed. In such case, this
	routine can be called for the light node and then again for
	light_sources to re-light the area without the removed light.

	values of from_nodes are lighting values.
*/
void VoxelManipulator::unspreadLight(enum LightBank bank,
		core::map<v3s16, u8> & from_nodes,
		core::map<v3s16, bool> & light_sources)
{
	v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	if(from_nodes.size() == 0)
		return;

	core::map<v3s16, u8> unlighted_nodes;
	core::map<v3s16, u8>::Iterator j;
	j = from_nodes.getIterator();

	for(; j.atEnd() == false; j++)
	{
		v3s16 pos = j.getNode()->getKey();

		addArea(VoxelArea(pos - v3s16(1,1,1), pos + v3s16(1,1,1)));

		//MapNode &n = m_data[m_area.index(pos)];

		u8 oldlight = j.getNode()->getValue();

		// Loop through 6 neighbors
		for(u16 i=0; i<6; i++)
		{
			// Get the position of the neighbor node
			v3s16 n2pos = pos + dirs[i];

			u32 n2i = m_area.index(n2pos);

			if(m_flags[n2i] & VOXELFLAG_NO_DATA)
				continue;

			MapNode &n2 = m_data[n2i];

			/*
				If the neighbor is dimmer than what was specified
				as oldlight (the light of the previous node)
			*/
			if(n2.getLight(bank, nodemgr) < oldlight)
			{
				/*
					And the neighbor is transparent and it has some light
				*/
				if(nodemgr->get(n2).light_propagates && n2.getLight(bank, nodemgr) != 0)
				{
					/*
						Set light to 0 and add to queue
					*/

					u8 current_light = n2.getLight(bank, nodemgr);
					n2.setLight(bank, 0);

					unlighted_nodes.insert(n2pos, current_light);

					/*
						Remove from light_sources if it is there
						NOTE: This doesn't happen nearly at all
					*/
					/*if(light_sources.find(n2pos))
					{
						std::cout<<"Removed from light_sources"<<std::endl;
						light_sources.remove(n2pos);
					}*/
				}
			}
			else{
				light_sources.insert(n2pos, true);
			}
		}
	}

	/*dstream<<"unspreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if(unlighted_nodes.size() > 0)
		unspreadLight(bank, unlighted_nodes, light_sources);
}
#endif

void VoxelManipulator::spreadLight(enum LightBank bank, v3s16 p,
		INodeDefManager *nodemgr)
{
	const v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	VoxelArea voxel_area(p - v3s16(1,1,1), p + v3s16(1,1,1));
	addArea(voxel_area);

	u32 i = m_area.index(p);

	if(m_flags[i] & VOXELFLAG_NO_DATA)
		return;

	MapNode &n = m_data[i];

	u8 oldlight = n.getLight(bank, nodemgr);
	u8 newlight = diminish_light(oldlight);

	// Loop through 6 neighbors
	for(u16 i=0; i<6; i++)
	{
		// Get the position of the neighbor node
		v3s16 n2pos = p + dirs[i];

		u32 n2i = m_area.index(n2pos);

		if(m_flags[n2i] & VOXELFLAG_NO_DATA)
			continue;

		MapNode &n2 = m_data[n2i];

		u8 light2 = n2.getLight(bank, nodemgr);

		/*
			If the neighbor is brighter than the current node,
			add to list (it will light up this node on its turn)
		*/
		if(light2 > undiminish_light(oldlight))
		{
			spreadLight(bank, n2pos, nodemgr);
		}
		/*
			If the neighbor is dimmer than how much light this node
			would spread on it, add to list
		*/
		if(light2 < newlight)
		{
			if(nodemgr->get(n2).light_propagates)
			{
				n2.setLight(bank, newlight, nodemgr);
				spreadLight(bank, n2pos, nodemgr);
			}
		}
	}
}

#if 0
/*
	Lights neighbors of from_nodes, collects all them and then
	goes on recursively.

	NOTE: This is faster on small areas but will overflow the
	      stack on large areas. Thus it is not used.
*/
void VoxelManipulator::spreadLight(enum LightBank bank,
		core::map<v3s16, bool> & from_nodes)
{
	if(from_nodes.size() == 0)
		return;

	core::map<v3s16, bool> lighted_nodes;
	core::map<v3s16, bool>::Iterator j;
	j = from_nodes.getIterator();

	for(; j.atEnd() == false; j++)
	{
		v3s16 pos = j.getNode()->getKey();

		spreadLight(bank, pos);
	}
}
#endif

const MapNode VoxelManipulator::ContentIgnoreNode = MapNode(CONTENT_IGNORE);

#if 1
/*
	Lights neighbors of from_nodes, collects all them and then
	goes on recursively.
*/
void VoxelManipulator::spreadLight(enum LightBank bank,
		std::set<v3s16> & from_nodes, INodeDefManager *nodemgr)
{
	const v3s16 dirs[6] = {
		v3s16(0,0,1), // back
		v3s16(0,1,0), // top
		v3s16(1,0,0), // right
		v3s16(0,0,-1), // front
		v3s16(0,-1,0), // bottom
		v3s16(-1,0,0), // left
	};

	if(from_nodes.empty())
		return;

	std::set<v3s16> lighted_nodes;

	for(std::set<v3s16>::iterator j = from_nodes.begin();
		j != from_nodes.end(); ++j)
	{
		v3s16 pos = *j;

		VoxelArea voxel_area(pos - v3s16(1,1,1), pos + v3s16(1,1,1));
		addArea(voxel_area);

		u32 i = m_area.index(pos);

		if(m_flags[i] & VOXELFLAG_NO_DATA)
			continue;

		MapNode &n = m_data[i];

		u8 oldlight = n.getLight(bank, nodemgr);
		u8 newlight = diminish_light(oldlight);

		// Loop through 6 neighbors
		for(u16 i=0; i<6; i++)
		{
			// Get the position of the neighbor node
			v3s16 n2pos = pos + dirs[i];

			try
			{
				u32 n2i = m_area.index(n2pos);

				if(m_flags[n2i] & VOXELFLAG_NO_DATA)
					continue;

				MapNode &n2 = m_data[n2i];

				u8 light2 = n2.getLight(bank, nodemgr);

				/*
					If the neighbor is brighter than the current node,
					add to list (it will light up this node on its turn)
				*/
				if(light2 > undiminish_light(oldlight))
				{
					lighted_nodes.insert(n2pos);
				}
				/*
					If the neighbor is dimmer than how much light this node
					would spread on it, add to list
				*/
				if(light2 < newlight)
				{
					if(nodemgr->get(n2).light_propagates)
					{
						n2.setLight(bank, newlight, nodemgr);
						lighted_nodes.insert(n2pos);
					}
				}
			}
			catch(InvalidPositionException &e)
			{
				continue;
			}
		}
	}

	/*dstream<<"spreadLight(): Changed block "
			<<blockchangecount<<" times"
			<<" for "<<from_nodes.size()<<" nodes"
			<<std::endl;*/

	if(!lighted_nodes.empty())
		spreadLight(bank, lighted_nodes, nodemgr);
}
#endif

//END
