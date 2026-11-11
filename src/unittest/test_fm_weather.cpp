// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"

#include "emerge.h"
#include "fm_weather_simulation.h"
#include "fm_weather_altitude.h"
#include "fm_weather_sync.h"
#include "network/fm_networkprotocol.h"
#include "util/msgpack_serialize.h"
#include "mapgen/mg_biome.h"
#include "mapgen/fm_earth_climate.h"
#include "mock_server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "util/directiontables.h"

#include <atomic>
#include <limits>
#include <future>
#include <thread>
#include <unordered_set>

class TestFmWeather : public TestBase
{
public:
	TestFmWeather() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmWeather"; }
	void runTests(IGameDef *) override
	{
		TEST(testFractionalEvolution);
		TEST(testMoisture);
		TEST(testBounds);
		TEST(testAltitudeTemperature);
		TEST(testAltitudePositionRange);
		TEST(testBiomeAltitude);
		TEST(testClock);
		TEST(testMonths);
		TEST(testEarthClimateFallback);
		TEST(testWindEvolution);
		TEST(testCalmAir);
		TEST(testWindSnapshots);
		TEST(testQueries);
		TEST(testScheduler);
		TEST(testWeatherDelivery);
		TEST(testWeatherPackets);
	}

	void testEarthClimateFallback()
	{
		using namespace fm_earth_climate;
		UASSERT(!valid_pixel(0, 0, 0, 255, true));
		UASSERT(valid_pixel(0, 0, 0, 255, false));
		UASSERT(!valid_pixel(100, 100, 100, 0, false));
		UASSERT(valid_pixel(1, 1, 1, 255, true));
		UASSERT(temperature(0, 0, 365, true) == 27.0f);
		UASSERT(temperature(90, 0, 365, false) < 0);
		UASSERT(temperature(45, 0, 365, true) != temperature(-45, 0, 365, true));
		UASSERT(humidity(0) == 75.0f);
	}

	void testFractionalEvolution()
	{
		std::array<weather::Neighbor, 6> neighbors{};
		weather::Sample state{20.0f, 50.0f};
		for (unsigned i = 0; i < 1000; ++i)
			state = weather::integrate(state, neighbors, 25.0f, 50.0f, nullptr, 1.0f);
		UASSERT(state.heat > 24.9f && state.heat < 25.01f);
		UASSERTEQ(weather::heat_t, weather::heat_value(state.heat), 25);
	}

	void testMoisture()
	{
		std::array<weather::Neighbor, 6> neighbors{};
		weather::Sample state{20.0f, 50.0f};
		// The warmer target contains the same vapor as the original air.
		const float water = weather::moisture(state.heat, state.humidity);
		const float warm_humidity = 100.0f * water / weather::saturation(30.0f);
		for (unsigned i = 0; i < 100; ++i)
			state = weather::integrate(
					state, neighbors, 30.0f, warm_humidity, nullptr, 1.0f);
		UASSERT(state.heat > 20.0f && state.humidity < 50.0f);
		UASSERT(std::abs(weather::moisture(state.heat, state.humidity) - water) <
				0.0001f);
		// Equal RH at different temperatures does not mean equal moisture.
		neighbors[0] = {{30.0f, 50.0f}, {1.0f, 0.0f, 0.0f}, true};
		state = {20.0f, 50.0f};
		const auto result =
				weather::integrate(state, neighbors, 20.0f, 50.0f, nullptr, 1.0f);
		UASSERT(weather::moisture(result.heat, result.humidity) > water);
	}

	void testBounds()
	{
		UASSERTEQ(weather::humidity_t, weather::humidity_value(-10), 0);
		UASSERTEQ(weather::humidity_t, weather::humidity_value(101), 100);
		UASSERTEQ(weather::heat_t, weather::heat_value(6000), 6000);
		UASSERTEQ(weather::heat_t, weather::heat_value(100000), 32767);
		UASSERTEQ(weather::humidity_t,
				weather::humidity_value(std::numeric_limits<float>::quiet_NaN()), 0);
		UASSERT(weather::limit_wind({100, 0, 0}).getLength() <= 8.001f);
		UASSERT(weather::limit_wind({std::numeric_limits<float>::infinity(), 0, 0})
						.getLengthSQ() == 0);
	}

	void testAltitudeTemperature()
	{
		constexpr double limit = WEATHER_LIMIT;
		for (const bool stepped : {false, true}) {
			const auto profile = [&](double height) {
				return weather::altitude_heat(
						height, -333, 1000, -40, 6000, limit, stepped);
			};
			const double warm = (stepped ? std::trunc(limit / 333) : limit / 333) + 6000;
			for (const double height :
					{-limit, -30500.0, -30007.0, -1000.0, 0.0, 332.0, 1000.0, limit}) {
				double legacy = height / -333;
				if (stepped)
					legacy = std::trunc(legacy);
				if (height < -(limit - 1000))
					legacy += 6000 * (1 - (height + limit) / 1000);
				UASSERT(std::abs(profile(height) - legacy) < 0.000001);
			}
			UASSERT(std::abs(profile(-limit) - warm) < 0.000001);
			UASSERT(std::abs(profile(-2 * limit) + 40) < 0.000001);
			UASSERT(std::abs(profile(-3 * limit) - warm) < 0.000001);
			UASSERT(std::abs(profile(-4 * limit) + 40) < 0.000001);
			UASSERT(std::abs(profile(-1.5 * limit) - (warm - 40) / 2) < 0.000001);
			UASSERT(std::abs(profile(-2.5 * limit) - (warm - 40) / 2) < 0.000001);
			UASSERT(std::abs(profile(-limit + 0.000001) - warm) < 0.00001);
			UASSERT(std::abs(profile(-limit - 0.000001) - warm) < 0.00001);
			for (unsigned i = 1; i <= 100; ++i) {
				const double fraction = i / 100.0;
				UASSERT(profile(-limit - fraction * limit) <=
						profile(-limit - (fraction - 0.01) * limit) + 0.000001);
				UASSERT(profile(-2 * limit - fraction * limit) >=
						profile(-2 * limit - (fraction - 0.01) * limit) - 0.000001);
			}
		}
		UASSERT(weather::altitude_heat(-2 * limit, -333, 1000, -100, 7000, limit) ==
				-100);
		UASSERT(std::abs(weather::altitude_heat(
								 -3 * limit, -333, 1000, -100, 7000, limit) -
						 (limit / 333 + 7000)) < 0.000001);
		UASSERT(weather::altitude_heat(1000, 0, 0, -40, 6000, limit) == 0);
		UASSERT(weather::altitude_heat(-2 * limit, 0, 0, 10, 6000, limit) ==
				0); // Inverted bounds.
		UASSERT(weather::altitude_heat(-3 * limit, 0, 0, -40, 6000, limit) ==
				0); // Disabled core.
		UASSERT(weather::altitude_heat(-limit, -333, 1000, -40, 6000, 0) == 0);
	}

	void testAltitudePositionRange()
	{
		// Exercise the full 32-bit coordinate range even in a 16-bit engine build.
		constexpr double low = std::numeric_limits<int32_t>::min();
		constexpr double high = std::numeric_limits<int32_t>::max();
		const auto profile = [](double height) {
			return weather::altitude_heat(height, -333, 1000, -40, 6000, WEATHER_LIMIT);
		};
		for (const double height : {low, low + 1, -2000000000.0, -1000000.0}) {
			const double heat = profile(height);
			UASSERT(std::isfinite(heat) && heat >= -40 &&
					heat <= 6000 + WEATHER_LIMIT / 333.0);
			UASSERT(std::abs(heat - profile(height + 2.0 * WEATHER_LIMIT)) < 0.000001);
		}
		UASSERT(std::isfinite(profile(high)));
		UASSERTEQ(weather::heat_t, weather::heat_value(profile(high)), -32768);
		UASSERT(weather::relative_height(high, high - 1) == 1);
		UASSERT(weather::relative_height(low + 1, low) == 1);
		UASSERT(weather::relative_height(high, low) == 4294967295.0);
		UASSERT(weather::relative_height(low, high) == -4294967295.0);
	}

	void testBiomeAltitude()
	{
		MockServer server(getTestTempDirectory());
		auto params = std::unique_ptr<MapgenParams>(
				Mapgen::createMapgenParams(MAPGEN_SINGLENODE));
		params->bparams = new BiomeParamsOriginal();
		params->bparams->np_heat.offset = 20;
		params->bparams->np_heat.scale = 0;
		BiomeManager manager(&server);
		manager.mapgen_params = params.get();
		manager.weather_heat_height = -333;
		manager.weather_hot_core = 1000;
		manager.weather_heat_deep_min = -40;
		manager.weather_hot_core_heat = 6000;
		const auto heat = [&](pos_t height) {
			return manager.calcBlockHeat(v3pos_t(0, height, 0), 0, 0, 0, false);
		};
		UASSERTEQ(weather::heat_t, heat(0), 20);
		UASSERTEQ(weather::heat_t, heat(332), 20);
		UASSERTEQ(weather::heat_t, heat(1000), 17);
		UASSERTEQ(weather::heat_t, heat(-WEATHER_LIMIT), 6113);
		if (double(std::numeric_limits<pos_t>::min()) <= -4.0 * WEATHER_LIMIT) {
			UASSERTEQ(
					weather::heat_t, heat(static_cast<pos_t>(-2.0 * WEATHER_LIMIT)), -20);
			UASSERTEQ(weather::heat_t, heat(static_cast<pos_t>(-3.0 * WEATHER_LIMIT)),
					6113);
			UASSERTEQ(
					weather::heat_t, heat(static_cast<pos_t>(-4.0 * WEATHER_LIMIT)), -20);
		}
		manager.weather_heat_deep_min = -100;
		manager.weather_hot_core_heat = 7000;
		auto clone = std::unique_ptr<BiomeManager>(manager.clone());
		UASSERT(clone->weather_heat_deep_min == -100);
		UASSERT(clone->weather_hot_core_heat == 7000);
		UASSERTEQ(weather::heat_t,
				clone->calcBlockHeat(v3pos_t(0, -WEATHER_LIMIT, 0), 0, 0, 0, false),
				7113);
		manager.weather_heat_height = 0;
		manager.weather_hot_core = 0;
		UASSERTEQ(weather::heat_t, heat(1000), 20);
	}

	void testClock()
	{
		UASSERTEQ(uint32_t, weather::step_seconds(10, 10), 0);
		UASSERTEQ(uint32_t, weather::step_seconds(20, 10), 10);
		// Bounded work retains debt in last_step rather than replacing it with now.
		uint32_t last = 10;
		last += weather::step_seconds(45, last);
		UASSERTEQ(uint32_t, last, 30);
		last += weather::step_seconds(45, last);
		UASSERTEQ(uint32_t, last, 45);
		UASSERTEQ(uint32_t, weather::step_seconds(2, UINT32_MAX - 2), 5);
		UASSERTEQ(uint32_t, weather::step_seconds(10, 20), 0);
	}

	void testMonths()
	{
		const float year = 86400.0f * 365.0f;
		UASSERT(weather::month_phase(0, 365) == 0.0f);
		UASSERT(weather::month_phase(year, 365) == 0.0f);
		UASSERT(std::abs(weather::month_phase(-year / 24, 365) - 11.5f) < 0.0001f);
		UASSERT(std::abs(*weather::blend_monthly(0, 20, 5, 0.5f) - 10) < 0.0001f);
		UASSERT(std::abs(*weather::blend_monthly(20, 0, 5, 0.9999f)) < 0.01f);
		UASSERT(*weather::blend_monthly(std::nullopt, 20, 0, 0.5f) == 10);
		UASSERT(*weather::blend_monthly(std::nullopt, 20, std::nullopt, 0.5f) == 20);
		UASSERT(!weather::blend_monthly(std::nullopt, std::nullopt, std::nullopt, 0.5f));
	}

	void testWindEvolution()
	{
		const weather::wind_t prevailing(2.0f, 0.0f, -1.0f);
		const weather::wind_t gradient(0.4f, -0.2f, -0.5f);
		const weather::wind_t initial(1.0f, 0.3f, -0.7f);
		const auto whole = weather::advance_wind(initial, gradient, &prevailing, 10.0f);
		auto divided = initial;
		for (unsigned i = 0; i < 10; ++i)
			divided = weather::advance_wind(divided, gradient, &prevailing, 1.0f);
		UASSERT((whole - divided).getLength() < 0.00001f);
		auto wind = prevailing;
		for (unsigned i = 0; i < 100; ++i)
			wind = weather::advance_wind(wind, {}, &prevailing, 1.0f);
		UASSERT((wind - prevailing).getLength() < 0.00001f);
		UASSERT(weather::advance_wind(initial, gradient, &prevailing, 0.0f) == initial);
		UASSERT(weather::advance_wind(initial, gradient, &prevailing, -1.0f) == initial);
	}

	void testCalmAir()
	{
		std::array<weather::Neighbor, 6> neighbors{};
		const weather::Sample uniform{20.0f, 50.0f};
		for (size_t i = 0; i < neighbors.size(); ++i) {
			const auto &dir = g_6dirs[i];
			neighbors[i] = {uniform, weather::wind_t(dir.X, dir.Y, dir.Z)};
		}
		auto state = uniform;
		for (unsigned i = 0; i < 100; ++i)
			state = weather::integrate(state, neighbors, 20.0f, 50.0f, nullptr, 1.0f);
		UASSERT(state.wind.getLengthSQ() == 0);
		UASSERT(state.heat == uniform.heat);
		UASSERT(std::abs(state.humidity - uniform.humidity) < 0.0001f);
		UASSERT(std::abs(neighbors[0].vapor() - weather::moisture(20, 50)) < 0.00001f);
	}

	void testWindSnapshots()
	{
		weather::Wind wind;
		wind = weather::wind_t(1, 2, 3);
		std::atomic_bool done{false}, coherent{true};
		std::thread writer([&] {
			for (unsigned i = 0; i < 10000; ++i) {
				const float sign = i % 2 ? 1.0f : -1.0f;
				wind = weather::wind_t(sign, 2 * sign, 3 * sign);
			}
			done = true;
		});
		do {
			const auto value = wind.load();
			if (value.Y != 2 * value.X || value.Z != 3 * value.X)
				coherent = false;
		} while (!done);
		writer.join();
		UASSERT(coherent);
	}

	void testQueries()
	{
		MockServer server(getTestTempDirectory());
		MetricsBackend metrics;
		EmergeManager emerge(&server, &metrics);
		auto map = std::make_unique<ServerMap>(
				server.getWorldPath(), &server, &emerge, &metrics);
		ServerEnvironment env(std::move(map), &server, &metrics);
		env.m_use_weather = true;
		auto &weather_map = env.getServerMap();
		const v3pos_t pos(0, 0, 0);
		auto block = weather_map.createBlock(getNodeBlockPos(pos));
		block->heat = 20;
		block->heat_add = 5;
		block->humidity = 95;
		block->humidity_add = 10;
		// Old publication timestamps must not reset simulated state to mapgen.
		block->heat_last_update = block->humidity_last_update = 1;
		block->wind = weather::wind_t(1, 2, 3);
		block->weather_state.wind_initialized = true;
		unordered_map_v3bpos<weather::heat_t> heat_cache;
		unordered_map_v3bpos<weather::humidity_t> humidity_cache;
		heat_cache[getNodeBlockPos(pos)] = -50;
		humidity_cache[getNodeBlockPos(pos)] = -50;
		for (unsigned i = 0; i < 100; ++i) {
			UASSERTEQ(weather::heat_t, weather_map.updateBlockHeat(&env, pos), 25);
			UASSERTEQ(
					weather::humidity_t, weather_map.updateBlockHumidity(&env, pos), 100);
			UASSERTEQ(weather::heat_t,
					weather_map.updateBlockHeat(&env, pos, block.get()), 25);
			UASSERTEQ(weather::humidity_t,
					weather_map.updateBlockHumidity(&env, pos, block.get()), 100);
			UASSERTEQ(weather::heat_t,
					weather_map.updateBlockHeat(&env, pos, nullptr, &heat_cache, false),
					20);
			UASSERTEQ(weather::humidity_t,
					weather_map.updateBlockHumidity(
							&env, pos, nullptr, &humidity_cache, false),
					95);
			UASSERT(weather_map.updateBlockWind(&env, pos) == weather::wind_t(1, 2, 3));
			UASSERT(weather_map.getHumidity(pos) <= 100);
		}
		UASSERTEQ(short, block->heat_add.load(), 5);
		UASSERTEQ(short, block->humidity_add.load(), 10);
		// The cache stores ambient values even when initialization first asks
		// for a result without node contributions.
		const v3pos_t fresh(MAP_BLOCKSIZE, 0, 0);
		auto fresh_block = weather_map.createBlock(getNodeBlockPos(fresh));
		fresh_block->heat_add = 5;
		fresh_block->humidity_add = 10;
		heat_cache[getNodeBlockPos(fresh)] = 20;
		humidity_cache[getNodeBlockPos(fresh)] = 95;
		UASSERTEQ(weather::heat_t,
				weather_map.updateBlockHeat(&env, fresh, nullptr, &heat_cache, false),
				20);
		UASSERTEQ(weather::humidity_t,
				weather_map.updateBlockHumidity(
						&env, fresh, nullptr, &humidity_cache, false),
				95);
		UASSERTEQ(weather::heat_t, weather_map.updateBlockHeat(&env, fresh), 25);
		UASSERTEQ(weather::humidity_t, weather_map.updateBlockHumidity(&env, fresh), 100);
		// A query before mapgen initialization must not mark zero as valid
		// climate or seed a permanent zero into the caller's cache.
		const v3pos_t early(MAP_BLOCKSIZE * 2, 0, 0);
		auto early_block = weather_map.createBlock(getNodeBlockPos(early));
		UASSERTEQ(weather::heat_t,
				weather_map.updateBlockHeat(&env, early, nullptr, &heat_cache), 0);
		UASSERTEQ(weather::humidity_t,
				weather_map.updateBlockHumidity(&env, early, nullptr, &humidity_cache),
				0);
		UASSERT(!early_block->heat_last_update && !early_block->humidity_last_update);
		UASSERT(!heat_cache.count(getNodeBlockPos(early)) &&
				!humidity_cache.count(getNodeBlockPos(early)));
		heat_cache[getNodeBlockPos(early)] = 23;
		humidity_cache[getNodeBlockPos(early)] = 54;
		UASSERTEQ(weather::heat_t,
				weather_map.updateBlockHeat(&env, early, nullptr, &heat_cache), 23);
		UASSERTEQ(weather::humidity_t,
				weather_map.updateBlockHumidity(&env, early, nullptr, &humidity_cache),
				54);
		// Cached fallback sampling is deterministic and bounded, too.
		const v3pos_t missing(MAP_BLOCKSIZE * 100, 0, 0);
		humidity_cache[getNodeBlockPos(missing)] = 101;
		UASSERTEQ(weather::humidity_t,
				weather_map.updateBlockHumidity(&env, missing, nullptr, &humidity_cache),
				100);
		// The generic block cache is thread-local and can hold another map's
		// block at the same coordinates. Weather must resolve its own map.
		{
			Map other_map(&server);
			auto other = other_map.createBlankBlock(getNodeBlockPos(pos));
			other->heat = -11;
			other->humidity = 17;
			other_map.getBlock(getNodeBlockPos(pos));
			UASSERTEQ(weather::heat_t, weather_map.updateBlockHeat(&env, pos), 25);
			UASSERTEQ(
					weather::humidity_t, weather_map.updateBlockHumidity(&env, pos), 100);
			UASSERTEQ(weather::heat_t, weather_map.getHeat(pos, true), 25);
			UASSERTEQ(weather::humidity_t, weather_map.getHumidity(pos, true), 100);
			UASSERT(weather_map.getWind(pos) == weather::wind_t(1, 2, 3));
			other_map.getBlockCacheFlush();
		}
		env.m_use_weather = false;
		UASSERT(weather_map.updateBlockWind(&env, pos) == weather::wind_t{});
		UASSERT(block->wind.load() == weather::wind_t{});
		env.deactivateBlocksAndObjects();
	}
	void testScheduler()
	{
		MockServer server(getTestTempDirectory());
		MetricsBackend metrics;

		EmergeManager emerge(&server, &metrics);
		auto map = std::make_unique<ServerMap>(
				server.getWorldPath(), &server, &emerge, &metrics);
		map->settings_mgr.setMapSetting("mg_name", "singlenode", true);
		auto *params = map->settings_mgr.makeMapgenParams();
		UASSERT(params && params->bparams);
		params->bparams->np_heat.offset = 40.0f;
		params->bparams->np_heat.scale = 0.0f;
		ServerEnvironment env(std::move(map), &server, &metrics);
		env.m_use_weather = true;
		emerge.env = &env;
		emerge.initMapgens(params);
		auto &weather_map = env.getServerMap();
		// Initial wind must also stay calm in uniform air, independent of
		// absolute humidity and the altitude separation of neighboring blocks.
		const v3pos_t calm_pos(MAP_BLOCKSIZE * 100, 0, 0);
		const auto initialize_calm = [&](const v3pos_t &node_pos) {
			auto calm = weather_map.createBlock(getNodeBlockPos(node_pos));
			calm->heat = 20;
			calm->humidity = 50;
			calm->heat_last_update = calm->humidity_last_update = 1;
		};
		initialize_calm(calm_pos);
		for (const auto &dir : g_6dirs)
			initialize_calm(calm_pos + dir * MAP_BLOCKSIZE);
		UASSERT(weather_map.updateBlockWind(&env, calm_pos).getLengthSQ() == 0);

		const v3pos_t pos(0, 0, 0);
		auto block = weather_map.createBlock(getNodeBlockPos(pos));
		block->setGenerated(true);
		block->heat = 20;
		block->humidity = 50;
		block->heat_add = 5;
		block->humidity_add = 7;
		block->heat_last_update = block->humidity_last_update = 1;
		block->weather_state.heat = 20.0f;
		block->weather_state.humidity = 50.0f;
		block->weather_state.initialized = true;
		block->weather_state.wind_initialized = true;
		block->wind = weather::wind_t(1, 0, 0);
		// Game time is zero in this environment. Use wrapping timestamps to
		// exercise actual ten-second integration without stepping Lua or ABMs.
		block->weather_state.last_step = env.getGameTime() - 10;
		UASSERTEQ(u32, weather_map.stepLoadedBlockWeather(&env, 10, 1000), 0);
		const float climate = emerge.getFirstMapgen()->calcBlockHeat(
				pos, weather_map.getSeed(), env.getTimeOfDayF(), 0.0f, true);
		const float expected_heat = 20.0f + (climate - 20.0f) * (1.0f - std::exp(-0.06f));
		UASSERT(std::abs(block->weather_state.heat - expected_heat) < 0.0001f);
		UASSERTEQ(u32, block->weather_state.last_step, env.getGameTime());
		UASSERTEQ(short, block->heat_add.load(), 5);
		UASSERTEQ(short, block->humidity_add.load(), 7);
		UASSERT(std::abs(block->wind.load().X - std::exp(-0.8f)) < 0.00001f);
		const float heat = block->weather_state.heat;
		weather_map.stepLoadedBlockWeather(&env, 10, 1000);
		UASSERT(block->weather_state.heat == heat);

		block->weather_state.last_step = env.getGameTime() - 40;
		weather_map.stepLoadedBlockWeather(&env, 10, 1000);
		UASSERTEQ(u32, env.getGameTime() - block->weather_state.last_step, 20);
		weather_map.stepLoadedBlockWeather(&env, 10, 1000);
		UASSERTEQ(u32, block->weather_state.last_step, env.getGameTime());

		block->weather_state.last_step = env.getGameTime() - 10;
		std::promise<void> locked, release;
		auto released = release.get_future();
		std::thread holder([&] {
			std::lock_guard<std::mutex> lock(block->weather_state.mutex);
			locked.set_value();
			released.wait();
		});
		locked.get_future().wait();
		weather_map.stepLoadedBlockWeather(&env, 10, 1000);
		release.set_value();
		holder.join();
		UASSERTEQ(u32, env.getGameTime() - block->weather_state.last_step, 10);
		weather_map.stepLoadedBlockWeather(&env, 10, 1000);
		UASSERTEQ(u32, block->weather_state.last_step, env.getGameTime());
		env.deactivateBlocksAndObjects();
	}
	void testWeatherDelivery()
	{
		UASSERT(CLIENT_PROTOCOL_VERSION_FM >= weather::sync_protocol_version);
		weather::ClientSync sync;
		const bpos_t large = std::numeric_limits<bpos_t>::max() / 2;
		const v3bpos_t center(large, 0, -large);
		const weather::Snapshot value{20, 50, {1, 0, 0}};
		UASSERT(sync.begin(center, 2, 100));
		std::unordered_set<v3bpos_t> positions;
		const auto first = sync.next(100);
		UASSERT(first && *first == center);
		positions.insert(*first);
		sync.sent(*first, value);
		while (const auto pos = sync.next(100)) {
			UASSERT(positions.insert(*pos).second);
			sync.sent(*pos, value);
		}
		UASSERTEQ(size_t, positions.size(), 125);
		UASSERTEQ(size_t, sync.size(), 125);
		UASSERT(!sync.begin(center, 2, 2099));
		UASSERT(sync.begin(center, 2, 2100));
		UASSERT(!sync.changed(center, value));
		auto changed = value;
		changed.wind.X += 0.05f;
		UASSERT(!sync.changed(center, changed));
		changed.wind.X += 0.07f;
		UASSERT(sync.changed(center, changed)); // Small changes accumulate.
		changed = value;
		++changed.humidity;
		UASSERT(sync.changed(center, changed));
		changed = value;
		++changed.heat;
		UASSERT(sync.changed(center, changed));
		sync.forget(center);
		UASSERT(sync.changed(center, value));
		sync.sent(center, value);
		UASSERT(sync.begin(center + v3bpos_t(1, 0, 0), 2, 2101));
		UASSERT(!sync.changed(center, value)); // Preserve overlapping weather.
		UASSERTEQ(size_t, sync.size(), 100);
		UASSERT(sync.begin(center, 1, 2102));
		UASSERTEQ(size_t, sync.size(), 27);
		sync.sent(center + v3bpos_t(2, 0, 0), value);
		UASSERTEQ(size_t, sync.size(), 27); // Cache remains bounded.
		UASSERT(sync.begin(-center, 1, 2103));
		UASSERTEQ(size_t, sync.size(), 0);
		UASSERT(sync.changed(-center, value));
		UASSERT(sync.begin(center, std::numeric_limits<pos_t>::max(), 2104));
		positions.clear();
		while (const auto pos = sync.next(2104))
			positions.insert(*pos);
		UASSERTEQ(size_t, positions.size(), 17 * 17 * 17); // Hard radius cap.
	}

	void testWeatherPackets()
	{
		MockServer server(getTestTempDirectory());
		Map map(&server);
		const v3bpos_t pos(10, 0, -10);
		auto block = map.createBlankBlock(pos);
		block->fill(MapNode(CONTENT_AIR));
		block->heat = 20;
		block->humidity = 95;
		block->heat_add = 5;
		block->humidity_add = 10;
		block->wind = weather::wind_t(1, 0, 0);
		const auto original = weather::snapshot(*block);
		UASSERTEQ(weather::heat_t, original.heat, 25);
		UASSERTEQ(weather::humidity_t, original.humidity, 100);
		const auto revision = block->far_make_mesh_timestamp.load();
		const auto changed_timestamp = block->m_changed_timestamp.load();
		const auto count = map.m_blocks.size();
		const weather::Snapshot value{12, 80, {2, 0.5f, -1}};
		const std::string encoded = weather::encode_updates({{pos, value}});
		msgpack::unpacked unpacked;
		msgpack::unpack(unpacked, encoded.data(), encoded.size());
		const auto objects = unpacked.get().as<std::vector<msgpack::object>>();
		UASSERTEQ(size_t, objects.size(), 1);
		const auto fields = objects.front().as<MsgpackPacket>();
		MsgpackPacketSafe packet;
		packet.insert(fields.begin(), fields.end());
		UASSERT(!packet.contains(TOCLIENT_BLOCKDATA_DATA));
		UASSERT(weather::apply_update(map, packet));
		UASSERTEQ(short, block->heat.load(), 12);
		UASSERTEQ(short, block->humidity.load(), 80);
		UASSERTEQ(short, block->heat_add.load(), 0);
		UASSERTEQ(short, block->humidity_add.load(), 0);
		UASSERT(block->wind.load() == value.wind);
		UASSERTEQ(u32, block->far_make_mesh_timestamp.load(), revision);
		UASSERTEQ(u32, block->m_changed_timestamp.load(), changed_timestamp);
		UASSERTEQ(content_t, block->getNodeNoCheck({0, 0, 0}).getContent(), CONTENT_AIR);

		// Missing required fields and invalid types change none of the weather.
		packet.erase(TOCLIENT_BLOCKDATA_WIND);
		UASSERT(weather::apply_update(map, packet));
		UASSERT(!weather::snapshot(*block).differs(value));
		packet[TOCLIENT_BLOCKDATA_WIND] = fields.at(TOCLIENT_BLOCKDATA_WEATHER_ONLY);
		UASSERT(weather::apply_update(map, packet));
		UASSERT(!weather::snapshot(*block).differs(value));

		const std::string unknown =
				weather::encode_updates({{pos + v3bpos_t(1, 0, 0), value}});
		msgpack::unpacked unknown_unpacked;
		msgpack::unpack(unknown_unpacked, unknown.data(), unknown.size());
		const auto unknown_fields =
				unknown_unpacked.get().as<std::vector<MsgpackPacket>>().front();
		MsgpackPacketSafe unknown_packet;
		unknown_packet.insert(unknown_fields.begin(), unknown_fields.end());
		UASSERT(weather::apply_update(map, unknown_packet));
		UASSERTEQ(size_t, map.m_blocks.size(), count); // Never create mapblocks.

		const std::string invalid_bounds =
				weather::encode_updates({{pos, {30, 200, {100, 0, 0}}}});
		msgpack::unpacked bounds_unpacked;
		msgpack::unpack(bounds_unpacked, invalid_bounds.data(), invalid_bounds.size());
		const auto bounds_fields =
				bounds_unpacked.get().as<std::vector<MsgpackPacket>>().front();
		MsgpackPacketSafe bounds_packet;
		bounds_packet.insert(bounds_fields.begin(), bounds_fields.end());
		UASSERT(weather::apply_update(map, bounds_packet));
		UASSERTEQ(short, block->humidity.load(), 100);
		UASSERT(block->wind.load().getLength() <= 8.001f);
		MsgpackPacketSafe legacy;
		UASSERT(!weather::apply_update(map, legacy));
		map.getBlockCacheFlush();
	}
};

static TestFmWeather g_test_instance;
