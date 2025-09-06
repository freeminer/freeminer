#include "adapter.h"
#include "emerge.h"

namespace arnis
{
//MapgenEarth *mg = {};

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
	OAK_FENCE = mg->m_emerge->ndef->getId("default:tree");
	OAK_FENCE_GATE = mg->m_emerge->ndef->getId("default:tree");
	OAK_PLANKS = mg->m_emerge->ndef->getId("default:tree");
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
}
}