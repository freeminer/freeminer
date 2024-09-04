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
#include "util/directiontables.h"
#include "util/timetaker.h"
#include <cstring>  // memcpy, memset

/*
	Debug stuff
*/
u64 addarea_time = 0;
u64 emerge_time = 0;
u64 emerge_load_time = 0;
u64 clearflag_time = 0;

VoxelManipulator::~VoxelManipulator()
{
	clear();
}

void VoxelManipulator::clear()
{
	// Reset area to volume=0
	m_area = VoxelArea();
   if(m_data)
	delete[] m_data;
	m_data = nullptr;
   if (m_flags)
	delete[] m_flags;
	m_flags = nullptr;
}

void VoxelManipulator::print(std::ostream &o, const NodeDefManager *ndef,
	VoxelPrintMode mode)
{
	const v3pos_t &em = m_area.getExtent();
	v3pos_t of = m_area.MinEdge;
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
						else if(!ndef->get(m).light_propagates)
							c = 'X';
						else
						{
							u8 light = n.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(n));
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
	MapNode *new_data = new MapNode[new_size];
	//MapNode *new_data = reinterpret_cast<MapNode*>( ::operator new(new_size * sizeof(MapNode)));
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
		delete[] old_data;
	if(old_flags)
		delete[] old_flags;

	//dstream<<"addArea done"<<std::endl;
}

void VoxelManipulator::copyFrom(MapNode *src, const VoxelArea& src_area,
		v3pos_t from_pos, v3pos_t to_pos, const v3pos_t &size)
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
		v3pos_t dst_pos, v3pos_t from_pos, const v3pos_t &size)
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
	//TimeTaker timer("clearFlag", &clearflag_time);

	//v3pos_t s = m_area.getExtent();

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

const MapNode VoxelManipulator::ContentIgnoreNode = MapNode(CONTENT_IGNORE);

//END
