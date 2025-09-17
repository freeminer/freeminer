#include "arnis_adapter.h"
#include "emerge.h"

XZ::operator XZPoint()
{
	return XZPoint{X, Y};
}

namespace arnis
{
//MapgenEarth *mg = {};
namespace block_definitions
{

Block AIR;
Block BLACK_CONCRETE;
Block BRICK;
Block COBBLESTONE_WALL;
Block COBBLESTONE;
Block DIRT;
Block GLOWSTONE;
Block GRASS_BLOCK;
Block GRAY_CONCRETE;
Block GREEN_WOOL;
Block LIGHT_GRAY_CONCRETE;
Block OAK_FENCE_GATE;
Block OAK_FENCE;
Block OAK_PLANKS;
Block RED_WOOL;
Block SAND;
Block SMOOTH_STONE;
Block SNOW_LAYER;
Block STONE_BLOCK_SLAB;
Block STONE_BRICKS;
Block STONE;
Block WHITE_CONCRETE;
Block WHITE_STAINED_GLASS;
Block WHITE_WOOL;
Block YELLOW_WOOL;
Block STONE_BRICK_SLAB;

Block GRAVEL;
Block OAK_LOG;
Block RAIL_NORTH_SOUTH;
Block RAIL_EAST_WEST;
Block RAIL_NORTH_WEST;
Block RAIL_NORTH_EAST;
Block RAIL_SOUTH_WEST;
Block RAIL_SOUTH_EAST;
Block IRON_BLOCK;
Block DIRT_PATH;
Block GLASS;
Block SANDSTONE;
Block SMOOTH_STONE_BLOCK;
}

bool inited = false;

void init(MapgenEarth *mg)
{
	if (inited) {
		return;
	}
	inited = true;
	/*
	if (mg_) {
		mg = mg_;
	}*/

	// TODO: find better in mods
	AIR = mg->m_emerge->ndef->getId("air");
	BLACK_CONCRETE = mg->m_emerge->ndef->getId("default:obsidian"); // ?
	BRICK = mg->m_emerge->ndef->getId("default:brick");
	COBBLESTONE = mg->m_emerge->ndef->getId("default:cobble");
	COBBLESTONE_WALL = mg->m_emerge->ndef->getId("default:cobble");
	DIRT = mg->m_emerge->ndef->getId("default:dirt");
	GLOWSTONE = mg->m_emerge->ndef->getId("default:stone");
	GRASS_BLOCK = mg->m_emerge->ndef->getId("default:dirt_with_grass");
	GRAY_CONCRETE = mg->m_emerge->ndef->getId("default:stone"); //  TODO
	GREEN_WOOL = mg->m_emerge->ndef->getId("wool:green");
	LIGHT_GRAY_CONCRETE = mg->m_emerge->ndef->getId("default:cobble"); // TODO
	OAK_FENCE = mg->m_emerge->ndef->getId("default:wood");
	OAK_FENCE_GATE = mg->m_emerge->ndef->getId("default:wood");
	OAK_PLANKS = mg->m_emerge->ndef->getId("default:wood");
	RED_WOOL = mg->m_emerge->ndef->getId("wool:red");
	SAND = mg->m_emerge->ndef->getId("default:sand");
	SMOOTH_STONE = mg->m_emerge->ndef->getId("default:stone");
	SNOW_LAYER = mg->m_emerge->ndef->getId("default:snow");
	STONE = mg->m_emerge->ndef->getId("default:stone");
	STONE_BLOCK_SLAB = mg->m_emerge->ndef->getId("default:stone");
	STONE_BRICK_SLAB = mg->m_emerge->ndef->getId("default:cobble");
	STONE_BRICKS = mg->m_emerge->ndef->getId("default:cobble");
	WHITE_CONCRETE = mg->m_emerge->ndef->getId("default:stone");	  // TODO
	WHITE_STAINED_GLASS = mg->m_emerge->ndef->getId("default:glass"); // ?
	WHITE_WOOL = mg->m_emerge->ndef->getId("wool:white");
	YELLOW_WOOL = mg->m_emerge->ndef->getId("wool:yellow");

	GRAVEL = mg->m_emerge->ndef->getId("default:gravel");
	OAK_LOG = mg->m_emerge->ndef->getId("default:tree"); // TODO
	RAIL_NORTH_SOUTH = mg->m_emerge->ndef->getId("default:rail");
	RAIL_EAST_WEST = mg->m_emerge->ndef->getId("default:rail");
	RAIL_NORTH_WEST = mg->m_emerge->ndef->getId("default:rail");
	RAIL_NORTH_EAST = mg->m_emerge->ndef->getId("default:rail");
	RAIL_SOUTH_WEST = mg->m_emerge->ndef->getId("default:rail");
	RAIL_SOUTH_EAST = mg->m_emerge->ndef->getId("default:rail");
	IRON_BLOCK = mg->m_emerge->ndef->getId("default:steelblock"); // TODO
	DIRT_PATH = mg->m_emerge->ndef->getId("default:dry_dirt");
	GLASS = mg->m_emerge->ndef->getId("default:glass");
	SANDSTONE = mg->m_emerge->ndef->getId("default:sandstone");
	SMOOTH_STONE_BLOCK = mg->m_emerge->ndef->getId("default:stone"); //TODO
}
}