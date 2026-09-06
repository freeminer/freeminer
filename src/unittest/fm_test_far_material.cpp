// SPDX-License-Identifier: GPL-3.0-or-later
// Included by test_fm_content_mapblock.cpp to reuse FmMockGameDef.

#include "client/fm_far_material.h"
#include "fm_far_node.h"
#include "fm_world_merge.h"

namespace
{
class FarMaterialFastFacesSetting
{
	const bool previous = g_settings->getBool("farmesh_fast_faces");

public:
	explicit FarMaterialFastFacesSetting(bool enabled)
	{
		g_settings->setBool("farmesh_fast_faces", enabled);
	}
	~FarMaterialFastFacesSetting()
	{
		g_settings->setBool("farmesh_fast_faces", previous);
	}
};

content_t addFarCover(
		FmMockGameDef &gamedef, NodeDrawType drawtype, AlphaMode alpha = ALPHAMODE_CLIP)
{
	ItemDefinition item;
	item.type = ITEM_NODE;
	item.name = "test:cover";
	ContentFeatures features;
	features.name = item.name;
	features.drawtype = drawtype;
	features.alpha = alpha;
	features.visuals = std::make_unique<NodeVisuals>();
	features.visuals->solidness = 0;
	features.visuals->visual_solidness = 1;
	features.visuals->fm_far_tiles = std::make_unique<TileLayer[]>(6);
	for (u8 i = 0; i < 6; ++i) {
		auto &base = features.visuals->tiles[i].layers[0];
		base.texture_id = 43;
		base.shader_id = 99;
		base.material_type = alpha_mode_to_material_type(alpha);
		features.visuals->fm_far_tiles[i] = base;
		features.visuals->fm_far_tiles[i].texture_id = 44;
		features.visuals->fm_far_tiles[i].texture_layer_idx = 7;
	}
	return gamedef.registerNode(item, std::move(features));
}

constexpr NodeDrawType far_cover_types[] = {NDT_GLASSLIKE, NDT_GLASSLIKE_FRAMED,
		NDT_GLASSLIKE_FRAMED_OPTIONAL, NDT_PLANTLIKE, NDT_ALLFACES, NDT_ALLFACES_OPTIONAL,
		NDT_NORMAL, NDT_NODEBOX};

class TestFmFarMaterial : public TestBase
{
public:
	TestFmFarMaterial() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmFarMaterial"; }
	void runTests(IGameDef *) override
	{
		setFmLightDecodeTable();
		TEST(testOpaqueUnderCover);
		TEST(testCoverOccupancy);
		TEST(testOpaqueSurfaceVote);
		TEST(testFarCoverRendering);
		TEST(testOverlayAndNearTextures);
		TEST(testTextureDefinitionAndClassification);
	}

	void testOpaqueUnderCover()
	{
		for (const auto drawtype : far_cover_types) {
			FmMockGameDef gamedef;
			const auto stone = gamedef.addSimpleNode("stone", 42);
			const auto cover = addFarCover(gamedef, drawtype);
			gamedef.finalize();
			UASSERT(farmesh::isTransparentCover(gamedef.ndef()->get(cover)));
			std::array<MapNode, 8> samples;
			samples.fill(MapNode(cover));
			samples[1] = MapNode(stone, 12, 3);
			std::array<bool, 8> exposed;
			exposed.fill(true);
			exposed[1] = false;
			for (const auto *exposure :
					{&exposed, static_cast<std::array<bool, 8> *>(nullptr)}) {
				const auto selected = world_merge::selectFarNodeIndex(
						samples, exposure, gamedef.ndef());
				UASSERT(selected);
				UASSERTEQ(size_t, *selected, 1);
			}
		}
	}

	void testCoverOccupancy()
	{
		for (const auto drawtype : far_cover_types) {
			FmMockGameDef gamedef;
			const auto cover = addFarCover(gamedef, drawtype, ALPHAMODE_BLEND);
			const auto stone = gamedef.addSimpleNode("stone", 42);
			gamedef.finalize();
			for (const size_t count : {size_t{0}, size_t{2}, size_t{4}, size_t{8}}) {
				std::array<MapNode, 8> samples;
				samples.fill(MapNode(CONTENT_AIR));
				for (size_t i = 0; i < count; ++i)
					samples[i] = MapNode(cover);
				const auto selected =
						world_merge::selectFarNodeIndex(samples, nullptr, gamedef.ndef());
				UASSERT(selected);
				UASSERTEQ(content_t, samples[*selected].getContent(),
						count >= 4 ? cover : CONTENT_AIR);
				if (count == 2) {
					samples[3] = MapNode(stone);
					const auto mixed = world_merge::selectFarNodeIndex(
							samples, nullptr, gamedef.ndef());
					UASSERT(mixed);
					UASSERTEQ(content_t, samples[*mixed].getContent(), CONTENT_AIR);
				}
			}
		}
	}

	void testOpaqueSurfaceVote()
	{
		FmMockGameDef gamedef;
		const auto dirt = gamedef.addSimpleNode("dirt", 42);
		const auto grass = gamedef.addSimpleNode("dirt_with_grass", 43);
		gamedef.finalize();
		std::array<MapNode, 8> samples;
		samples.fill(MapNode(dirt));
		samples[0] = MapNode(grass);
		std::array<bool, 8> exposed{};
		exposed[0] = true;
		const auto selected =
				world_merge::selectFarNodeIndex(samples, &exposed, gamedef.ndef());
		UASSERT(selected);
		UASSERTEQ(content_t, samples[*selected].getContent(), grass);
	}

	void testFarCoverRendering()
	{
		for (const bool fast : {false, true})
			for (const auto alpha : {ALPHAMODE_CLIP, ALPHAMODE_BLEND})
				for (const auto drawtype : far_cover_types) {
					FarMaterialFastFacesSetting setting(fast);
					FmMockGameDef gamedef;
					const auto cover = addFarCover(gamedef, drawtype, alpha);
					gamedef.finalize();
					auto data = gamedef.makeMMD();
					data.m_vmanip.setNode({0, 0, 0}, MapNode(cover));
					MeshCollector collector{{}};
					MapblockMeshGenerator{&data, &collector}.generate();
					UASSERTEQ(size_t, collector.prebuffers[0].size(), 1);
					const auto &buffer = collector.prebuffers[0][0];
					UASSERTEQ(size_t, buffer.vertices.size(), 24);
					UASSERTEQ(size_t, buffer.indices.size(), 36);
					UASSERTEQ(u32, buffer.layer.texture_id, 44);
					UASSERTEQ(u16, buffer.layer.texture_layer_idx, 7);
					UASSERTEQ(u32, buffer.layer.shader_id, 99);
				}
	}

	void testOverlayAndNearTextures()
	{
		FmMockGameDef gamedef;
		const auto cover = addFarCover(gamedef, NDT_GLASSLIKE);
		gamedef.finalize();
		auto &visuals = *gamedef.ndef()->get(cover).visuals;
		visuals.tiles[0].layers[1] = visuals.tiles[0].layers[0];
		visuals.tiles[0].layers[1].texture_id = 45;
		for (const int far_step : {0, 2}) {
			auto data = gamedef.makeMMD(false, far_step);
			TileSpec tile;
			getNodeTileN(MapNode(cover), {0, 0, 0}, 0, &data, tile);
			UASSERTEQ(u32, tile.layers[0].texture_id, far_step ? 44 : 43);
			UASSERTEQ(u32, tile.layers[0].shader_id, 99);
			UASSERTEQ(u32, tile.layers[1].texture_id, 45);
		}
		const auto stone = gamedef.addSimpleNode("stone", 46);
		auto data = gamedef.makeMMD();
		TileSpec tile;
		getNodeTileN(MapNode(stone), {0, 0, 0}, 0, &data, tile);
		UASSERTEQ(u32, tile.layers[0].texture_id, 46);
	}

	void testTextureDefinitionAndClassification()
	{
		TileDef original;
		original.name = "glass.png^[opacity:64";
		original.animation.type = TAT_VERTICAL_FRAMES;
		original.animation.vertical_frames.aspect_w = 16;
		original.animation.vertical_frames.aspect_h = 16;
		original.scale = 2;
		const auto far = farmesh::opaqueFarTileDef(original);
		UASSERTEQ(std::string, far.name, "glass.png^[opacity:64^[noalpha");
		UASSERTEQ(TileAnimationType, far.animation.type, TAT_VERTICAL_FRAMES);
		UASSERTEQ(u8, far.scale, original.scale);
		UASSERTEQ(std::string, original.name, "glass.png^[opacity:64");
		UASSERT(farmesh::opaqueFarTileDef(TileDef{}).name.empty());
		ContentFeatures liquid;
		liquid.drawtype = NDT_LIQUID;
		liquid.liquid_type = LIQUID_SOURCE;
		liquid.alpha = ALPHAMODE_BLEND;
		UASSERT(!farmesh::isTransparentCover(liquid));
		UASSERT(!farmesh::isOpaqueStructure(liquid));
	}
};

static TestFmFarMaterial g_far_material_tests;
}
