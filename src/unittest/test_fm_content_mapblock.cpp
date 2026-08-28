// Freeminer far-mesh tests kept separate from the upstream mesh-generator tests.

#include "test.h"

#include "client/content_mapblock.h"
#include "client/mapblock_mesh.h"
#include "client/meshgen/collector.h"
#include "client/node_visuals.h"
#include "dummygamedef.h"
#include "gamedef.h"
#include "inventory.h"
#include "settings.h"

#include <memory>

namespace
{
class FmMockGameDef : public DummyGameDef
{
public:
	IWritableItemDefManager *itemMgr() noexcept
	{
		return static_cast<IWritableItemDefManager *>(m_itemdef);
	}

	NodeDefManager *nodeMgr() noexcept
	{
		return const_cast<NodeDefManager *>(m_nodedef);
	}

	content_t registerNode(const ItemDefinition &itemdef, ContentFeatures &&nodedef)
	{
		itemMgr()->registerItem(itemdef);
		return nodeMgr()->set(nodedef.name, std::move(nodedef));
	}

	void finalize()
	{
		nodeMgr()->resolveCrossrefs();
		nodeMgr()->applyFunction([](ContentFeatures &f) {
			if (!f.visuals)
				f.visuals = std::make_unique<NodeVisuals>();
		});
	}

	MeshMakeData makeMMD(bool smooth_lighting = false, int far_step = 2,
			u16 side_length = 1)
	{
		MeshMakeData data{ndef(), side_length, MeshGrid{1}, 0, far_step};
		data.m_generate_minimap = false;
		data.m_smooth_lighting = smooth_lighting;
		data.m_enable_water_reflections = false;
		data.m_blockpos = {0, 0, 0};
		const auto padding = static_cast<s16>(data.fscale);
		for (s16 x = -padding; x <= side_length * padding; ++x)
		for (s16 y = -padding; y <= side_length * padding; ++y)
		for (s16 z = -padding; z <= side_length * padding; ++z)
			data.m_vmanip.setNode({x, y, z}, {CONTENT_AIR, 0, 0});
		return data;
	}

	content_t addSimpleNode(const std::string &name, u32 texture)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name;
		itemdef.description = name;

		ContentFeatures f;
		f.visuals = std::make_unique<NodeVisuals>();
		f.name = itemdef.name;
		f.drawtype = NDT_NORMAL;
		f.visuals->solidness = 2;
		f.alpha = ALPHAMODE_OPAQUE;
		for (TileDef &tiledef : f.tiledef)
			tiledef.name = name + ".png";
		for (TileSpec &tile : f.visuals->tiles)
			tile.layers[0].texture_id = texture;
		return registerNode(itemdef, std::move(f));
	}

	content_t addPlantNode(const std::string &name, u32 texture)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name;
		itemdef.description = name;

		ContentFeatures f;
		f.visuals = std::make_unique<NodeVisuals>();
		f.name = itemdef.name;
		f.drawtype = NDT_PLANTLIKE;
		f.visuals->solidness = 0;
		f.alpha = ALPHAMODE_CLIP;
		for (TileSpec &tile : f.visuals->tiles)
			tile.layers[0].texture_id = texture;
		return registerNode(itemdef, std::move(f));
	}
};

void setFmLightDecodeTable()
{
	u8 table[LIGHT_SUN + 1] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	};
	memcpy(const_cast<u8 *>(light_decode_table), table, sizeof(table));
}

class TestFmContentMapblock : public TestBase
{
public:
	TestFmContentMapblock() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmContentMapblock"; }
	void runTests(IGameDef *gamedef) override;

	void testFarNode();
	void testFastFaceMerging();
	void testFastFaceDisabled();
	void testUnknownNeighborDoesNotHideFace();
	void testFastFaceCoverage();
	void testGrassUsesOpaqueGround();
	void testEmbeddedNodeUsesHost();
};

static TestFmContentMapblock g_test_instance;

void TestFmContentMapblock::runTests(IGameDef *gamedef)
{
	setFmLightDecodeTable();
	TEST(testFarNode);
	TEST(testFastFaceMerging);
	TEST(testFastFaceDisabled);
	TEST(testUnknownNeighborDoesNotHideFace);
	TEST(testFastFaceCoverage);
	TEST(testGrassUsesOpaqueGround);
	TEST(testEmbeddedNodeUsesHost);
}

void TestFmContentMapblock::testFarNode()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	content_t wood = gamedef.addSimpleNode("wood", 13);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode(
			{static_cast<s16>(data.fscale), 0, 0}, {wood, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERTEQ(std::size_t, buf.vertices.size(), 20);
	UASSERTEQ(std::size_t, buf.indices.size(), 30);
	for (const auto &vertex : buf.vertices)
		UASSERT(vertex.Normal != v3f(1, 0, 0));

	aabb3f bounds(buf.vertices[0].Pos);
	for (const auto &vertex : buf.vertices)
		bounds.addInternalPoint(vertex.Pos);
	UASSERT(bounds.MinEdge ==
			v3f(-HBS, 1.5f * BS - data.fscale * BS, -HBS));
	UASSERT(bounds.MaxEdge == v3f(data.fscale * BS - HBS, 1.5f * BS,
			data.fscale * BS - HBS));
}

void TestFmContentMapblock::testFastFaceMerging()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD(false, 2, 2);
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode(
			{static_cast<s16>(data.fscale), 0, 0}, {stone, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(std::size_t, buf.vertices.size(), 24);
	UASSERTEQ(std::size_t, buf.indices.size(), 36);

	aabb3f bounds(buf.vertices[0].Pos);
	for (const auto &vertex : buf.vertices)
		bounds.addInternalPoint(vertex.Pos);
	UASSERT(bounds.MinEdge ==
			v3f(-HBS, 1.5f * BS - data.fscale * BS, -HBS));
	UASSERT(bounds.MaxEdge == v3f(2 * data.fscale * BS - HBS, 1.5f * BS,
			data.fscale * BS - HBS));
}

void TestFmContentMapblock::testFastFaceDisabled()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD(false, 2, 2);
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode(
			{static_cast<s16>(data.fscale), 0, 0}, {stone, 0, 0});

	const bool was_enabled = g_settings->getBool("farmesh_fast_faces");
	g_settings->setBool("farmesh_fast_faces", false);
	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	g_settings->setBool("farmesh_fast_faces", was_enabled);

	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(std::size_t, buf.vertices.size(), 40);
	UASSERTEQ(std::size_t, buf.indices.size(), 60);
}

void TestFmContentMapblock::testUnknownNeighborDoesNotHideFace()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	static const v3s16 directions[6] = {
			{0, 1, 0}, {0, -1, 0}, {1, 0, 0},
			{-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
	for (const auto &dir : directions)
		data.m_vmanip.setNode(dir * data.fscale, {CONTENT_IGNORE, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(std::size_t, buf.vertices.size(), 24);
	UASSERTEQ(std::size_t, buf.indices.size(), 36);
}

void TestFmContentMapblock::testFastFaceCoverage()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	constexpr int side = 4;
	bool occupied[side][side][side]{};
	MeshMakeData data = gamedef.makeMMD(false, 2, side);
	for (int x = 0; x < side; ++x)
	for (int y = 0; y < side; ++y)
	for (int z = 0; z < side; ++z) {
		occupied[x][y][z] = (x + 2 * y + 3 * z) % 5 < 2;
		if (occupied[x][y][z])
			data.m_vmanip.setNode(v3s16(x, y, z) * data.fscale, {stone, 0, 0});
	}

	size_t expected_faces = 0;
	static const v3s16 directions[6] = {
			{0, 1, 0}, {0, -1, 0}, {1, 0, 0},
			{-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
	for (int x = 0; x < side; ++x)
	for (int y = 0; y < side; ++y)
	for (int z = 0; z < side; ++z) {
		if (!occupied[x][y][z])
			continue;
		for (const auto &dir : directions) {
			const int nx = x + dir.X;
			const int ny = y + dir.Y;
			const int nz = z + dir.Z;
			if (nx < 0 || nx >= side || ny < 0 || ny >= side ||
					nz < 0 || nz >= side || !occupied[nx][ny][nz])
				++expected_faces;
		}
	}

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	double triangle_area = 0.0;
	for (size_t i = 0; i < buf.indices.size(); i += 3) {
		const auto &a = buf.vertices[buf.indices[i]];
		const auto &b = buf.vertices[buf.indices[i + 1]];
		const auto &c = buf.vertices[buf.indices[i + 2]];
		const v3f cross = (b.Pos - a.Pos).crossProduct(c.Pos - a.Pos);
		UASSERT(cross.dotProduct(a.Normal) > 0.0f);
		triangle_area += cross.getLength() * 0.5;
	}
	const double cell_face_area = data.fscale * BS * data.fscale * BS;
	const double expected_area = expected_faces * cell_face_area;
	UASSERT(std::abs(triangle_area - expected_area) < expected_area * 0.0001);
}

void TestFmContentMapblock::testGrassUsesOpaqueGround()
{
	FmMockGameDef gamedef;
	content_t ground = gamedef.addSimpleNode("dirt_with_grass", 42);
	content_t grass = gamedef.addPlantNode("grass_5", 13);
	UASSERT(ground != grass);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {grass, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERTEQ(std::size_t, buf.vertices.size(), 24);
	UASSERTEQ(std::size_t, buf.indices.size(), 36);
}

void TestFmContentMapblock::testEmbeddedNodeUsesHost()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	content_t ore = gamedef.addSimpleNode("stone_with_copper", 13);
	UASSERT(stone != ore);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {ore, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERTEQ(std::size_t, buf.vertices.size(), 24);
	UASSERTEQ(std::size_t, buf.indices.size(), 36);
}
}
