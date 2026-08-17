#pragma once
#include "mapnode.h"
#include <string>
#include <unordered_map>
namespace arnis
{
class Block : public MapNode
{
public:
	Block(content_t c = {}) : MapNode{c} {}

	content_t id() const { return getContent(); }
};
class BlockWithProperties
{
public:
	Block block;
	// Java/Sponge block-state values.  MapNode has no portable generic state
	// channel, but retaining these makes the C++ model equivalent to Rust and
	// allows format-specific world writers to encode them.
	std::unordered_map<std::string, std::string> properties;
	BlockWithProperties(Block b = {}) : block(b) {}
	BlockWithProperties(Block b, std::unordered_map<std::string, std::string> p) :
			block(b), properties(std::move(p)) {}
	static BlockWithProperties simple(Block b)
	{
		return BlockWithProperties{b};
	}
};

}
