/*
Minetest
Copyright (C) 2015 est31 <mtest31@outlook.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef AREA_STORE_H_
#define AREA_STORE_H_

#include "irr_v3d.h"
#include "noise.h" // for PcgRandom
#include <map>
#include <list>
#include <vector>
#include <istream>
#include "util/container.h"
#include "util/numeric.h"
#ifndef ANDROID
	#include "cmake_config.h"
#endif
#if USE_SPATIAL
	#include <spatialindex/SpatialIndex.h>
	#include "util/serialize.h"
#endif


struct Area {
	Area() : id(U32_MAX) {}
	Area(const v3s16 &mine, const v3s16 &maxe) :
		id(U32_MAX), minedge(mine), maxedge(maxe)
	{
		sortBoxVerticies(minedge, maxedge);
	}

	u32 id;
	v3s16 minedge, maxedge;
	std::string data;
};


class AreaStore {
public:
	AreaStore() :
		m_cache_enabled(true),
		m_cacheblock_radius(64),
		m_res_cache(1000, &cacheMiss, this),
		m_next_id(0)
	{}

	virtual ~AreaStore() {}

	static AreaStore *getOptimalImplementation();

	virtual void reserve(size_t count) {};
	size_t size() const { return areas_map.size(); }

	/// Add an area to the store.
	/// Updates the area's ID if it hasn't already been set.
	/// @return Whether the area insertion was successful.
	virtual bool insertArea(Area *a) = 0;

	/// Removes an area from the store by ID.
	/// @return Whether the area was in the store and removed.
	virtual bool removeArea(u32 id) = 0;

	/// Finds areas that the passed position is contained in.
	/// Stores output in passed vector.
	void getAreasForPos(std::vector<Area *> *result, v3s16 pos);

	/// Finds areas that are completely contained inside the area defined
	/// by the passed edges.  If @p accept_overlap is true this finds any
	/// areas that intersect with the passed area at any point.
	virtual void getAreasInArea(std::vector<Area *> *result,
		v3s16 minedge, v3s16 maxedge, bool accept_overlap) = 0;

	/// Sets cache parameters.
	void setCacheParams(bool enabled, u8 block_radius, size_t limit);

	/// Returns a pointer to the area coresponding to the passed ID,
	/// or NULL if it doesn't exist.
	const Area *getArea(u32 id) const;

	/// Serializes the store's areas to a binary ostream.
	void serialize(std::ostream &is) const;

	/// Deserializes the Areas from a binary istream.
	/// This does not currently clear the AreaStore before adding the
	/// areas, making it possible to deserialize multiple serialized
	/// AreaStores.
	void deserialize(std::istream &is);

protected:
	/// Invalidates the getAreasForPos cache.
	/// Call after adding or removing an area.
	void invalidateCache();

	/// Implementation of getAreasForPos.
	/// getAreasForPos calls this if the cache is disabled.
	virtual void getAreasForPosImpl(std::vector<Area *> *result, v3s16 pos) = 0;

	/// Returns the next area ID and increments it.
	u32 getNextId() { return m_next_id++; }

	// Note: This can't be an unordered_map, since all
	// references would be invalidated on rehash.
	typedef std::map<u32, Area> AreaMap;
	AreaMap areas_map;

private:
	/// Called by the cache when a value isn't found in the cache.
	static void cacheMiss(void *data, const v3s16 &mpos, std::vector<Area *> *dest);

	bool m_cache_enabled;
	/// Range, in nodes, of the getAreasForPos cache.
	/// If you modify this, call invalidateCache()
	u8 m_cacheblock_radius;
	LRUCache<v3s16, std::vector<Area *> > m_res_cache;

	u32 m_next_id;
};


class VectorAreaStore : public AreaStore {
public:
	virtual void reserve(size_t count) { m_areas.reserve(count); }
	virtual bool insertArea(Area *a);
	virtual bool removeArea(u32 id);
	virtual void getAreasInArea(std::vector<Area *> *result,
		v3s16 minedge, v3s16 maxedge, bool accept_overlap);

protected:
	virtual void getAreasForPosImpl(std::vector<Area *> *result, v3s16 pos);

private:
	std::vector<Area *> m_areas;
};


#if USE_SPATIAL

class SpatialAreaStore : public AreaStore {
public:
	SpatialAreaStore();
	virtual ~SpatialAreaStore();

	virtual bool insertArea(Area *a);
	virtual bool removeArea(u32 id);
	virtual void getAreasInArea(std::vector<Area *> *result,
		v3s16 minedge, v3s16 maxedge, bool accept_overlap);

protected:
	virtual void getAreasForPosImpl(std::vector<Area *> *result, v3s16 pos);

private:
	SpatialIndex::ISpatialIndex *m_tree;
	SpatialIndex::IStorageManager *m_storagemanager;

	class VectorResultVisitor : public SpatialIndex::IVisitor {
	public:
		VectorResultVisitor(std::vector<Area *> *result, SpatialAreaStore *store) :
			m_store(store),
			m_result(result)
		{}
		~VectorResultVisitor() {}

		virtual void visitNode(const SpatialIndex::INode &in) {}

		virtual void visitData(const SpatialIndex::IData &in)
		{
			u32 id = in.getIdentifier();

			std::map<u32, Area>::iterator itr = m_store->areas_map.find(id);
			assert(itr != m_store->areas_map.end());
			m_result->push_back(&itr->second);
		}

		virtual void visitData(std::vector<const SpatialIndex::IData *> &v)
		{
			for (size_t i = 0; i < v.size(); i++)
				visitData(*(v[i]));
		}

	private:
		SpatialAreaStore *m_store;
		std::vector<Area *> *m_result;
	};
};

#endif // USE_SPATIAL

#endif // AREA_STORE_H_
