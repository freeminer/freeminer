#include "test.h"

#include "mapgen/earth/arnis-cpp/src/bedrock_addon/models.h"
#include "mapgen/earth/arnis-cpp/src/models_3d/voxelize.h"

#include <cmath>

class TestFmArnisBedrockModels : public TestBase
{
public:
	TestFmArnisBedrockModels() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmArnisBedrockModels"; }
	void runTests(IGameDef *) override
	{
		TEST(testVisibleBoundsOffsetUsesUnroundedHeight);
		TEST(testCubeCoordinatesMatchBedrockGeometryFrame);
		TEST(testBrakeLampCreatesDimAndEmissiveBones);
		TEST(testTiltUsesTheLowerCornerAsPivot);
		TEST(testModelTransformIdentityAndWorldYaw);
		TEST(testModelTransformIntrinsicYawAndScaleTranslation);
		TEST(testModelTransformNonuniformWorldScale);
	}

	void testVisibleBoundsOffsetUsesUnroundedHeight()
	{
		arnis::bedrock_addon::Shape shape;
		shape.block = "minecraft:white_concrete";
		shape.size = {1.0f, 0.5f, 1.0f};
		shape.corner = {0.0f, 0.25f, 0.0f};
		const auto geometry = arnis::bedrock_addon::models::geometry("test", {shape});
		const auto &description = geometry["minecraft:geometry"][0]["description"];

		// Rust rounds up the visible bounds height but computes the vertical offset
		// from the original top+1 value (1.75 / 2), not from the rounded bound (2 / 2).
		UASSERT(description["visible_bounds_height"].get<double>() == 2.0);
		UASSERT(std::abs(description["visible_bounds_offset"][1].get<double>() - 0.875) <
				1e-9);
	}

	void testCubeCoordinatesMatchBedrockGeometryFrame()
	{
		arnis::bedrock_addon::Shape shape;
		shape.block = "minecraft:white_concrete";
		shape.size = {1.0f, 0.25f, 0.1f};
		shape.corner = {-0.5f, 0.5f, 2.0f};
		shape.lamp = arnis::bedrock_addon::Lamp::On;
		const auto geometry = arnis::bedrock_addon::models::geometry("test", {shape});
		const auto &bones = geometry["minecraft:geometry"][0]["bones"];
		UASSERT(bones.size() == 1);
		UASSERT(bones[0]["name"] == "lit");
		const auto &cube = bones[0]["cubes"][0];
		UASSERT(cube["origin"] == nlohmann::json::array({-8.0, 8.0, -33.6}));
		UASSERT(cube["size"] == nlohmann::json::array({16.0, 4.0, 1.6}));
	}

	void testBrakeLampCreatesDimAndEmissiveBones()
	{
		arnis::bedrock_addon::Shape body;
		body.block = "minecraft:blue_concrete";
		body.size = {2.0f, 1.0f, 4.0f};
		body.corner = {-1.0f, 0.25f, -2.0f};
		arnis::bedrock_addon::Shape brake = body;
		brake.block = "minecraft:red_concrete";
		brake.lamp = arnis::bedrock_addon::Lamp::Brake;
		const auto geometry =
				arnis::bedrock_addon::models::geometry("car", {body, brake});
		const auto &bones = geometry["minecraft:geometry"][0]["bones"];
		UASSERT(bones.size() == 3);
		UASSERT(bones[0]["name"] == "body");
		UASSERT(bones[1]["name"] == "brake_off");
		UASSERT(bones[2]["name"] == "lit_brake");
		UASSERT(bones[1]["cubes"].size() == 1);
		UASSERT(bones[2]["cubes"].size() == 1);
	}

	void testTiltUsesTheLowerCornerAsPivot()
	{
		arnis::bedrock_addon::Shape shape;
		shape.block = "minecraft:tinted_glass";
		shape.size = {2.0f, 1.0f, 4.0f};
		shape.corner = {-1.0f, 0.25f, -2.0f};
		shape.tilt = -0.5f;
		const auto geometry = arnis::bedrock_addon::models::geometry("plate", {shape});
		const auto &cube = geometry["minecraft:geometry"][0]["bones"][0]["cubes"][0];
		UASSERT(cube["pivot"] == nlohmann::json::array({-16.0, 4.0, 32.0}));
		UASSERT(std::abs(cube["rotation"][0].get<double>() + 28.648) < 1e-9);
	}

	void testModelTransformIdentityAndWorldYaw()
	{
		using arnis::models_3d::WorldTransform;
		const auto identity =
				WorldTransform(0.0, 1.0, {0, 0, 0}, 1.0, 0.0, 0, 0, 0).apply({1, 2, 3});
		UASSERT(std::abs(identity[0] - 1.0f) < 1e-5f);
		UASSERT(std::abs(identity[1] - 2.0f) < 1e-5f);
		UASSERT(std::abs(identity[2] - 3.0f) < 1e-5f);

		const auto rotated =
				WorldTransform(0.0, 1.0, {0, 0, 0}, 1.0, 90.0, 0, 0, 0).apply({1, 0, 0});
		UASSERT(std::abs(rotated[0]) < 1e-5f);
		UASSERT(std::abs(rotated[2] - 1.0f) < 1e-5f);
	}

	void testModelTransformIntrinsicYawAndScaleTranslation()
	{
		using arnis::models_3d::WorldTransform;
		const auto rotated =
				WorldTransform(90.0, 1.0, {0, 0, 0}, 1.0, 0.0, 0, 0, 0).apply({1, 0, 0});
		UASSERT(std::abs(rotated[0]) < 1e-5f);
		UASSERT(std::abs(rotated[2] - 1.0f) < 1e-5f);

		const auto translated =
				WorldTransform(0.0, 2.0, {5, 0, 0}, 1.0, 0.0, 0, 0, 0).apply({1, 0, 0});
		UASSERT(std::abs(translated[0] - 7.0f) < 1e-5f);
		UASSERT(std::abs(translated[1]) < 1e-5f);
		UASSERT(std::abs(translated[2]) < 1e-5f);
	}

	void testModelTransformNonuniformWorldScale()
	{
		const auto transformed = arnis::models_3d::WorldTransform::with_world_scale_xyz(
				0.0, 1.0, {0, 0, 0}, {3.0f, 5.0f, 7.0f}, 0.0, 0, 0, 0)
										 .apply({1, 1, 1});
		UASSERT(std::abs(transformed[0] - 3.0f) < 1e-5f);
		UASSERT(std::abs(transformed[1] - 5.0f) < 1e-5f);
		UASSERT(std::abs(transformed[2] - 7.0f) < 1e-5f);
	}
};

static TestFmArnisBedrockModels g_test_fm_arnis_bedrock_models;
