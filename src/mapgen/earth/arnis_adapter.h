#pragma once

// Compatibility facade for the ported Arnis API. Large implementation types
// live in focused headers so users can include only the layer they need.
#include "arnis_types.h"
#include "arnis_ground.h"
#include "arnis_world_editor.h"
#include "../../debug/dump.h"

#include "arnis-cpp/src/block_definitions.h"

namespace arnis
{

void init(MapgenEarth *mg);
Block get_castle_wall_block();

namespace args
{
using Args = arnis::Args;
}

namespace osm_parser
{
using ElementType = arnis::ElementType;
using ProcessedElement = arnis::ProcessedElement;
using ProcessedNode = arnis::ProcessedNode;
using ProcessedRelation = arnis::ProcessedRelation;
using ProcessedWay = arnis::ProcessedWay;
using Way = arnis::ProcessedWay;
}

using Node = ProcessedNode;

namespace coordinate_system::cartesian
{
using XZPoint = arnis::XZPoint;
}

namespace block_definitions
{
using Block = arnis::Block;
}

}

namespace crate = arnis;

#include "arnis-cpp/src/bresenham.h"
