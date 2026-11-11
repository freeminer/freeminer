#include "test.h"

#include "mapgen/earth/arnis-cpp/src/clipping.h"
#include "mapgen/earth/arnis-cpp/src/args.h"
#include "mapgen/earth/arnis-cpp/src/data_processing.h"
#include "mapgen/earth/arnis-cpp/src/decals/render.h"
#include "mapgen/earth/arnis-cpp/src/fm_ll_shapes.h"
#include "mapgen/earth/arnis-cpp/src/fm_xz_bbox_shape.h"
#include "mapgen/earth/arnis-cpp/src/map_transformation/rotate/rotator.h"
#include "mapgen/earth/arnis-cpp/src/world_editor/floor_state.h"
#include "mapgen/earth/arnis_world_editor.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class TestFmArnisShapes : public TestBase
{
public:
	TestFmArnisShapes() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisShapes"; }
	void runTests(IGameDef *) override
	{
		TEST(testGeoJsonShapesPreserveHoles);
		TEST(testOutlineRasterSupportsCoverageAndHoles);
		TEST(testRustRasterizationEdgeCases);
		TEST(testOutlineRasterRejectsOutsideSpans);
		TEST(testWaterRingClipMatchesRustFastPaths);
		TEST(testClippedRingPreservesSurvivingNodeIdsAndTags);
		TEST(testShapeCropAndSpawnHelpers);
		TEST(testSpawnMustBeInsideSelectedShape);
		TEST(testBedrockGenerationCanUseParallelTiles);
		TEST(testNetworkWaysAreCutAtTheOutline);
		TEST(testOutlineCutMatchesRustEdgeCases);
		TEST(testOutlineCutSaturatesRemoteCoordinates);
		TEST(testRotatingWorldAlsoRotatesOutline);
		TEST(testWorldEditorRejectsWritesOutsideOutline);
		TEST(testTransparentPreviewPixelsStayClearOnLocalMaps);
	}

	void testGeoJsonShapesPreserveHoles()
	{
		const std::string json = R"({"type":"FeatureCollection","features":[
			{"type":"Feature","geometry":{"type":"Polygon","coordinates":[
				[[13,52],[14,52],[14,53],[13,53],[13,52]],
				[[13.2,52.2],[13.8,52.2],[13.8,52.8],[13.2,52.8],[13.2,52.2]]]}},
			{"type":"Feature","geometry":{"type":"Point","coordinates":[0,0]}}]})";
		std::string error;
		auto polygons = arnis::LLShapes::from_geojson(json, &error);
		UASSERT(polygons.has_value());
		auto shapes = arnis::LLShapes::create(*polygons, &error);
		UASSERT(shapes.has_value());
		UASSERT(shapes->polygons().size() == 1);
		UASSERT(shapes->polygons().front().size() == 2);
		UASSERT(shapes->contains(arnis::geographic::LLPoint(52.1, 13.1)));
		UASSERT(!shapes->contains(arnis::geographic::LLPoint(52.5, 13.5)));
		UASSERT(shapes->contains(arnis::geographic::LLPoint(52.2, 13.5)));
		const auto missing_geometry = arnis::LLShapes::from_geojson(
				R"({"type":"FeatureCollection","features":[{"type":"Feature"},{"type":"Feature","geometry":null},{"type":"Feature","geometry":{"type":"Polygon","coordinates":[[[13,52],[14,52],[14,53],[13,53]]]}}]})",
				&error);
		UASSERT(missing_geometry.has_value());
		UASSERT(missing_geometry->size() == 1);
		auto point_only = arnis::LLShapes::from_geojson(
				R"({"type":"Point","coordinates":[13,52]})", &error);
		UASSERT(!point_only.has_value());
		UASSERT(error == "no Polygon or MultiPolygon geometry found");
		arnis::LLShapes::Polygons invalid_polygons;
		invalid_polygons.push_back({{arnis::geographic::LLPoint(52.0, 13.0),
				arnis::geographic::LLPoint(52.0, 13.1)}});
		auto invalid_shape = arnis::LLShapes::create(invalid_polygons, &error);
		UASSERT(!invalid_shape.has_value());
		UASSERT(error.find("Shape 1: ") == 0);
	}

	void testOutlineRasterSupportsCoverageAndHoles()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {20, 20});
		arnis::XZBBoxShape::Polygons polygons{{{{0, 0}, {20, 0}, {20, 20}, {0, 20}},
				{{7, 7}, {13, 7}, {13, 13}, {7, 13}}}};
		std::string error;
		auto shape = arnis::XZBBoxShape::create(rect, std::move(polygons), &error);
		UASSERT(shape.has_value());
		UASSERT(shape->contains(3, 3));
		UASSERT(!shape->contains(10, 10));
		UASSERT(shape->contains(7, 10));
		UASSERT(shape->covers(1, 1, 5, 5));
		UASSERT(!shape->covers(5, 5, 9, 9));
		UASSERT(!shape->touches(9, 9, 11, 11));
		UASSERT(shape->touches(5, 9, 9, 11));
		UASSERT(shape->covered_columns() > 0 && shape->covered_columns() < 21 * 21);
	}

	void testRustRasterizationEdgeCases()
	{
		const arnis::cartesian::XZBBoxRect frame({0, 0}, {30, 30});
		const auto cw = arnis::XZBBoxShape::create(
				frame, {{{{2.5, 3.5}, {2.5, 20.2}, {17.9, 20.2}, {17.9, 3.5}}}});
		const auto ccw = arnis::XZBBoxShape::create(frame,
				{{{{2.5, 3.5}, {17.9, 3.5}, {17.9, 20.2}, {2.5, 20.2}, {2.5, 3.5}}}});
		UASSERT(cw.has_value() && ccw.has_value());
		UASSERT(cw->covered_columns() == 16 * 18);
		for (int z = 0; z <= 30; ++z)
			for (int x = 0; x <= 30; ++x)
				UASSERT(cw->contains(x, z) == ccw->contains(x, z));

		const auto l_shape = arnis::XZBBoxShape::create(
				frame, {{{{0, 0}, {20, 0}, {20, 8}, {8, 8}, {8, 20}, {0, 20}}}});
		UASSERT(l_shape.has_value());
		UASSERT(!l_shape->contains(9, 9) && !l_shape->contains(20, 20));
		UASSERT(l_shape->contains(20, 8) && l_shape->contains(8, 20));
		UASSERT(l_shape->covered_columns() == 21 * 9 + 9 * 12);
		UASSERT(l_shape->covers(0, 0, 15, 7));
		UASSERT(!l_shape->covers(0, 0, 15, 15));
		UASSERT(l_shape->touches(5, 5, 15, 15));
		UASSERT(!l_shape->touches(10, 10, 20, 20));

		const auto overlapping = arnis::XZBBoxShape::create(
				frame, {{{{0, 0}, {15, 0}, {15, 15}, {0, 15}}},
							   {{{10, 10}, {25, 10}, {25, 25}, {10, 25}}}});
		UASSERT(overlapping.has_value());
		UASSERT(overlapping->contains(12, 12));
		UASSERT(overlapping->covered_columns() == 2 * 16 * 16 - 6 * 6);

		const arnis::cartesian::XZBBoxRect triangle_frame({0, 0}, {50, 50});
		const std::array<std::pair<double, double>, 3> triangle{
				{{0.3, 0.2}, {49.6, 7.1}, {12.4, 48.9}}};
		arnis::XZBBoxShape::Polygons triangle_polygons{
				{arnis::XZBBoxShape::Ring(triangle.begin(), triangle.end())}};
		const auto triangle_shape =
				arnis::XZBBoxShape::create(triangle_frame, std::move(triangle_polygons));
		UASSERT(triangle_shape.has_value());
		constexpr int samples = 200;
		for (int i = 0; i <= samples; ++i)
			for (int j = 0; j <= samples - i; ++j) {
				const double u = double(i) / samples, v = double(j) / samples;
				const double w = 1.0 - u - v;
				const double x = triangle[0].first * w + triangle[1].first * u +
								 triangle[2].first * v;
				const double z = triangle[0].second * w + triangle[1].second * u +
								 triangle[2].second * v;
				UASSERT(triangle_shape->contains(int(std::floor(x)), int(std::floor(z))));
			}
		UASSERT(!triangle_shape->contains(49, 49));
		UASSERT(!triangle_shape->contains(0, 49));

		const auto remote = arnis::XZBBoxShape::create(
				frame, {{{{1e300, 1e300}, {2e300, 1e300}, {2e300, 2e300}}}});
		UASSERT(!remote.has_value());
	}

	void testOutlineRasterRejectsOutsideSpans()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {20, 20});
		std::string error;
		auto east = arnis::XZBBoxShape::create(
				rect, {{{{30, 2}, {35, 2}, {35, 8}, {30, 8}}}}, &error);
		UASSERT(!east.has_value());
		auto west = arnis::XZBBoxShape::create(
				rect, {{{{-15, 2}, {-10, 2}, {-10, 8}, {-15, 8}}}}, &error);
		UASSERT(!west.has_value());
	}

	void testWaterRingClipMatchesRustFastPaths()
	{
		const ::XZBBox bounds(0, 0, 10, 10);
		const std::vector<arnis::ProcessedNode> inside_open{
				{11, {}, 1, 1}, {12, {}, 8, 1}, {13, {}, 8, 8}};
		const auto untouched =
				arnis::clipping::clip_water_ring_to_bbox(inside_open, bounds);
		UASSERT(untouched.has_value());
		UASSERT(untouched->size() == inside_open.size());
		for (std::size_t i = 0; i < inside_open.size(); ++i) {
			UASSERT((*untouched)[i].id == inside_open[i].id);
			UASSERT((*untouched)[i].x == inside_open[i].x);
			UASSERT((*untouched)[i].z == inside_open[i].z);
		}

		const std::vector<arnis::ProcessedNode> crossing_open{
				{21, {}, 1, 1}, {22, {}, 8, 1}, {23, {}, 12, 8}};
		UASSERT(!arnis::clipping::clip_water_ring_to_bbox(crossing_open, bounds));
	}

	void testClippedRingPreservesSurvivingNodeIdsAndTags()
	{
		const ::XZBBox bounds(0, 0, 10, 10);
		const std::vector<arnis::ProcessedNode> ring{
				{101, {}, -5, 5},
				{102, {{"barrier", "gate"}}, 5, 5},
				{103, {}, 5, 15},
				{104, {}, -5, 15},
				{101, {}, -5, 5},
		};
		const auto clipped = arnis::clipping::clip_way_to_bbox(ring, bounds);
		UASSERT(clipped.size() >= 4);
		const auto kept = std::find_if(clipped.begin(), clipped.end(),
				[](const auto &node) { return node.x == 5 && node.z == 5; });
		UASSERT(kept != clipped.end());
		UASSERT(kept->id == 102);
		UASSERT(kept->tags.at("barrier") == "gate");
		UASSERT(clipped.front().id == clipped.back().id);
	}

	void testNetworkWaysAreCutAtTheOutline()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {20, 20});
		auto shape = arnis::XZBBoxShape::create(
				rect, {{{{0, 0}, {20, 0}, {20, 20}, {0, 20}}}});
		UASSERT(shape.has_value());
		const auto shared = std::make_shared<const arnis::XZBBoxShape>(std::move(*shape));
		const ::XZBBox world(0, 0, 20, 20);
		arnis::ProcessedWay way;
		way.id = 900;
		way.nodes = {{1, {}, -5, 5}, {2, {}, 10, 5}, {3, {}, 25, 5}};
		auto cut = arnis::clipping::cut_way_to_outline(way, world.with_outline(shared));
		UASSERT(cut.has_value());
		UASSERT(cut->size() == 1);
		UASSERT(cut->front().nodes.size() >= 2);
		UASSERT(arnis::clipping::is_invented_node_id(cut->front().nodes.front().id));
		UASSERT(arnis::clipping::is_invented_node_id(cut->front().nodes.back().id));
	}

	void testOutlineCutMatchesRustEdgeCases()
	{
		const auto make_world = [](int max_x, int max_z,
										arnis::XZBBoxShape::Polygons polygons) {
			const arnis::cartesian::XZBBoxRect rect({0, 0}, {max_x, max_z});
			auto shape = arnis::XZBBoxShape::create(rect, std::move(polygons));
			UASSERT(shape.has_value());
			return ::XZBBox(0, 0, max_x, max_z)
					.with_outline(std::make_shared<const arnis::XZBBoxShape>(
							std::move(*shape)));
		};
		const auto waver =
				make_world(200, 200, {{{{0, 0}, {100, 0}, {100, 200}, {0, 200}}}});
		std::vector<arnis::clipping::OutlinePoint> road;
		for (int k = 1; k < 40; ++k)
			road.emplace_back(k + 1, 100.5 + (k % 2 == 0 ? 1.5 : -1.5), 5.5 + k * 4.5);
		std::uint64_t cuts = 0;
		UASSERT(!arnis::clipping::cut_line_to_outline(3, road, waver, cuts));
		UASSERT(cuts == 0);

		const std::vector<arnis::clipping::OutlinePoint> graze{
				{1, 95.5, -8.5}, {2, 108.5, 4.5}};
		cuts = 0;
		const auto grazed = arnis::clipping::cut_line_to_outline(3, graze, waver, cuts);
		UASSERT(grazed.has_value() && grazed->empty());

		const std::vector<arnis::clipping::OutlinePoint> ends_outside{
				{1, 50.5, 50.5}, {2, 99.5, 50.5}, {3, 101.5, 50.5}, {4, 160.5, 50.5}};
		cuts = 0;
		const auto clipped =
				arnis::clipping::cut_line_to_outline(3, ends_outside, waver, cuts);
		UASSERT(clipped.has_value() && clipped->size() == 1);
		UASSERT(clipped->front().size() == 3);
		UASSERT(std::get<0>(clipped->front()[0]) == 1);
		UASSERT(std::get<0>(clipped->front()[1]) == 2);
		const auto end = clipped->front().back();
		UASSERT(arnis::clipping::is_invented_node_id(std::get<0>(end)));
		UASSERT(std::get<1>(end) > 99.5 && std::get<1>(end) < 101.0);

		arnis::XZBBoxShape::Polygons narrow_polygons{
				{{{0, 0}, {30, 0}, {30, 20}, {0, 20}}},
				{{{35, 0}, {60, 0}, {60, 20}, {35, 20}}}};
		const auto narrow_gap = make_world(60, 30, std::move(narrow_polygons));
		const std::vector<arnis::clipping::OutlinePoint> crossing{
				{1, 5.5, 10.5}, {2, 55.5, 10.5}};
		cuts = 0;
		const auto split =
				arnis::clipping::cut_line_to_outline(7, crossing, narrow_gap, cuts);
		UASSERT(split.has_value() && split->size() == 2);
		for (const auto &run : *split)
			for (const auto &[id, x, z] : run) {
				(void)id;
				UASSERT(narrow_gap.contains(static_cast<int>(std::floor(x)),
						static_cast<int>(std::floor(z))));
			}

		const std::vector<arnis::clipping::OutlinePoint> start_outside{
				{1, 45.5, 21.5}, {2, 45.5, 5.5}};
		cuts = 0;
		const auto entered =
				arnis::clipping::cut_line_to_outline(7, start_outside, narrow_gap, cuts);
		UASSERT(entered.has_value() && entered->size() == 1);
		UASSERT(arnis::clipping::is_invented_node_id(
				std::get<0>(entered->front().front())));
		UASSERT(std::abs(std::get<2>(entered->front().front()) - 21.0) < 0.01);
		UASSERT(std::get<0>(entered->front().back()) == 2);
	}

	void testOutlineCutSaturatesRemoteCoordinates()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {20, 20});
		auto shape = arnis::XZBBoxShape::create(
				rect, {{{{0, 0}, {20, 0}, {20, 20}, {0, 20}}}});
		UASSERT(shape.has_value());
		const ::XZBBox world(0, 0, 20, 20);
		const auto remote =
				static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1024.0;
		const std::vector<arnis::clipping::OutlinePoint> points{
				{1, remote, 5.5}, {2, remote, 10.5}};
		std::uint64_t cuts = 0;
		const auto runs = arnis::clipping::cut_line_to_outline(7, points,
				world.with_outline(
						std::make_shared<const arnis::XZBBoxShape>(std::move(*shape))),
				cuts);
		UASSERT(runs.has_value());
		UASSERT(runs->empty());
		UASSERT(cuts == 0);
	}

	void testShapeCropAndSpawnHelpers()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {20, 20});
		auto shape = arnis::XZBBoxShape::create(
				rect, {{{{0, 0}, {20, 0}, {20, 20}, {0, 20}},
							  {{7, 7}, {13, 7}, {13, 13}, {7, 13}}}});
		UASSERT(shape.has_value());
		auto bbox = arnis::cartesian::XZBBox::rect_from_min_max(0, 0, 20, 20)
							.with_outline(std::make_shared<const arnis::XZBBoxShape>(
									std::move(*shape)));
		const auto cropped = bbox.crop(0, 0, 5, 5);
		UASSERT(cropped.contains({2, 2}));
		UASSERT(!cropped.contains({8, 8}));
		UASSERT(cropped.outline()->covered_columns() == 36);
		const arnis::cartesian::XZBBoxRect crop_rect({0, 0}, {5, 5});
		const auto direct_crop = cropped.outline()->cropped(crop_rect);
		UASSERT(direct_crop.contains(2, 2));
		UASSERT(!direct_crop.contains(8, 2));
		UASSERT(!direct_crop.contains(2, 8));
		const auto default_spawn = cropped.default_spawn();
		UASSERT(cropped.contains({default_spawn.first, default_spawn.second}));
		const auto center_spawn = cropped.center_spawn();
		UASSERT(cropped.contains({center_spawn.first, center_spawn.second}));
		const auto rectangle =
				arnis::cartesian::XZBBox::rect_from_min_max(10, 10, 20, 20);
		UASSERT(rectangle.default_spawn() == std::pair(11, 11));
	}

	void testSpawnMustBeInsideSelectedShape()
	{
		arnis::Args conflicting;
		conflicting.bbox.emplace(0.0, 0.0, 1.0, 1.0);
		conflicting.polygon.push_back("0,0 1,0 0,1");
		std::string resolve_error;
		UASSERT(!conflicting.resolve_shapes(&resolve_error));
		UASSERT(resolve_error.find("cannot be combined with --bbox") !=
				std::string::npos);

		arnis::Args args;
		const auto shapes = arnis::LLShapes::from_geojson(
				R"({"type":"Polygon","coordinates":[[[0,0],[2,0],[0,2],[0,0]]]})");
		UASSERT(shapes.has_value());
		args.shapes = arnis::LLShapes::create(*shapes);
		UASSERT(args.shapes.has_value());
		args.bbox = args.shapes->bbox();
		args.spawn_lat = 1.5;
		args.spawn_lng = 1.5;
		const auto error = args.validation_error();
		UASSERT(error.has_value());
		UASSERT(error->find("inside the --polygon shapes") != std::string::npos);

		args.spawn_lat = 0.25;
		args.spawn_lng = 0.25;
		UASSERT(!args.validation_error().has_value());
	}

	void testBedrockGenerationCanUseParallelTiles()
	{
		UASSERT(arnis::should_use_parallel_tiles(3, true));
		UASSERT(arnis::should_use_parallel_tiles(3, false, true));
		UASSERT(!arnis::should_use_parallel_tiles(3, false));
		UASSERT(!arnis::should_use_parallel_tiles(2, false, true));
	}

	void testRotatingWorldAlsoRotatesOutline()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {100, 100});
		auto shape = arnis::XZBBoxShape::create(
				rect, {{{{0.3, 0.3}, {40.3, 0.3}, {40.3, 99.7}, {0.3, 99.7}}}});
		UASSERT(shape.has_value());
		auto bbox = arnis::cartesian::XZBBox::rect_from_min_max(0, 0, 100, 100)
							.with_outline(std::make_shared<const arnis::XZBBoxShape>(
									std::move(*shape)));
		std::vector<arnis::ProcessedElement> elements;
		arnis::map_transformation::rotate::rotate_world(90.0, elements, bbox);
		UASSERT(bbox.contains({50, 5}));
		UASSERT(!bbox.contains({50, 95}));
	}

	void testWorldEditorRejectsWritesOutsideOutline()
	{
		const arnis::cartesian::XZBBoxRect rect({0, 0}, {63, 63});
		auto shape = arnis::XZBBoxShape::create(rect, {{{{0, 0}, {0, 63}, {63, 63}}}});
		UASSERT(shape.has_value());

		arnis::WorldEditor editor;
		const arnis::Block test_block{static_cast<content_t>(42)};
		editor.enable_standalone_storage();
		editor.set_generation_bounds(0, 0, 63, 63);
		editor.set_generation_outline(
				std::make_shared<const arnis::XZBBoxShape>(std::move(*shape)));
		const std::pair<int, int> inside{5, 50};
		const std::pair<int, int> outside{30, 5};
		for (const auto &[x, z] : {inside, outside}) {
			editor.set_block_absolute(test_block, x, 1, z);
			editor.set_block_if_absent_absolute(test_block, x, 2, z);
			editor.fill_column_absolute(test_block, x, z, 3, 5, false);
			editor.fill_column_with_absolute(
					x, z, 6, 7, [test_block](int) { return test_block; }, false);
		}
		for (int y = 1; y <= 7; ++y) {
			UASSERT(editor.block_exists_absolute(inside.first, y, inside.second));
			UASSERT(!editor.block_exists_absolute(outside.first, y, outside.second));
		}
		{
			arnis::world_editor::ScopedFloorState restore(
					arnis::world_editor::floor_state_snapshot());
			arnis::world_editor::set_base_chunk_y(40);
			arnis::world_editor::set_base_chunk_block_id(
					static_cast<std::uint16_t>(test_block.id()));
			editor.set_filler_outside_outline(inside.first, inside.second);
			editor.set_filler_outside_outline(outside.first, outside.second);
			editor.set_filler_outside_outline(70, 5);
			UASSERT(!editor.block_exists_absolute(inside.first, 40, inside.second));
			UASSERT(editor.block_exists_absolute(outside.first, 40, outside.second));
			UASSERT(!editor.block_exists_absolute(70, 40, 5));
		}
	}

	void testTransparentPreviewPixelsStayClearOnLocalMaps()
	{
		arnis::decals::RgbaImage preview{
				192, 192, std::vector<std::uint8_t>(192 * 192 * 4, 0)};
		for (std::size_t i = 0; i < preview.pixels.size(); i += 4) {
			preview.pixels[i] = 0;
			preview.pixels[i + 1] = 0;
			preview.pixels[i + 2] = 255;
		}
		const arnis::decals::PreviewRaster raster{&preview, 904, 904, 1};
		const auto canvas = arnis::decals::render(
				arnis::decals::DecalKey{arnis::decals::LocalMapKey{1000, 1000}}, raster);
		UASSERT(canvas.get(30, 30) == arnis::decals::colors::LIGHT_GRAY);
	}
};

static TestFmArnisShapes g_test_fm_arnis_shapes;
