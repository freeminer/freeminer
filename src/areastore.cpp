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

#include "areastore.h"
#include "util/serialize.h"
#include "util/container.h"

#if USE_SPATIAL
	#include <spatialindex/SpatialIndex.h>
	#include <spatialindex/RTree.h>
	#include <spatialindex/Point.h>
#endif

#define AST_SMALLER_EQ_AS(p, q) (((p).X <= (q).X) && ((p).Y <= (q).Y) && ((p).Z <= (q).Z))

#define AST_OVERLAPS_IN_DIMENSION(amine, amaxe, b, d) \
	(!(((amine).d > (b)->maxedge.d) || ((amaxe).d < (b)->minedge.d)))

#define AST_CONTAINS_PT(a, p) (AST_SMALLER_EQ_AS((a)->minedge, (p)) && \
	AST_SMALLER_EQ_AS((p), (a)->maxedge))

#define AST_CONTAINS_AREA(amine, amaxe, b)         \
	(AST_SMALLER_EQ_AS((amine), (b)->minedge) \
	&& AST_SMALLER_EQ_AS((b)->maxedge, (amaxe)))

#define AST_AREAS_OVERLAP(amine, amaxe, b)                \
	(AST_OVERLAPS_IN_DIMENSION((amine), (amaxe), (b), X) && \
	AST_OVERLAPS_IN_DIMENSION((amine), (amaxe), (b), Y) &&  \
	AST_OVERLAPS_IN_DIMENSION((amine), (amaxe), (b), Z))

u16 AreaStore::size() const
{
	return areas_map.size();
}

u32 AreaStore::getFreeId(v3s16 minedge, v3s16 maxedge)
{
	int keep_on = 100;
	while (keep_on--) {
		m_highest_id++;
		// Handle overflows, we dont want to return 0
		if (m_highest_id == AREA_ID_INVALID)
			m_highest_id++;
		if (areas_map.find(m_highest_id) == areas_map.end())
			return m_highest_id;
	}
	// search failed
	return AREA_ID_INVALID;
}

const Area *AreaStore::getArea(u32 id) const
{
	const Area *res = NULL;
	std::map<u32, Area>::const_iterator itr = areas_map.find(id);
	if (itr != areas_map.end()) {
		res = &itr->second;
	}
	return res;
}

#if 0
Currently, serialisation is commented out. This is because of multiple reasons:
1. Why do we store the areastore into a file, why not into the database?
2. We don't use libspatial's serialisation, but we should, or perhaps not, because
	it would remove the ability to switch. Perhaps write migration routines?
3. Various things need fixing, e.g. the size is serialized as
	c++ implementation defined size_t
bool AreaStore::deserialize(std::istream &is)
{
	u8 ver = readU8(is);
	if (ver != 1)
		return false;
	u16 count_areas = readU16(is);
	for (u16 i = 0; i < count_areas; i++) {
		// deserialize an area
		Area a;
		a.id = readU32(is);
		a.minedge = readV3S16(is);
		a.maxedge = readV3S16(is);
		a.datalen = readU16(is);
		a.data = new char[a.datalen];
		is.read((char *) a.data, a.datalen);
		insertArea(a);
	}
	return true;
}


static bool serialize_area(void *ostr, Area *a)
{
	std::ostream &os = *((std::ostream *) ostr);
	writeU32(os, a->id);
	writeV3S16(os, a->minedge);
	writeV3S16(os, a->maxedge);
	writeU16(os, a->datalen);
	os.write(a->data, a->datalen);

	return false;
}


void AreaStore::serialize(std::ostream &os) const
{
	// write initial data
	writeU8(os, 1); // serialisation version
	writeU16(os, areas_map.size()); //DANGER: not platform independent
	forEach(&serialize_area, &os);
}

#endif

void AreaStore::invalidateCache()
{
	if (cache_enabled) {
		m_res_cache.invalidate();
	}
}

void AreaStore::setCacheParams(bool enabled, u8 block_radius, size_t limit)
{
	cache_enabled = enabled;
	m_cacheblock_radius = MYMAX(block_radius, 16);
	m_res_cache.setLimit(MYMAX(limit, 20));
	invalidateCache();
}

void AreaStore::cacheMiss(void *data, const v3s16 &mpos, std::vector<Area *> *dest)
{
	AreaStore *as = (AreaStore *)data;
	u8 r = as->m_cacheblock_radius;

	// get the points at the edges of the mapblock
	v3s16 minedge(mpos.X * r, mpos.Y * r, mpos.Z * r);
	v3s16 maxedge(
		minedge.X + r - 1,
		minedge.Y + r - 1,
		minedge.Z + r - 1);

	as->getAreasInArea(dest, minedge, maxedge, true);

	/* infostream << "Cache miss with " << dest->size() << " areas, between ("
			<< minedge.X << ", " << minedge.Y << ", " << minedge.Z
			<< ") and ("
			<< maxedge.X << ", " << maxedge.Y << ", " << maxedge.Z
			<< ")" << std::endl; // */
}

void AreaStore::getAreasForPos(std::vector<Area *> *result, v3s16 pos)
{
	if (cache_enabled) {
		v3s16 mblock = getContainerPos(pos, m_cacheblock_radius);
		const std::vector<Area *> *pre_list = m_res_cache.lookupCache(mblock);

		size_t s_p_l = pre_list->size();
		for (size_t i = 0; i < s_p_l; i++) {
			Area *b = (*pre_list)[i];
			if (AST_CONTAINS_PT(b, pos)) {
				result->push_back(b);
			}
		}
	} else {
		return getAreasForPosImpl(result, pos);
	}
}


////
// VectorAreaStore
////


void VectorAreaStore::insertArea(const Area &a)
{
	areas_map[a.id] = a;
	m_areas.push_back(&(areas_map[a.id]));
	invalidateCache();
}

void VectorAreaStore::reserve(size_t count)
{
	m_areas.reserve(count);
}

bool VectorAreaStore::removeArea(u32 id)
{
	std::map<u32, Area>::iterator itr = areas_map.find(id);
	if (itr != areas_map.end()) {
		size_t msiz = m_areas.size();
		for (size_t i = 0; i < msiz; i++) {
			Area * b = m_areas[i];
			if (b->id == id) {
				areas_map.erase(itr);
				m_areas.erase(m_areas.begin() + i);
				invalidateCache();
				return true;
			}
		}
		// we should never get here, it means we did find it in map,
		// but not in the vector
	}
	return false;
}

void VectorAreaStore::getAreasForPosImpl(std::vector<Area *> *result, v3s16 pos)
{
	size_t msiz = m_areas.size();
	for (size_t i = 0; i < msiz; i++) {
		Area *b = m_areas[i];
		if (AST_CONTAINS_PT(b, pos)) {
			result->push_back(b);
		}
	}
}

void VectorAreaStore::getAreasInArea(std::vector<Area *> *result,
		v3s16 minedge, v3s16 maxedge, bool accept_overlap)
{
	size_t msiz = m_areas.size();
	for (size_t i = 0; i < msiz; i++) {
		Area * b = m_areas[i];
		if (accept_overlap ? AST_AREAS_OVERLAP(minedge, maxedge, b) :
				AST_CONTAINS_AREA(minedge, maxedge, b)) {
			result->push_back(b);
		}
	}
}

#if 0
bool VectorAreaStore::forEach(bool (*callback)(void *args, Area *a), void *args) const
{
	size_t msiz = m_areas.size();
	for (size_t i = 0; i < msiz; i++) {
		if (callback(args, m_areas[i])) {
			return true;
		}
	}
	return false;
}
#endif

#if USE_SPATIAL

static inline SpatialIndex::Region get_spatial_region(const v3s16 minedge,
		const v3s16 maxedge)
{
	const double p_low[] = {(double)minedge.X,
		(double)minedge.Y, (double)minedge.Z};
	const double p_high[] = {(double)maxedge.X, (double)maxedge.Y,
		(double)maxedge.Z};
	return SpatialIndex::Region(p_low, p_high, 3);
}

static inline SpatialIndex::Point get_spatial_point(const v3s16 pos)
{
	const double p[] = {(double)pos.X, (double)pos.Y, (double)pos.Z};
	return SpatialIndex::Point(p, 3);
}


void SpatialAreaStore::insertArea(const Area &a)
{
	areas_map[a.id] = a;
	m_tree->insertData(0, NULL, get_spatial_region(a.minedge, a.maxedge), a.id);
	invalidateCache();
}

bool SpatialAreaStore::removeArea(u32 id)
{
	std::map<u32, Area>::iterator itr = areas_map.find(id);
	if (itr != areas_map.end()) {
		Area *a = &itr->second;
		bool result = m_tree->deleteData(get_spatial_region(a->minedge,
			a->maxedge), id);
		invalidateCache();
		return result;
	} else {
		return false;
	}
}

void SpatialAreaStore::getAreasForPosImpl(std::vector<Area *> *result, v3s16 pos)
{
	VectorResultVisitor visitor(result, this);
	m_tree->pointLocationQuery(get_spatial_point(pos), visitor);
}

void SpatialAreaStore::getAreasInArea(std::vector<Area *> *result,
		v3s16 minedge, v3s16 maxedge, bool accept_overlap)
{
	VectorResultVisitor visitor(result, this);
	if (accept_overlap) {
		m_tree->intersectsWithQuery(get_spatial_region(minedge, maxedge),
			visitor);
	} else {
		m_tree->containsWhatQuery(get_spatial_region(minedge, maxedge), visitor);
	}
}

#if 0
bool SpatialAreaStore::forEach(bool (*callback)(void *args, Area *a), void *args) const
{
	// TODO ?? (this is only needed for serialisation, but libspatial has its own serialisation)
	return false;
}
#endif

SpatialAreaStore::~SpatialAreaStore()
{
	delete m_tree;
}

SpatialAreaStore::SpatialAreaStore()
{
	m_storagemanager =
		SpatialIndex::StorageManager::createNewMemoryStorageManager();
	SpatialIndex::id_type id;
	m_tree = SpatialIndex::RTree::createNewRTree(
		*m_storagemanager,
		.7, // Fill factor
		100, // Index capacity
		100, // Leaf capacity
		3, // dimension :)
		SpatialIndex::RTree::RV_RSTAR,
		id);
}

#endif
