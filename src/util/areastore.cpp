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

#include "util/areastore.h"
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


AreaStore *AreaStore::getOptimalImplementation()
{
#if USE_SPATIAL
	return new SpatialAreaStore();
#else
	return new VectorAreaStore();
#endif
}

const Area *AreaStore::getArea(u32 id) const
{
	AreaMap::const_iterator it = areas_map.find(id);
	if (it == areas_map.end())
		return NULL;
	return &it->second;
}

void AreaStore::serialize(std::ostream &os) const
{
	writeU8(os, 0); // Serialisation version

	// TODO: Compression?
	writeU16(os, areas_map.size());
	for (AreaMap::const_iterator it = areas_map.begin();
			it != areas_map.end(); ++it) {
		const Area &a = it->second;
		writeV3S16(os, a.minedge);
		writeV3S16(os, a.maxedge);
		writeU16(os, a.data.size());
		os.write(a.data.data(), a.data.size());
	}
}

void AreaStore::deserialize(std::istream &is)
{
	u8 ver = readU8(is);
	if (ver != 0)
		throw SerializationError("Unknown AreaStore "
				"serialization version!");

	u16 num_areas = readU16(is);
	for (u32 i = 0; i < num_areas; ++i) {
		Area a;
		a.minedge = readV3S16(is);
		a.maxedge = readV3S16(is);
		u16 data_len = readU16(is);
		char *data = new char[data_len];
		is.read(data, data_len);
		a.data = std::string(data, data_len);
		insertArea(&a);
	}
}

void AreaStore::invalidateCache()
{
	if (m_cache_enabled) {
		m_res_cache.invalidate();
	}
}

void AreaStore::setCacheParams(bool enabled, u8 block_radius, size_t limit)
{
	m_cache_enabled = enabled;
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
	if (m_cache_enabled) {
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


bool VectorAreaStore::insertArea(Area *a)
{
	if (a->id == U32_MAX)
		a->id = getNextId();
	std::pair<AreaMap::iterator, bool> res =
			areas_map.insert(std::make_pair(a->id, *a));
	if (!res.second)
		// ID is not unique
		return false;
	m_areas.push_back(&res.first->second);
	invalidateCache();
	return true;
}

bool VectorAreaStore::removeArea(u32 id)
{
	AreaMap::iterator it = areas_map.find(id);
	if (it == areas_map.end())
		return false;
	Area *a = &it->second;
	for (std::vector<Area *>::iterator v_it = m_areas.begin();
			v_it != m_areas.end(); ++v_it) {
		if (*v_it == a) {
			m_areas.erase(v_it);
			break;
		}
	}
	areas_map.erase(it);
	invalidateCache();
	return true;
}

void VectorAreaStore::getAreasForPosImpl(std::vector<Area *> *result, v3s16 pos)
{
	for (size_t i = 0; i < m_areas.size(); ++i) {
		Area *b = m_areas[i];
		if (AST_CONTAINS_PT(b, pos)) {
			result->push_back(b);
		}
	}
}

void VectorAreaStore::getAreasInArea(std::vector<Area *> *result,
		v3s16 minedge, v3s16 maxedge, bool accept_overlap)
{
	for (size_t i = 0; i < m_areas.size(); ++i) {
		Area *b = m_areas[i];
		if (accept_overlap ? AST_AREAS_OVERLAP(minedge, maxedge, b) :
				AST_CONTAINS_AREA(minedge, maxedge, b)) {
			result->push_back(b);
		}
	}
}

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


bool SpatialAreaStore::insertArea(Area *a)
{
	if (a->id == U32_MAX)
		a->id = getNextId();
	if (!areas_map.insert(std::make_pair(a->id, *a)).second)
		// ID is not unique
		return false;
	m_tree->insertData(0, NULL, get_spatial_region(a->minedge, a->maxedge), a->id);
	invalidateCache();
	return true;
}

bool SpatialAreaStore::removeArea(u32 id)
{
	std::map<u32, Area>::iterator itr = areas_map.find(id);
	if (itr != areas_map.end()) {
		Area *a = &itr->second;
		bool result = m_tree->deleteData(get_spatial_region(a->minedge,
			a->maxedge), id);
		areas_map.erase(itr);
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
