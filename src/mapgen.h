/*
mapgen.h
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef MAPGEN_HEADER
#define MAPGEN_HEADER

#include "noise.h"
#include "nodedef.h"
#include "mapnode.h"
#include "util/string.h"
#include "util/container.h"

#define DEFAULT_MAPGEN "indev"

/////////////////// Mapgen flags
#define MG_TREES         0x01
#define MG_CAVES         0x02
#define MG_DUNGEONS      0x04
#define MG_FLAT          0x08
#define MG_LIGHT         0x10

class Settings;
class MMVManip;
class INodeDefManager;

extern FlagDesc flagdesc_mapgen[];
extern FlagDesc flagdesc_gennotify[];

class Biome;
class EmergeManager;
class MapBlock;
class VoxelManipulator;
struct BlockMakeData;
class VoxelArea;
class Map;

enum MapgenObject {
	MGOBJ_VMANIP,
	MGOBJ_HEIGHTMAP,
	MGOBJ_BIOMEMAP,
	MGOBJ_HEATMAP,
	MGOBJ_HUMIDMAP,
	MGOBJ_GENNOTIFY
};

enum GenNotifyType {
	GENNOTIFY_DUNGEON,
	GENNOTIFY_TEMPLE,
	GENNOTIFY_CAVE_BEGIN,
	GENNOTIFY_CAVE_END,
	GENNOTIFY_LARGECAVE_BEGIN,
	GENNOTIFY_LARGECAVE_END,
	GENNOTIFY_DECORATION,
	NUM_GENNOTIFY_TYPES
};

// TODO(hmmmm/paramat): make stone type selection dynamic
enum MgStoneType {
	STONE,
	DESERT_STONE,
	SANDSTONE,
};

struct GenNotifyEvent {
	GenNotifyType type;
	v3s16 pos;
	u32 id;
};

class GenerateNotifier {
public:
	GenerateNotifier();
	GenerateNotifier(u32 notify_on, std::set<u32> *notify_on_deco_ids);

	void setNotifyOn(u32 notify_on);
	void setNotifyOnDecoIds(std::set<u32> *notify_on_deco_ids);

	bool addEvent(GenNotifyType type, v3s16 pos, u32 id=0);
	void getEvents(std::map<std::string, std::vector<v3s16> > &event_map,
		bool peek_events=false);

private:
	u32 m_notify_on;
	std::set<u32> *m_notify_on_deco_ids;
	std::list<GenNotifyEvent> m_notify_events;
};

struct MapgenSpecificParams {
	virtual void readParams(Settings *settings) = 0;
	virtual void writeParams(Settings *settings) const = 0;
	virtual ~MapgenSpecificParams() {}
};

struct MapgenParams {
	std::string mg_name;
	s16 chunksize;
	u64 seed;
	s16 water_level;
	s16 liquid_pressure;
	u32 flags;

	NoiseParams np_biome_heat;
	NoiseParams np_biome_heat_blend;
	NoiseParams np_biome_humidity;
	NoiseParams np_biome_humidity_blend;

	MapgenSpecificParams *sparams;

	MapgenParams() :
		mg_name(DEFAULT_MAPGEN),
		chunksize(5),
		seed(0),
		water_level(1),
		liquid_pressure(0),
		flags(MG_TREES | MG_CAVES | MG_LIGHT),
		np_biome_heat(NoiseParams(15, 30, v3f(750.0, 750.0, 750.0), 5349, 3, 0.5, 2.0)),
		np_biome_heat_blend(NoiseParams(0, 1.5, v3f(8.0, 8.0, 8.0), 13, 2, 1.0, 2.0)),
		np_biome_humidity(NoiseParams(50, 50, v3f(750.0, 750.0, 750.0), 842, 3, 0.5, 2.0)),
		np_biome_humidity_blend(NoiseParams(0, 1.5, v3f(8.0, 8.0, 8.0), 90003, 2, 1.0, 2.0)),
		sparams(NULL)
	{}

	void load(Settings &settings);
	void save(Settings &settings) const;
};

class Mapgen {
public:
	int seed;
	int water_level;
	u32 flags;
	bool generating;
	int id;

	MMVManip *vm;
	INodeDefManager *ndef;

	u32 blockseed;
	s16 *heightmap;
	u8 *biomemap;
	float *heatmap;
	float *humidmap;
	v3s16 csize;

	GenerateNotifier gennotify;

	Mapgen();
	Mapgen(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	virtual ~Mapgen();

	static u32 getBlockSeed(v3s16 p, int seed);
	static u32 getBlockSeed2(v3s16 p, int seed);
	s16 findGroundLevelFull(v2s16 p2d);
	s16 findGroundLevel(v2s16 p2d, s16 ymin, s16 ymax);
	s16 findLiquidSurface(v2s16 p2d, s16 ymin, s16 ymax);
	void updateHeightmap(v3s16 nmin, v3s16 nmax);
	void updateLiquid(v3s16 nmin, v3s16 nmax);

	void setLighting(u8 light, v3s16 nmin, v3s16 nmax);
	void lightSpread(VoxelArea &a, v3s16 p, u8 light);

	void calcLighting(v3s16 nmin, v3s16 nmax);
	void calcLighting(v3s16 nmin, v3s16 nmax,
		v3s16 full_nmin, v3s16 full_nmax);

	void propagateSunlight(v3s16 nmin, v3s16 nmax);
	void spreadLight(v3s16 nmin, v3s16 nmax);

	virtual void makeChunk(BlockMakeData *data) {}
	virtual int getGroundLevelAtPoint(v2s16 p) { return 0; }

	// freeminer:
	s16 liquid_pressure;
	unordered_map_v3POS<s16> heat_cache;
	unordered_map_v3POS<s16> humidity_cache;


private:
	DISABLE_CLASS_COPY(Mapgen);
};

struct MapgenFactory {
	virtual Mapgen *createMapgen(int mgid, MapgenParams *params,
		EmergeManager *emerge) = 0;
	virtual MapgenSpecificParams *createMapgenParams() = 0;
	virtual ~MapgenFactory() {}
};

#endif
