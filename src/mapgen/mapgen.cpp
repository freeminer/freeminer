/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2015-2018 paramat

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

#include <cmath>
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "mapgen.h"
#include "voxel.h"
#include "noise.h"
#include "gamedef.h"
#include "mg_biome.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "nodedef.h"
#include "emerge.h"
#include "voxelalgorithms.h"
#include "porting.h"
#include "profiler.h"
#include "settings.h"
#include "treegen.h"
#include "serialization.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/directiontables.h"
#include "filesys.h"
#include "log.h"
#include "mapgen_carpathian.h"
#include "mapgen_flat.h"
#include "mapgen_fractal.h"
#include "mapgen_v5.h"
#include "mapgen_v6.h"
#include "mapgen_v7.h"
#include "mapgen_valleys.h"
#include "mapgen_singlenode.h"
#include "cavegen.h"
#include "dungeongen.h"

// fm:
#include "log_types.h"
#include "mapgen_indev.h"
#include "mapgen_math.h"
#include "mapgen_earth.h"
#include "serverenvironment.h"


FlagDesc flagdesc_mapgen[] = {
	{"caves",       MG_CAVES},
	{"dungeons",    MG_DUNGEONS},
	{"light",       MG_LIGHT},
	{"decorations", MG_DECORATIONS},
	{"biomes",      MG_BIOMES},
	{"ores",        MG_ORES},
	{NULL,          0}
};

FlagDesc flagdesc_gennotify[] = {
	{"dungeon",          1 << GENNOTIFY_DUNGEON},
	{"temple",           1 << GENNOTIFY_TEMPLE},
	{"cave_begin",       1 << GENNOTIFY_CAVE_BEGIN},
	{"cave_end",         1 << GENNOTIFY_CAVE_END},
	{"large_cave_begin", 1 << GENNOTIFY_LARGECAVE_BEGIN},
	{"large_cave_end",   1 << GENNOTIFY_LARGECAVE_END},
	{"decoration",       1 << GENNOTIFY_DECORATION},
	{NULL,               0}
};

struct MapgenDesc {
	const char *name;
	bool is_user_visible;
};

////
//// Built-in mapgens
////

// Order used here defines the order of appearance in mainmenu.
// v6 always last to discourage selection.
// Special mapgens flat, fractal, singlenode, next to last. Of these, singlenode
// last to discourage selection.
// Of the remaining, v5 last due to age, v7 first due to being the default.
// The order of 'enum MapgenType' in mapgen.h must match this order.
static MapgenDesc g_reg_mapgens[] = {
	{"v7",         true},
	{"valleys",    true},
	{"carpathian", true},
	{"v5",         true},
	{"flat",       true},
	{"fractal",    true},
	{"singlenode", true},
	{"v6",         true},

	{"indev",      true},
	{"math",       true},
	{"earth",       true},
};

static_assert(
	ARRLEN(g_reg_mapgens) == MAPGEN_INVALID,
	"g_reg_mapgens is wrong size");

////
//// Mapgen
////

Mapgen::Mapgen(int mapgenid, MapgenParams *params, EmergeParams *emerge) :
	gennotify(emerge->gen_notify_on, emerge->gen_notify_on_deco_ids)
{
	id           = mapgenid;
	water_level  = params->water_level;
	mapgen_limit = params->mapgen_limit;
	flags        = params->flags;
	csize        = v3pos_t(1, 1, 1) * (params->chunksize * MAP_BLOCKSIZE);

	// freeminer:
	env          = emerge->env;
	liquid_pressure = params->liquid_pressure;

	/*
		We are losing half our entropy by doing this, but it is necessary to
		preserve reverse compatibility.  If the top half of our current 64 bit
		seeds ever starts getting used, existing worlds will break due to a
		different hash outcome and no way to differentiate between versions.

		A solution could be to add a new bit to designate that the top half of
		the seed value should be used, essentially a 1-bit version code, but
		this would require increasing the total size of a seed to 9 bytes (yuck)

		It's probably okay if this never gets fixed.  4.2 billion possibilities
		ought to be enough for anyone.
	*/
	seed = (s32)params->seed;

	m_emerge  = emerge;
	ndef      = emerge->ndef;
}

Mapgen::~Mapgen()
{
	delete m_emerge; // this is our responsibility
}


MapgenType Mapgen::getMapgenType(const std::string &mgname)
{
	for (size_t i = 0; i != ARRLEN(g_reg_mapgens); i++) {
		if (mgname == g_reg_mapgens[i].name)
			return (MapgenType)i;
	}

	return MAPGEN_INVALID;
}


const char *Mapgen::getMapgenName(MapgenType mgtype)
{
	size_t index = (size_t)mgtype;
	if (index == MAPGEN_INVALID || index >= ARRLEN(g_reg_mapgens))
		return "invalid";

	return g_reg_mapgens[index].name;
}


Mapgen *Mapgen::createMapgen(MapgenType mgtype, MapgenParams *params,
	EmergeParams *emerge)
{
	switch (mgtype) {

	case MAPGEN_INDEV:
		return new MapgenIndev((MapgenIndevParams *)params, emerge);
	case MAPGEN_MATH:
		return new MapgenMath((MapgenMathParams *)params, emerge);
	case MAPGEN_EARTH:
		return new MapgenEarth((MapgenEarthParams *)params, emerge);

	case MAPGEN_CARPATHIAN:
		return new MapgenCarpathian((MapgenCarpathianParams *)params, emerge);
	case MAPGEN_FLAT:
		return new MapgenFlat((MapgenFlatParams *)params, emerge);
	case MAPGEN_FRACTAL:
		return new MapgenFractal((MapgenFractalParams *)params, emerge);
	case MAPGEN_SINGLENODE:
		return new MapgenSinglenode((MapgenSinglenodeParams *)params, emerge);
	case MAPGEN_V5:
		return new MapgenV5((MapgenV5Params *)params, emerge);
	case MAPGEN_V6:
		return new MapgenV6((MapgenV6Params *)params, emerge);
	case MAPGEN_V7:
		return new MapgenV7((MapgenV7Params *)params, emerge);
	case MAPGEN_VALLEYS:
		return new MapgenValleys((MapgenValleysParams *)params, emerge);
	default:
		return nullptr;
	}
}


MapgenParams *Mapgen::createMapgenParams(MapgenType mgtype)
{
	switch (mgtype) {

	case MAPGEN_INDEV:
		return new MapgenIndevParams;
	case MAPGEN_MATH:
		return new MapgenMathParams;
	case MAPGEN_EARTH:
		return new MapgenEarthParams;

	case MAPGEN_CARPATHIAN:
		return new MapgenCarpathianParams;
	case MAPGEN_FLAT:
		return new MapgenFlatParams;
	case MAPGEN_FRACTAL:
		return new MapgenFractalParams;
	case MAPGEN_SINGLENODE:
		return new MapgenSinglenodeParams;
	case MAPGEN_V5:
		return new MapgenV5Params;
	case MAPGEN_V6:
		return new MapgenV6Params;
	case MAPGEN_V7:
		return new MapgenV7Params;
	case MAPGEN_VALLEYS:
		return new MapgenValleysParams;
	default:
		return nullptr;
	}
}


void Mapgen::getMapgenNames(std::vector<const char *> *mgnames, bool include_hidden)
{
	for (u32 i = 0; i != ARRLEN(g_reg_mapgens); i++) {
		if (include_hidden || g_reg_mapgens[i].is_user_visible)
			mgnames->push_back(g_reg_mapgens[i].name);
	}
}

void Mapgen::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mg_flags", flagdesc_mapgen,
		 MG_CAVES | MG_DUNGEONS | MG_LIGHT | MG_DECORATIONS | MG_BIOMES | MG_ORES);

	for (int i = 0; i < (int)MAPGEN_INVALID; ++i) {
		MapgenParams *params = createMapgenParams((MapgenType)i);
		params->setDefaultSettings(settings);
		delete params;
	}
}

u32 Mapgen::getBlockSeed(v3pos_t p, s32 seed)
{
	return (u32)seed   +
		p.Z * 38134234 +
		p.Y * 42123    +
		p.X * 23;
}


u32 Mapgen::getBlockSeed2(v3pos_t p, s32 seed)
{
	// Multiply by unsigned number to avoid signed overflow (UB)
	u32 n = 1619U * p.X + 31337U * p.Y + 52591U * p.Z + 1013U * seed;
	n = (n >> 13) ^ n;
	return (n * (n * n * 60493 + 19990303) + 1376312589);
}


// Returns -MAX_MAP_GENERATION_LIMIT if not found
pos_t Mapgen::findGroundLevel(v2pos_t p2d, pos_t ymin, pos_t ymax)
{
	const v3pos_t &em = vm->m_area.getExtent();
	u32 i = vm->m_area.index(p2d.X, ymax, p2d.Y);
	pos_t y;

	for (y = ymax; y >= ymin; y--) {
		MapNode &n = vm->m_data[i];
		if (ndef->get(n).walkable)
			break;

		VoxelArea::add_y(em, i, -1);
	}
	return (y >= ymin) ? y : -MAX_MAP_GENERATION_LIMIT;
}


// Returns -MAX_MAP_GENERATION_LIMIT if not found or if ground is found first
pos_t Mapgen::findLiquidSurface(v2pos_t p2d, pos_t ymin, pos_t ymax)
{
	const v3pos_t &em = vm->m_area.getExtent();
	u32 i = vm->m_area.index(p2d.X, ymax, p2d.Y);
	pos_t y;

	for (y = ymax; y >= ymin; y--) {
		MapNode &n = vm->m_data[i];
		if (ndef->get(n).walkable)
			return -MAX_MAP_GENERATION_LIMIT;

		if (ndef->get(n).isLiquid())
			break;

		VoxelArea::add_y(em, i, -1);
	}
	return (y >= ymin) ? y : -MAX_MAP_GENERATION_LIMIT;
}


void Mapgen::updateHeightmap(v3pos_t nmin, v3pos_t nmax)
{
	if (!heightmap)
		return;

	//TimeTaker t("Mapgen::updateHeightmap", NULL, PRECISION_MICRO);
	int index = 0;
	for (pos_t z = nmin.Z; z <= nmax.Z; z++) {
		for (pos_t x = nmin.X; x <= nmax.X; x++, index++) {
			pos_t y = findGroundLevel(v2pos_t(x, z), nmin.Y, nmax.Y);

			heightmap[index] = y;
		}
	}
}


void Mapgen::getSurfaces(v2pos_t p2d, pos_t ymin, pos_t ymax,
	std::vector<pos_t> &floors, std::vector<pos_t> &ceilings)
{
	const v3pos_t &em = vm->m_area.getExtent();

	bool is_walkable = false;
	u32 vi = vm->m_area.index(p2d.X, ymax, p2d.Y);
	MapNode mn_max = vm->m_data[vi];
	bool walkable_above = ndef->get(mn_max).walkable;
	VoxelArea::add_y(em, vi, -1);

	for (pos_t y = ymax - 1; y >= ymin; y--) {
		MapNode mn = vm->m_data[vi];
		is_walkable = ndef->get(mn).walkable;

		if (is_walkable && !walkable_above) {
			floors.push_back(y);
		} else if (!is_walkable && walkable_above) {
			ceilings.push_back(y + 1);
		}

		VoxelArea::add_y(em, vi, -1);
		walkable_above = is_walkable;
	}
}


inline bool Mapgen::isLiquidHorizontallyFlowable(u32 vi, v3pos_t em)
{
	u32 vi_neg_x = vi;
	VoxelArea::add_x(em, vi_neg_x, -1);
	if (vm->m_data[vi_neg_x].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_nx = ndef->get(vm->m_data[vi_neg_x]);
		if (c_nx.floodable && !c_nx.isLiquid())
			return true;
	}
	u32 vi_pos_x = vi;
	VoxelArea::add_x(em, vi_pos_x, +1);
	if (vm->m_data[vi_pos_x].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_px = ndef->get(vm->m_data[vi_pos_x]);
		if (c_px.floodable && !c_px.isLiquid())
			return true;
	}
	u32 vi_neg_z = vi;
	VoxelArea::add_z(em, vi_neg_z, -1);
	if (vm->m_data[vi_neg_z].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_nz = ndef->get(vm->m_data[vi_neg_z]);
		if (c_nz.floodable && !c_nz.isLiquid())
			return true;
	}
	u32 vi_pos_z = vi;
	VoxelArea::add_z(em, vi_pos_z, +1);
	if (vm->m_data[vi_pos_z].getContent() != CONTENT_IGNORE) {
		const ContentFeatures &c_pz = ndef->get(vm->m_data[vi_pos_z]);
		if (c_pz.floodable && !c_pz.isLiquid())
			return true;
	}
	return false;
}

void Mapgen::updateLiquid(UniqueQueue<v3pos_t> *trans_liquid, v3pos_t nmin, v3pos_t nmax)
{
	if (!env)
		return;
	bool isignored, isliquid, wasignored, wasliquid, waschecked, waspushed;

	bool rare = g_settings->getBool("liquid_real");
	int rarecnt = 0;

	content_t was_n;
	const v3pos_t &em  = vm->m_area.getExtent();

	isignored = true;
	isliquid = false;
	was_n = CONTENT_IGNORE;

	for (pos_t z = nmin.Z + 1; z <= nmax.Z - 1; z++)
	for (pos_t x = nmin.X + 1; x <= nmax.X - 1; x++) {
		wasignored = true;
		wasliquid = false;
		waschecked = false;
		waspushed = false;

		u32 vi = vm->m_area.index(x, nmax.Y, z);
		for (pos_t y = nmax.Y; y >= nmin.Y; y--) {
			const content_t is_n = vm->m_data[vi].getContent();
			if (is_n != was_n) {
				isignored = is_n == CONTENT_IGNORE;
				isliquid = ndef->get(is_n).isLiquid();
			}

			if (isignored || wasignored || isliquid == wasliquid) {
				// Neither topmost node of liquid column nor topmost node below column
				waschecked = false;
				waspushed = false;
			} else if (isliquid) {
				// This is the topmost node in the column
				bool ispushed = false;
				if ((!rare || !(rarecnt++ % 36)) && isLiquidHorizontallyFlowable(vi, em)) {
					//trans_liquid->push_back(v3s16(x, y, z));
					env->getServerMap().transforming_liquid_add({x, y, z});
					ispushed = true;
				}
				// Remember waschecked and waspushed to avoid repeated
				// checks/pushes in case the column consists of only this node
				waschecked = true;
				waspushed = ispushed;
			} else {
				// This is the topmost node below a liquid column
				u32 vi_above = vi;
				VoxelArea::add_y(em, vi_above, 1);
				if ((!rare || !(rarecnt++ % 36)) && !waspushed && (ndef->get(vm->m_data[vi]).floodable ||
						(!waschecked && isLiquidHorizontallyFlowable(vi_above, em)))) {
					// Push back the lowest node in the column which is one
					// node above this one
					// trans_liquid->push_back(v3s16(x, y + 1, z));
					env->getServerMap().transforming_liquid_add(v3pos_t(x, y + 1, z));
				}
			}

			was_n = is_n;
			wasliquid = isliquid;
			wasignored = isignored;
			VoxelArea::add_y(em, vi, -1);
		}
	}
}


void Mapgen::setLighting(u8 light, v3pos_t nmin, v3pos_t nmax)
{
	ScopeProfiler sp(g_profiler, "EmergeThread: update lighting", SPT_AVG);
	VoxelArea a(nmin, nmax);

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
			u32 i = vm->m_area.index(a.MinEdge.X, y, z);
			for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++, i++)
				vm->m_data[i].param1 = light;
		}
	}
}


void Mapgen::lightSpread(VoxelArea &a, std::queue<std::pair<v3pos_t, u8>> &queue,
	const v3pos_t &p, u8 light)
{
	if (light <= 1 || !a.contains(p))
		return;

	u32 vi = vm->m_area.index(p);
	MapNode &n = vm->m_data[vi];

	// Decay light in each of the banks separately
	u8 light_day = light & 0x0F;
	if (light_day > 0)
		light_day -= 0x01;

	u8 light_night = light & 0xF0;
	if (light_night > 0x10)
		light_night -= 0x10;

	// Bail out only if we have no more light from either bank to propogate, or
	// we hit a solid block that light cannot pass through.
	if ((light_day  <= (n.param1 & 0x0F) &&
			light_night <= (n.param1 & 0xF0)) ||
			!ndef->getLightingFlags(n).light_propagates)
		return;

	// MYMAX still needed here because we only exit early if both banks have
	// nothing to propagate anymore.
	light = MYMAX(light_day, n.param1 & 0x0F) |
			MYMAX(light_night, n.param1 & 0xF0);

	n.param1 = light;

	// add to queue
	queue.emplace(p, light);
}

void Mapgen::calcLighting(v3pos_t nmin, v3pos_t nmax, v3pos_t full_nmin, v3pos_t full_nmax,
	bool propagate_shadow)
{
	ScopeProfiler sp(g_profiler, "EmergeThread: update lighting", SPT_AVG);

	propagateSunlight(nmin, nmax, propagate_shadow);
	spreadLight(full_nmin, full_nmax);
}


void Mapgen::propagateSunlight(v3pos_t nmin, v3pos_t nmax, bool propagate_shadow)
{
	//TimeTaker t("propagateSunlight");
	VoxelArea a(nmin, nmax);
	bool block_is_underground = (water_level >= nmax.Y);
	const v3pos_t &em = vm->m_area.getExtent();

	// NOTE: Direct access to the low 4 bits of param1 is okay here because,
	// by definition, sunlight will never be in the night lightbank.

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++) {
			// see if we can get a light value from the overtop
			u32 i = vm->m_area.index(x, a.MaxEdge.Y + 1, z);
			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {
				if (block_is_underground)
					continue;
			} else if ((vm->m_data[i].param1 & 0x0F) != LIGHT_SUN &&
					propagate_shadow) {
				u32 ii = 0;
				if (
				(x < a.MaxEdge.X && (ii = vm->m_area.index(x + 1, a.MaxEdge.Y + 1, z    )) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))||
				(x > a.MinEdge.X && (ii = vm->m_area.index(x - 1, a.MaxEdge.Y + 1, z    )) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))||
				(z > a.MinEdge.Z && (ii = vm->m_area.index(x    , a.MaxEdge.Y + 1, z - 1)) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))||
				(z < a.MaxEdge.Z && (ii = vm->m_area.index(x    , a.MaxEdge.Y + 1, z + 1)) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))
				) {
				} else
				continue;
			}
			VoxelArea::add_y(em, i, -1);

			for (int y = a.MaxEdge.Y; y >= a.MinEdge.Y; y--) {
				MapNode &n = vm->m_data[i];
				if (!ndef->getLightingFlags(n).sunlight_propagates)
					break;
				n.param1 = LIGHT_SUN;
				VoxelArea::add_y(em, i, -1);
			}
		}
	}
	//printf("propagateSunlight: %dms\n", t.stop());
}


void Mapgen::spreadLight(const v3pos_t &nmin, const v3pos_t &nmax)
{
	//TimeTaker t("spreadLight");
	std::queue<std::pair<v3pos_t, u8>> queue;
	VoxelArea a(nmin, nmax);

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
			u32 i = vm->m_area.index(a.MinEdge.X, y, z);
			for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++, i++) {
				MapNode &n = vm->m_data[i];
				if (n.getContent() == CONTENT_IGNORE)
					continue;

				ContentLightingFlags cf = ndef->getLightingFlags(n);
				if (!cf.light_propagates)
					continue;

				// TODO(hmmmmm): Abstract away direct param1 accesses with a
				// wrapper, but something lighter than MapNode::get/setLight

				u8 light_produced = cf.light_source;
				if (light_produced)
					n.param1 = light_produced | (light_produced << 4);

				u8 light = n.param1;
				if (light) {
					const v3pos_t p(x, y, z);
					// spread to all 6 neighbor nodes
					for (const auto &dir : g_6dirs)
						lightSpread(a, queue, p + dir, light);
				}
			}
		}
	}

	while (!queue.empty()) {
		const auto &i = queue.front();
		// spread to all 6 neighbor nodes
		for (const auto &dir : g_6dirs)
			lightSpread(a, queue, i.first + dir, i.second);
		queue.pop();
	}

	//printf("spreadLight: %lums\n", t.stop());
}


////
//// MapgenBasic
////

MapgenBasic::MapgenBasic(int mapgenid, MapgenParams *params, EmergeParams *emerge)
	: Mapgen(mapgenid, params, emerge)
{
	this->m_bmgr   = emerge->biomemgr;

	//// Here, 'stride' refers to the number of elements needed to skip to index
	//// an adjacent element for that coordinate in noise/height/biome maps
	//// (*not* vmanip content map!)

	// Note there is no X stride explicitly defined.  Items adjacent in the X
	// coordinate are assumed to be adjacent in memory as well (i.e. stride of 1).

	// Number of elements to skip to get to the next Y coordinate
	this->ystride = csize.X;

	// Number of elements to skip to get to the next Z coordinate
	this->zstride = csize.X * csize.Y;

	// Z-stride value for maps oversized for 1-down overgeneration
	this->zstride_1d = csize.X * (csize.Y + 1);

	// Z-stride value for maps oversized for 1-up 1-down overgeneration
	this->zstride_1u1d = csize.X * (csize.Y + 2);

	//// Allocate heightmap
	this->heightmap = new pos_t[csize.X * csize.Z];

	//// Initialize biome generator
	biomegen = emerge->biomegen;
	biomegen->assertChunkSize(csize);
	biomemap = biomegen->biomemap;

	//// Look up some commonly used content
	c_stone              = ndef->getId("mapgen_stone");
	c_water_source       = ndef->getId("mapgen_water_source");
	c_river_water_source = ndef->getId("mapgen_river_water_source");
	c_lava_source        = ndef->getId("mapgen_lava_source");
	c_cobble             = ndef->getId("mapgen_cobble");

	//freeminer:
	c_ice                = ndef->getId("mapgen_ice");
	if (c_ice == CONTENT_IGNORE)
		c_ice = c_water_source;
	//=========

	// Fall back to more basic content if not defined.
	// Lava falls back to water as both are suitable as cave liquids.
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = c_water_source;

	if (c_stone == CONTENT_IGNORE)
		errorstream << "Mapgen: Mapgen alias 'mapgen_stone' is invalid!" << std::endl;
	if (c_water_source == CONTENT_IGNORE)
		errorstream << "Mapgen: Mapgen alias 'mapgen_water_source' is invalid!" << std::endl;
	if (c_river_water_source == CONTENT_IGNORE)
		warningstream << "Mapgen: Mapgen alias 'mapgen_river_water_source' is invalid!" << std::endl;
}


MapgenBasic::~MapgenBasic()
{
	delete []heightmap;
}


void MapgenBasic::generateBiomes()
{
	// can't generate biomes without a biome generator!
	assert(biomegen);
	assert(biomemap);

	const v3pos_t &em = vm->m_area.getExtent();
	u32 index = 0;

	noise_filler_depth->perlinMap2D(node_min.X, node_min.Z);

	s16 *biome_transitions = biomegen->getBiomeTransitions();

	for (pos_t z = node_min.Z; z <= node_max.Z; z++)
	for (pos_t x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = NULL;
		biome_t water_biome_index = 0;
		pos_t depth_top = 0;
		u16 base_filler = 0;
		u16 depth_water_top = 0;
		u16 depth_riverbed = 0;
		u32 vi = vm->m_area.index(x, node_max.Y, z);

		int cur_biome_depth = 0;
		s16 biome_y_min = biome_transitions[cur_biome_depth];

		// Check node at base of mapchunk above, either a node of a previously
		// generated mapchunk or if not, a node of overgenerated base terrain.
		content_t c_above = vm->m_data[vi + em.X].getContent();
		bool air_above = c_above == CONTENT_AIR;
		bool river_water_above = c_above == c_river_water_source;
		bool water_above = c_above == c_water_source || c_above == c_ice || river_water_above;

		biomemap[index] = BIOME_NONE;

		// If there is air or water above enable top/filler placement, otherwise force
		// nplaced to stone level by setting a number exceeding any possible filler depth.
		u16 nplaced = (air_above || water_above) ? 0 : U16_MAX;

		const auto heat = m_emerge->env->m_use_weather ? m_emerge->env->getServerMap().updateBlockHeat(m_emerge->env, v3pos_t(x,node_max.Y,z), NULL, &heat_cache) : 0;

		for (pos_t y = node_max.Y; y >= node_min.Y; y--) {
			content_t c = vm->m_data[vi].getContent();
			bool cc_stone = (c != CONTENT_AIR && c != c_water_source && c != c_ice && c != CONTENT_IGNORE); // was in mt: c == c_stone
			// Biome is (re)calculated:
			// 1. At the surface of stone below air or water.
			// 2. At the surface of water below air.
			// 3. When stone or water is detected but biome has not yet been calculated.
			// 4. When stone or water is detected just below a biome's lower limit.
			bool is_stone_surface = (cc_stone) &&
				(air_above || water_above || !biome || y < biome_y_min); // 1, 3, 4

			bool is_water_surface =
				(c == c_water_source || c == c_river_water_source) &&
				(air_above || !biome || y < biome_y_min); // 2, 3, 4

			if (is_stone_surface || is_water_surface) {
				if (!biome || y < biome_y_min) {
					// (Re)calculate biome
					biome = biomegen->getBiomeAtIndex(index, v3pos_t(x, y, z));

					// Finding the height of the next biome
					// On first iteration this may loop a couple times after than it should just run once
					while (y < biome_y_min) {
						biome_y_min = biome_transitions[++cur_biome_depth];
					}

					/*if (x == node_min.X && z == node_min.Z)
						printf("Map: check @ %i -> %s -> again at %i\n", y, biome->name.c_str(), biome_y_min);*/
				}

				// Add biome to biomemap at first stone surface detected
				if (biomemap[index] == BIOME_NONE && is_stone_surface)
					biomemap[index] = biome->index;

				// Store biome of first water surface detected, as a fallback
				// entry for the biomemap.
				if (water_biome_index == 0 && is_water_surface)
					water_biome_index = biome->index;

				depth_top = biome->depth_top;
				base_filler = MYMAX(depth_top +
					biome->depth_filler +
					noise_filler_depth->result[index], 0.0f);
				depth_water_top = biome->depth_water_top;
				depth_riverbed = biome->depth_riverbed;
			}

			if (cc_stone && biome && (c == biome->c_ice || c == biome->c_water || c == biome->c_water_top))
				cc_stone = false;

			if (cc_stone) {
				content_t c_below = vm->m_data[vi - em.X].getContent();

				// If the node below isn't solid, make this node stone, so that
				// any top/filler nodes above are structurally supported.
				// This is done by aborting the cycle of top/filler placement
				// immediately by forcing nplaced to stone level.
				if (c_below == CONTENT_AIR
						|| c_below == c_water_source
						|| c_below == c_river_water_source)
					nplaced = U16_MAX;

				if (river_water_above) {
					if (nplaced < depth_riverbed) {
						vm->m_data[vi] = MapNode(biome->c_riverbed);
						nplaced++;
					} else {
						nplaced = U16_MAX;  // Disable top/filler placement
						river_water_above = false;
					}
				} else if (nplaced < depth_top) {
					vm->m_data[vi] = MapNode(
								((y < water_level) /* && (biome->c_top == c_dirt_with_grass)*/ )
									? biome->c_top
									: heat < -3
										? biome->c_top_cold
										: biome->c_top);
					nplaced++;
				} else if (nplaced < base_filler) {
					vm->m_data[vi] = MapNode(biome->c_filler);
					nplaced++;
				} else if (nplaced < (depth_top+base_filler+depth_water_top)) {
					vm->m_data[vi] = MapNode(biome->c_stone);
					nplaced = U16_MAX;  // Disable top/filler placement
				}

				air_above = false;
				water_above = false;
			} else if (c == c_water_source) {

				bool ice = heat < 0 && y > water_level + heat/4;

				vm->m_data[vi] = MapNode((y > (s32)(water_level - depth_water_top))

						? (ice ? biome->c_ice : biome->c_water_top) : ice ? biome->c_ice : biome->c_water);

				nplaced = 0;  // Enable top/filler placement for next surface
				air_above = false;
				water_above = true;
			} else if (c == c_river_water_source) {
				vm->m_data[vi] = MapNode(biome->c_river_water);
				nplaced = 0;  // Enable riverbed placement for next surface
				air_above = false;
				water_above = true;
				river_water_above = true;
			} else if (c == CONTENT_AIR) {
				nplaced = 0;  // Enable top/filler placement for next surface
				air_above = true;
				water_above = false;
			} else {  // Possible various nodes overgenerated from neighboring mapchunks
				nplaced = U16_MAX;  // Disable top/filler placement
				air_above = false;
				water_above = false;
			}

			VoxelArea::add_y(em, vi, -1);
		}
		// If no stone surface detected in mapchunk column and a water surface
		// biome fallback exists, add it to the biomemap. This avoids water
		// surface decorations failing in deep water.
 		if (biomemap[index] == BIOME_NONE && water_biome_index != 0)
			biomemap[index] = water_biome_index;
	}
}


void MapgenBasic::dustTopNodes()
{
	if (node_max.Y < water_level)
		return;

	const v3pos_t &em = vm->m_area.getExtent();
	u32 index = 0;

	for (pos_t z = node_min.Z; z <= node_max.Z; z++)
	for (pos_t x = node_min.X; x <= node_max.X; x++, index++) {
		Biome *biome = (Biome *)m_bmgr->getRaw(biomemap[index]);

		if (biome->c_dust == CONTENT_IGNORE)
			continue;

		// Check if mapchunk above has generated, if so, drop dust from 16 nodes
		// above current mapchunk top, above decorations that will extend above
		// the current mapchunk. If the mapchunk above has not generated, it
		// will provide this required dust when it does.
		u32 vi = vm->m_area.index(x, full_node_max.Y, z);
		content_t c_full_max = vm->m_data[vi].getContent();
		pos_t y_start;

		if (c_full_max == CONTENT_AIR) {
			y_start = full_node_max.Y - 1;
		} else if (c_full_max == CONTENT_IGNORE) {
			vi = vm->m_area.index(x, node_max.Y + 1, z);
			content_t c_max = vm->m_data[vi].getContent();

			if (c_max == CONTENT_AIR)
				y_start = node_max.Y;
			else
				continue;
		} else {
			continue;
		}

		vi = vm->m_area.index(x, y_start, z);
		for (pos_t y = y_start; y >= node_min.Y - 1; y--) {
			if (vm->m_data[vi].getContent() != CONTENT_AIR)
				break;

			VoxelArea::add_y(em, vi, -1);
		}

		content_t c = vm->m_data[vi].getContent();
		NodeDrawType dtype = ndef->get(c).drawtype;
		// Only place on cubic, walkable, non-dust nodes.
		// Dust check needed due to avoid double layer of dust caused by
		// dropping dust from 16 nodes above mapchunk top.

		//fm?: if (c == c_stone || c == biome->c_stone || c == c_desert_stone || c == c_sandstone || c == c_mossycobble || c == c_sandstonebrick)
		const auto & cdef = ndef->get(c);
		if (!cdef.waving && !cdef.buildable_to && c != c_ice)
		if ((dtype == NDT_NORMAL ||
				dtype == NDT_ALLFACES ||
				dtype == NDT_ALLFACES_OPTIONAL ||
				dtype == NDT_GLASSLIKE ||
				dtype == NDT_GLASSLIKE_FRAMED ||
				dtype == NDT_GLASSLIKE_FRAMED_OPTIONAL) &&
				ndef->get(c).walkable && c != biome->c_dust) {
			VoxelArea::add_y(em, vi, 1);
			vm->m_data[vi] = MapNode(biome->c_dust);
		}
	}
}


void MapgenBasic::generateCavesNoiseIntersection(pos_t max_stone_y)
{
	// cave_width >= 10 is used to disable generation and avoid the intensive
	// 3D noise calculations. Tunnels already have zero width when cave_width > 1.
	if (node_min.Y > max_stone_y || cave_width >= 10.0f)
		return;

	CavesNoiseIntersection caves_noise(ndef, m_bmgr, biomegen, csize,
		&np_cave1, &np_cave2, seed, cave_width);

	caves_noise.generateCaves(vm, node_min, node_max, biomemap);
}


void MapgenBasic::generateCavesRandomWalk(pos_t max_stone_y, pos_t large_cave_ymax)
{
	if (node_min.Y > max_stone_y)
		return;

	PseudoRandom ps(blockseed + 21343);
	// Small randomwalk caves
	u32 num_small_caves = ps.range(small_cave_num_min, small_cave_num_max);

	for (u32 i = 0; i < num_small_caves; i++) {
		CavesRandomWalk cave(ndef, &gennotify, seed, water_level,
			c_water_source, c_lava_source, large_cave_flooded, biomegen);
		cave.makeCave(vm, node_min, node_max, &ps, false, max_stone_y, heightmap);
	}

	if (node_max.Y > large_cave_ymax)
		return;

	// Large randomwalk caves below 'large_cave_ymax'.
	// 'large_cave_ymax' can differ from the 'large_cave_depth' mapgen parameter,
	// it is set to world base to disable large caves in or near caverns.
	u32 num_large_caves = ps.range(large_cave_num_min, large_cave_num_max);

	for (u32 i = 0; i < num_large_caves; i++) {
		CavesRandomWalk cave(ndef, &gennotify, seed, water_level,
			c_water_source, c_lava_source, large_cave_flooded, biomegen);
		cave.makeCave(vm, node_min, node_max, &ps, true, max_stone_y, heightmap);
	}
}


bool MapgenBasic::generateCavernsNoise(pos_t max_stone_y)
{
	if (node_min.Y > max_stone_y || node_min.Y > cavern_limit)
		return false;

	CavernsNoise caverns_noise(ndef, csize, &np_cavern,
		seed, cavern_limit, cavern_taper, cavern_threshold);

	return caverns_noise.generateCaverns(vm, node_min, node_max);
}


void MapgenBasic::generateDungeons(pos_t max_stone_y)
{
	if (node_min.Y > max_stone_y || node_min.Y > dungeon_ymax ||
			node_max.Y < dungeon_ymin)
		return;

	u16 num_dungeons = std::fmax(std::floor(
		NoisePerlin3D(&np_dungeons, node_min.X, node_min.Y, node_min.Z, seed)), 0.0f);
	if (num_dungeons == 0)
		return;

	PseudoRandom ps(blockseed + 70033);

	DungeonParams dp;

	dp.np_alt_wall =
		NoiseParams(-0.4, 1.0, v3f(40.0, 40.0, 40.0), 32474, 6, 1.1, 2.0);

	dp.seed                = seed;
	dp.only_in_ground      = true;
	dp.num_dungeons        = num_dungeons;
	dp.notifytype          = GENNOTIFY_DUNGEON;
	dp.num_rooms           = ps.range(2, 16);

    float far_multi = farscale(5, vm->m_area.MinEdge.X, vm->m_area.MinEdge.Y, vm->m_area.MinEdge.Z);
	dp.num_rooms = ps.range(2, ps.range(2, 16 * far_multi));

	dp.room_size_min       = v3pos_t(5, 5, 5);
	dp.room_size_max       = v3pos_t(12, 6, 12);
	dp.room_size_large_min = v3pos_t(12, 6, 12);
	dp.room_size_large_max = v3pos_t(16, 16, 16);
	dp.large_room_chance   = (ps.range(1, 4) == 1) ? 8 : 0;
	dp.diagonal_dirs       = ps.range(1, 8) == 1;
	// Diagonal corridors must have 'hole' width >=2 to be passable
	u8 holewidth           = (dp.diagonal_dirs) ? 2 : ps.range(1, 2);
	dp.holesize            = v3pos_t(holewidth, 3, holewidth);
	dp.corridor_len_min    = 1;
	dp.corridor_len_max    = 13;

	// Get biome at mapchunk midpoint
	v3pos_t chunk_mid = node_min + (node_max - node_min) / v3pos_t(2, 2, 2);
	Biome *biome = (Biome *)biomegen->getBiomeAtPoint(chunk_mid);

	// Use biome-defined dungeon nodes if defined
	if (biome->c_dungeon != CONTENT_IGNORE) {
		dp.c_wall = biome->c_dungeon;
		// If 'node_dungeon_alt' is not defined by biome, it and dp.c_alt_wall
		// become CONTENT_IGNORE which skips the alt wall node placement loop in
		// dungeongen.cpp.
		dp.c_alt_wall = biome->c_dungeon_alt;
		// Stairs fall back to 'c_dungeon' if not defined by biome
		dp.c_stair = (biome->c_dungeon_stair != CONTENT_IGNORE) ?
			biome->c_dungeon_stair : biome->c_dungeon;
	// Fallback to using cobble mapgen alias if defined
	} else if (c_cobble != CONTENT_IGNORE) {
		dp.c_wall     = c_cobble;
		dp.c_alt_wall = CONTENT_IGNORE;
		dp.c_stair    = c_cobble;
	// Fallback to using biome-defined stone
	} else {
		dp.c_wall     = biome->c_stone;
		dp.c_alt_wall = CONTENT_IGNORE;
		dp.c_stair    = biome->c_stone;
	}

	DungeonGen dgen(ndef, &gennotify, &dp);
	dgen.generate(vm, blockseed, full_node_min, full_node_max);
}


////
//// GenerateNotifier
////

GenerateNotifier::GenerateNotifier(u32 notify_on,
	const std::set<u32> *notify_on_deco_ids)
{
	m_notify_on = notify_on;
	m_notify_on_deco_ids = notify_on_deco_ids;
}


bool GenerateNotifier::addEvent(GenNotifyType type, v3pos_t pos, u32 id)
{
	if (!(m_notify_on & (1 << type)))
		return false;

	if (type == GENNOTIFY_DECORATION &&
		m_notify_on_deco_ids->find(id) == m_notify_on_deco_ids->cend())
		return false;

	GenNotifyEvent gne;
	gne.type = type;
	gne.pos  = pos;
	gne.id   = id;
	m_notify_events.push_back(gne);

	return true;
}


void GenerateNotifier::getEvents(
	std::map<std::string, std::vector<v3pos_t> > &event_map)
{
	std::list<GenNotifyEvent>::iterator it;

	for (it = m_notify_events.begin(); it != m_notify_events.end(); ++it) {
		GenNotifyEvent &gn = *it;
		std::string name = (gn.type == GENNOTIFY_DECORATION) ?
			"decoration#"+ itos(gn.id) :
			flagdesc_gennotify[gn.type].name;

		event_map[name].push_back(gn.pos);
	}
}


void GenerateNotifier::clearEvents()
{
	m_notify_events.clear();
}


////
//// MapgenParams
////


MapgenParams::~MapgenParams()
{
	delete bparams;
}


void MapgenParams::readParams(const Settings *settings)
{
	// should always be used via MapSettingsManager
	assert(settings != g_settings);

	std::string seed_str;
	if (settings->getNoEx("seed", seed_str)) {
		if (!seed_str.empty())
			seed = read_seed(seed_str.c_str());
		else
			myrand_bytes(&seed, sizeof(seed));
	}

	std::string mg_name;
	if (settings->getNoEx("mg_name", mg_name)) {
		mgtype = Mapgen::getMapgenType(mg_name);
		if (mgtype == MAPGEN_INVALID)
			mgtype = MAPGEN_DEFAULT;
	}

	settings->getPosNoEx("water_level", water_level);
	settings->getPosNoEx("mapgen_limit", mapgen_limit);
	settings->getS16NoEx("chunksize", chunksize);
	settings->getFlagStrNoEx("mg_flags", flags, flagdesc_mapgen);

	settings->getS16NoEx("liquid_pressure", liquid_pressure);
	
	chunksize = rangelim(chunksize, 1, 10);

	delete bparams;
	bparams = static_cast<BiomeParamsOriginal*>(BiomeManager::createBiomeParams(BIOMEGEN_ORIGINAL));
	if (bparams) {
		bparams->readParams(settings);
		bparams->seed = seed;
	}
}


void MapgenParams::writeParams(Settings *settings) const
{
	settings->set("mg_name", Mapgen::getMapgenName(mgtype));
	settings->setU64("seed", seed);
	settings->setPos("water_level", water_level);
	settings->setPos("mapgen_limit", mapgen_limit);
	settings->setS16("chunksize", chunksize);
	settings->setFlagStr("mg_flags", flags, flagdesc_mapgen);

	settings->setS16("liquid_pressure", liquid_pressure);

	if (bparams)
		bparams->writeParams(settings);
}


s32 MapgenParams::getSpawnRangeMax()
{
	if (!m_mapgen_edges_calculated) {
		std::pair<pos_t, pos_t> edges = get_mapgen_edges(mapgen_limit, chunksize);
		mapgen_edge_min = edges.first;
		mapgen_edge_max = edges.second;
		m_mapgen_edges_calculated = true;
	}

	return MYMIN(-mapgen_edge_min, mapgen_edge_max);
}


std::pair<pos_t, pos_t> get_mapgen_edges(pos_t mapgen_limit, s16 chunksize)
{
	// Central chunk offset, in blocks
	s16 ccoff_b = -chunksize / 2;
	// Chunksize, in nodes
	s32 csize_n = chunksize * MAP_BLOCKSIZE;
	// Minp/maxp of central chunk, in nodes
	s16 ccmin = ccoff_b * MAP_BLOCKSIZE;
	s16 ccmax = ccmin + csize_n - 1;
	// Fullminp/fullmaxp of central chunk, in nodes
	s16 ccfmin = ccmin - MAP_BLOCKSIZE;
	s16 ccfmax = ccmax + MAP_BLOCKSIZE;
	// Effective mapgen limit, in blocks
	// Uses same calculation as ServerMap::blockpos_over_mapgen_limit(v3pos_t p)
	pos_t mapgen_limit_b = rangelim(mapgen_limit,
		0, MAX_MAP_GENERATION_LIMIT) / MAP_BLOCKSIZE;
	// Effective mapgen limits, in nodes
	pos_t mapgen_limit_min = -mapgen_limit_b * MAP_BLOCKSIZE;
	pos_t mapgen_limit_max = (mapgen_limit_b + 1) * MAP_BLOCKSIZE - 1;
	// Number of complete chunks from central chunk fullminp/fullmaxp
	// to effective mapgen limits.
	pos_t numcmin = MYMAX((ccfmin - mapgen_limit_min) / csize_n, 0);
	pos_t numcmax = MYMAX((mapgen_limit_max - ccfmax) / csize_n, 0);
	// Mapgen edges, in nodes
	return std::pair<pos_t, pos_t>(ccmin - numcmin * csize_n, ccmax + numcmax * csize_n);
}

bool Mapgen::visible_water_level(const v3pos_t &p)
{
	return p.Y < water_level;
}

bool Mapgen::visible(const v3pos_t &p)
{
	return getGroundLevelAtPoint({p.X, p.Z}) >= p.Y;
}

const MapNode &Mapgen::visible_content(const v3pos_t &p, bool use_weather)
{
	const auto v = visible(p);
	const auto vw = visible_water_level(p);
	if (!v && !vw)
		return visible_transparent;
	if (!use_weather)
		return visible_surface_green;
	const auto heat = m_emerge->biomemgr->calcBlockHeat(p, seed,
			env ? env->getTimeOfDay() * env->m_time_of_day_speed : 0,
			env ? env->getGameTime() : 0, !!env && env->m_use_weather);
	if (!v && p.Y < water_level)
		return heat < 0 ? visible_ice : visible_water;
	const auto humidity = m_emerge->biomemgr->calcBlockHumidity(p, seed,
			env ? env->getTimeOfDay() * env->m_time_of_day_speed : 0,
			env ? env->getGameTime() : 0, !!env && env->m_use_weather);
	return heat < 0	   ? (humidity < 20 ? visible_surface : visible_surface_cold)
		   : heat < 10 ? visible_surface
		   : heat < 40 ? (humidity < 20 ? visible_surface_dry : visible_surface_green)
					   : visible_surface_hot;
}
