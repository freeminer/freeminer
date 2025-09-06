#pragma once
#include <optional>
#include "adapter.h"

class MapgenEarth;
/*
namespace osmium
{
class Relation;
class Way;
}
*/

void go_buildings(MapgenEarth *mg, const osmium::Relation &relation);
void go_way(MapgenEarth *mg, const osmium::Way &way);

namespace buildings
{

void generate_building_from_relation(WorldEditor &editor,
		const ProcessedRelation &relation, const Ground &ground, const Args &args);
void generate_buildings(WorldEditor &editor, const ProcessedWay &element,
		const Ground &ground, const Args &args, std::optional<int> relation_levels);
}