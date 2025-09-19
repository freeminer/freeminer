#pragma once
#include <optional>
#include "../../arnis_adapter.h"

class MapgenEarth;
/*
namespace osmium
{
class Relation;
class Way;
}
*/
namespace arnis
{

void go_buildings(MapgenEarth *mg, const osmium::Relation &relation);
void go_way(MapgenEarth *mg, const osmium::Way &way);

namespace buildings
{

void generate_building_from_relation(
		WorldEditor &editor, const ProcessedRelation &relation, const Args &args);
void generate_buildings(WorldEditor *editor, const ProcessedWay &element,
		const Args &args, const std::optional<int> &relation_levels);
}
}