// Freeminer far-mesh tests kept separate from the upstream mesh-generator tests.

#include "irr_v3d.h"
#include "test.h"

#include "client/content_mapblock.h"
#include "client/mapblock_mesh.h"
#include "client/meshgen/collector.h"
#include "client/node_visuals.h"
#include "dummygamedef.h"
#include "gamedef.h"
#include "inventory.h"
#include "settings.h"

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <tuple>

namespace
{
class FmFastFacesSetting
{
	const bool previous = g_settings->getBool("farmesh_fast_faces");

public:
	explicit FmFastFacesSetting(bool enabled)
	{
		g_settings->setBool("farmesh_fast_faces", enabled);
	}
	~FmFastFacesSetting() { g_settings->setBool("farmesh_fast_faces", previous); }
};

class FmMockGameDef : public DummyGameDef
{
public:
	IWritableItemDefManager *itemMgr() noexcept
	{
		return static_cast<IWritableItemDefManager *>(m_itemdef);
	}

	NodeDefManager *nodeMgr() noexcept { return const_cast<NodeDefManager *>(m_nodedef); }

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

	MeshMakeData makeMMD(
			bool smooth_lighting = false, int far_step = 2, u16 side_length = 1)
	{
		MeshMakeData data{ndef(), side_length, MeshGrid{1}, 0, far_step};
		data.m_generate_minimap = false;
		data.m_smooth_lighting = smooth_lighting;
		data.m_enable_water_reflections = false;
		data.m_blockpos = {0, 0, 0};
		const auto padding = static_cast<pos_t>(data.fscale);
		for (pos_t x = -padding; x <= side_length * padding; ++x)
			for (pos_t y = -padding; y <= side_length * padding; ++y)
				for (pos_t z = -padding; z <= side_length * padding; ++z)
					data.m_vmanip.setNode({x, y, z}, {CONTENT_AIR, 0, 0});
		return data;
	}

	content_t addSimpleNode(const std::string &name, u32 texture, bool stone = false)
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
		if (stone)
			f.groups["stone"] = 1;
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
			0x00,
			0x11,
			0x22,
			0x33,
			0x44,
			0x55,
			0x66,
			0x77,
			0x88,
			0x99,
			0xAA,
			0xBB,
			0xCC,
			0xDD,
			0xEE,
			0xFF,
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
	void testFastFaceRectangles();
	void testRectangleBoundaries();
	void testFastFaceDisabled();
	void testUnknownNeighborOccludesFace();
	void testFastFaceCoverage();
	void testGrassUsesOpaqueGround();
	void testEmbeddedNodeUsesHost();
	void testSurfaceCoverKeepsTexture();
};

static TestFmContentMapblock g_test_instance;

void TestFmContentMapblock::runTests(IGameDef *gamedef)
{
	setFmLightDecodeTable();
	const FmFastFacesSetting fast_faces(true);
	TEST(testFarNode);
	TEST(testFastFaceMerging);
	TEST(testFastFaceRectangles);
	TEST(testRectangleBoundaries);
	TEST(testFastFaceDisabled);
	TEST(testUnknownNeighborOccludesFace);
	TEST(testFastFaceCoverage);
	TEST(testGrassUsesOpaqueGround);
	TEST(testEmbeddedNodeUsesHost);
	TEST(testSurfaceCoverKeepsTexture);
}

void TestFmContentMapblock::testFarNode()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	content_t wood = gamedef.addSimpleNode("wood", 13);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode({static_cast<pos_t>(data.fscale), 0, 0}, {wood, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERTEQ(std::size_t, buf.vertices.size(), 20);
	UASSERTEQ(std::size_t, buf.indices.size(), 30);
	for (const auto &vertex : buf.vertices)
		UASSERT(vertex.Normal != v3opos_t(1, 0, 0));

	aabb3f bounds(buf.vertices[0].Pos);
	for (const auto &vertex : buf.vertices)
		bounds.addInternalPoint(vertex.Pos);
	UASSERT(bounds.MinEdge == v3opos_t(-HBS, 1.5f * BS - data.fscale * BS, -HBS));
	UASSERT(bounds.MaxEdge ==
			v3opos_t(data.fscale * BS - HBS, 1.5f * BS, data.fscale * BS - HBS));
}

void TestFmContentMapblock::testFastFaceMerging()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD(false, 2, 2);
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode({static_cast<pos_t>(data.fscale), 0, 0}, {stone, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(std::size_t, buf.vertices.size(), 24);
	UASSERTEQ(std::size_t, buf.indices.size(), 36);

	aabb3f bounds(buf.vertices[0].Pos);
	for (const auto &vertex : buf.vertices)
		bounds.addInternalPoint(vertex.Pos);
	UASSERT(bounds.MinEdge == v3opos_t(-HBS, 1.5f * BS - data.fscale * BS, -HBS));
	UASSERT(bounds.MaxEdge ==
			v3opos_t(2 * data.fscale * BS - HBS, 1.5f * BS, data.fscale * BS - HBS));
}

void TestFmContentMapblock::testFastFaceRectangles()
{
	for (const bool world_aligned : {false, true})
		for (const int step : {1, 2, 3}) {
			FmMockGameDef gamedef;
			const auto stone = gamedef.addSimpleNode("stone", 42);
			for (auto &tile : gamedef.ndef()->get(stone).visuals->tiles)
				tile.world_aligned = world_aligned;
			gamedef.finalize();
			MeshMakeData data = gamedef.makeMMD(false, step, 6);
			const v3pos_t size(2, 3, 4);
			const v3pos_t origin(1, 1, 1);
			for (pos_t x = 0; x < size.X; ++x)
				for (pos_t y = 0; y < size.Y; ++y)
					for (pos_t z = 0; z < size.Z; ++z)
						data.m_vmanip.setNode(
								(origin + v3pos_t(x, y, z)) * data.fscale, {stone, 0, 0});
			MeshCollector col{{}};
			MapblockMeshGenerator{&data, &col}.generate();
			UASSERTEQ(size_t, col.prebuffers[0].size(), 1);
			const auto &buf = col.prebuffers[0][0];
			// Six rectangles replace the strips on all sides of the cuboid.
			UASSERTEQ(size_t, buf.vertices.size(), 24);
			UASSERTEQ(size_t, buf.indices.size(), 36);
			std::array<unsigned, 6> directions{};
			for (size_t i = 0; i < buf.vertices.size(); i += 4) {
				const auto &a = buf.vertices[i];
				const auto &b = buf.vertices[i + 1];
				const auto &d = buf.vertices[i + 3];
				const int axis = a.Normal.X ? 0 : a.Normal.Y ? 1 : 2;
				++directions[axis * 2 + (a.Normal[axis] < 0)];
				// UV density must match individual cells along both edges, including
				// faces whose second merge direction is vertical.
				UASSERT(std::abs(std::abs(a.TCoords.X - b.TCoords.X) -
								 (a.Pos - b.Pos).getLength() / BS) < 0.0001f);
				UASSERT(std::abs(std::abs(a.TCoords.Y - d.TCoords.Y) -
								 (a.Pos - d.Pos).getLength() / (BS * data.fscale)) <
						0.0001f);
			}
			for (const auto count : directions)
				UASSERTEQ(unsigned, count, 1);
			aabb3f bounds(buf.vertices[0].Pos);
			for (const auto &vertex : buf.vertices)
				bounds.addInternalPoint(vertex.Pos);
			const auto expected_min = v3opos_t::from(origin) * (BS * data.fscale) +
									  v3opos_t(-HBS, 1.5f * BS - data.fscale * BS, -HBS);
			UASSERT(bounds.MinEdge == expected_min);
			UASSERT(bounds.MaxEdge ==
					expected_min + v3opos_t::from(size) * (BS * data.fscale));
		}
}

// Expand rectangles into unit far faces so gaps, duplicate coverage, material
// boundaries and corner lighting can be compared with the individual-face path.
using FmFaceCoverage = std::map<std::tuple<pos_t, pos_t, pos_t, int>,
		std::pair<u32, std::array<u32, 4>>>;

static FmFaceCoverage getFmFaceCoverage(const MeshCollector &col, int fscale)
{
	FmFaceCoverage coverage;
	const float cell = BS * fscale;
	const v3opos_t base(-HBS, 1.5f * BS - cell, -HBS);
	for (const auto &buf : col.prebuffers[0]) {
		for (size_t i = 0; i < buf.indices.size(); i += 3) {
			const auto &a = buf.vertices[buf.indices[i]];
			const auto &b = buf.vertices[buf.indices[i + 1]];
			const auto &c = buf.vertices[buf.indices[i + 2]];
			UASSERT((b.Pos - a.Pos).crossProduct(c.Pos - a.Pos).dotProduct(a.Normal) > 0);
		}
		for (size_t i = 0; i < buf.vertices.size(); i += 4) {
			const auto &first = buf.vertices[i];
			const int axis = first.Normal.X ? 0 : first.Normal.Y ? 1 : 2;
			const int u_axis = first.Normal.X ? 2 : 0;
			const int v_axis = first.Normal.Y ? 2 : 1;
			aabb3f bounds(first.Pos);
			for (size_t j = 1; j < 4; ++j)
				bounds.addInternalPoint(buf.vertices[i + j].Pos);
			v3pos_t begin, end;
			for (int a = 0; a < 3; ++a) {
				begin[a] = std::lround((bounds.MinEdge[a] - base[a]) / cell);
				end[a] = std::lround((bounds.MaxEdge[a] - base[a]) / cell);
			}
			begin[axis] -= first.Normal[axis] > 0;
			end[axis] = begin[axis] + 1;
			for (pos_t x = begin.X; x < end.X; ++x)
				for (pos_t y = begin.Y; y < end.Y; ++y)
					for (pos_t z = begin.Z; z < end.Z; ++z) {
						const auto cell_min = base + v3opos_t(x, y, z) * cell;
						std::array<u32, 4> colors;
						for (int corner = 0; corner < 4; ++corner) {
							const float u =
									(cell_min[u_axis] + (corner % 2) * cell -
											bounds.MinEdge[u_axis]) /
									(bounds.MaxEdge[u_axis] - bounds.MinEdge[u_axis]);
							const float v =
									(cell_min[v_axis] + (corner / 2) * cell -
											bounds.MinEdge[v_axis]) /
									(bounds.MaxEdge[v_axis] - bounds.MinEdge[v_axis]);
							std::array<float, 4> channels{};
							for (size_t j = 0; j < 4; ++j) {
								const auto &vertex = buf.vertices[i + j];
								const float weight =
										(vertex.Pos[u_axis] == bounds.MinEdge[u_axis]
														? 1 - u
														: u) *
										(vertex.Pos[v_axis] == bounds.MinEdge[v_axis]
														? 1 - v
														: v);
								channels[0] += weight * vertex.Color.getAlpha();
								channels[1] += weight * vertex.Color.getRed();
								channels[2] += weight * vertex.Color.getGreen();
								channels[3] += weight * vertex.Color.getBlue();
							}
							colors[corner] = video::SColor(std::lround(channels[0]),
									std::lround(channels[1]), std::lround(channels[2]),
									std::lround(channels[3]))
													 .color;
						}
						UASSERT(coverage.emplace(std::make_tuple(x, y, z,
														 axis * 2 + (first.Normal[axis] <
																			0)),
												std::make_pair(
														buf.layer.texture_id, colors))
										.second);
					}
		}
	}
	return coverage;
}

void TestFmContentMapblock::testRectangleBoundaries()
{
	for (const bool smooth : {false, true}) {
		FmMockGameDef gamedef;
		const auto stone = gamedef.addSimpleNode("stone", 42);
		const auto wood = gamedef.addSimpleNode("wood", 13);
		gamedef.finalize();
		constexpr pos_t side = 6;
		MeshMakeData data = gamedef.makeMMD(smooth, 2, side);
		// Provide both smooth gradients and a hard light boundary across rows.
		for (pos_t x = -data.fscale; x <= side * data.fscale; ++x)
			for (pos_t y = -data.fscale; y <= side * data.fscale; ++y)
				for (pos_t z = -data.fscale; z <= side * data.fscale; ++z) {
					const u8 light = z < 3 * data.fscale ? 0xEE : 0x66;
					data.m_vmanip.setNode({x, y, z}, {CONTENT_AIR, light, 0});
				}
		for (pos_t x = 0; x < side; ++x)
			for (pos_t y = 0; y < 3; ++y)
				for (pos_t z = 0; z < side; ++z) {
					// A platform with a hole, a material seam and a raised staircase.
					const bool hole = x >= 2 && x < 4 && z >= 2 && z < 4;
					if (!hole && (!y || (x < 3 - y && z < 3 - y)))
						data.m_vmanip.setNode(v3pos_t(x, y, z) * data.fscale,
								{x == 4 ? wood : stone, 0, 0});
				}
		MeshCollector merged{{}};
		MapblockMeshGenerator{&data, &merged}.generate();
		MeshCollector individual{{}};
		{
			const FmFastFacesSetting disabled(false);
			MapblockMeshGenerator{&data, &individual}.generate();
		}
		const auto expected = getFmFaceCoverage(individual, data.fscale);
		UASSERT(!expected.empty());
		UASSERT(getFmFaceCoverage(merged, data.fscale) == expected);
		size_t merged_vertices = 0, individual_vertices = 0;
		for (const auto &buf : merged.prebuffers[0])
			merged_vertices += buf.vertices.size();
		for (const auto &buf : individual.prebuffers[0])
			individual_vertices += buf.vertices.size();
		UASSERT(merged_vertices < individual_vertices);
	}
}

void TestFmContentMapblock::testFastFaceDisabled()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD(false, 2, 2);
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode({static_cast<pos_t>(data.fscale), 0, 0}, {stone, 0, 0});

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

void TestFmContentMapblock::testUnknownNeighborOccludesFace()
{
	FmMockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	static const v3pos_t directions[6] = {
			{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
	for (const auto &dir : directions)
		data.m_vmanip.setNode(dir * data.fscale, {CONTENT_IGNORE, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	// Authoritative unknown cells are invisible occluders in both far paths.
	UASSERT(col.prebuffers[0].empty());
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
					data.m_vmanip.setNode(v3pos_t(x, y, z) * data.fscale, {stone, 0, 0});
			}

	size_t expected_faces = 0;
	static const v3pos_t directions[6] = {
			{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
	for (int x = 0; x < side; ++x)
		for (int y = 0; y < side; ++y)
			for (int z = 0; z < side; ++z) {
				if (!occupied[x][y][z])
					continue;
				for (const auto &dir : directions) {
					const int nx = x + dir.X;
					const int ny = y + dir.Y;
					const int nz = z + dir.Z;
					if (nx < 0 || nx >= side || ny < 0 || ny >= side || nz < 0 ||
							nz >= side || !occupied[nx][ny][nz])
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
		const v3opos_t cross = (b.Pos - a.Pos).crossProduct(c.Pos - a.Pos);
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
	content_t stone = gamedef.addSimpleNode("stone", 42, true);
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

void TestFmContentMapblock::testSurfaceCoverKeepsTexture()
{
	FmMockGameDef gamedef;
	content_t dirt = gamedef.addSimpleNode("dirt", 42);
	content_t grass = gamedef.addSimpleNode("dirt_with_grass", 13);
	UASSERT(dirt != grass);
	gamedef.finalize();
	MeshMakeData data = gamedef.makeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {grass, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator{&data, &col}.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	const auto &buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 13);
	UASSERTEQ(std::size_t, buf.vertices.size(), 24);
	UASSERTEQ(std::size_t, buf.indices.size(), 36);
}
}

// fm: Far transparent-cover selection and texture regression tests.
#include "fm_test_far_material.cpp"
// ===
