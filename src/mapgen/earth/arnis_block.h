
#include "mapnode.h"
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
	static BlockWithProperties simple(Block b)
	{
		return BlockWithProperties{
				b /*, StairFacing::North, StairShape::Straight, false*/};
	}
};

}