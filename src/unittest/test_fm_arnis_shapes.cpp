#include "test.h"

#include "mapgen/earth/arnis-cpp/src/clipping.h"
#include "mapgen/earth/arnis-cpp/src/args.h"
#include "mapgen/earth/arnis-cpp/src/data_processing.h"
#include "mapgen/earth/arnis-cpp/src/decals/render.h"
#include "mapgen/earth/arnis-cpp/src/fm_ll_shapes.h"
#include "mapgen/earth/arnis-cpp/src/fm_xz_bbox_shape.h"
#include "mapgen/earth/arnis-cpp/src/map_transformation/rotate/rotator.h"

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
		TEST(testShapeCropAndSpawnHelpers);
		TEST(testSpawnMustBeInsideSelectedShape);
		TEST(testBedrockGenerationCanUseParallelTiles);
		TEST(testNetworkWaysAreCutAtTheOutline);
		TEST(testRotatingWorldAlsoRotatesOutline);
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
