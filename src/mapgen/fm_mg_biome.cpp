// Shared, overflow-safe altitude temperature profile.
#include "mapgen/mapgen.h"
#include "mg_biome.h"
#include "fm_weather_altitude.h"
#include "fm_weather_simulation.h"
#include "util/numeric.h"

namespace
{

float biome_smoothstep(float edge0, float edge1, float x)
{
	if (edge0 == edge1)
		return x < edge0 ? 0.0f : 1.0f;

	x = rangelim((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return x * x * (3.0f - 2.0f * x);
}

float biome_cycle_distance(float value, float center)
{
	value -= std::floor(value);
	center -= std::floor(center);
	const float distance = std::abs(value - center);
	return std::min(distance, 1.0f - distance);
}

}

weather::heat_t BiomeManager::calcBlockHeat(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
	//variant 1: full random
	//f32 heat = NoisePerlin3D(np_heat, p.X, env->getGameTime()/100, p.Z, seed);

	//variant 2: season change based on default heat map
	auto heat = NoiseFractal2D(
			&(mapgen_params->bparams->np_heat), p.X, p.Z, seed); // -30..20..70
	// auto heat =calcHeatAtPoint(p);

	if (use_weather) {
		f32 seasonv = totaltime;
		seasonv /= 86400 * year_days; // season change speed
		seasonv +=
				(f32)p.X / weather_heat_width; // you can walk to area with other season
		seasonv = sin(seasonv * M_PI);
		//heat += (weather_heat_season * (heat < offset ? 2 : 0.5)) * seasonv; // -60..0..30
		heat += (weather_heat_season)*seasonv; // -60..0..30

		// daily change, hotter at sun +4, colder at night -4
		heat += weather_heat_daily *
				(sin(cycle_shift(timeofday, -0.25) * M_PI) - 0.5); //-64..0..34
	}
	// Preserve shallow weather and repeat bounded deep thermal layers.
	heat += weather::altitude_heat(static_cast<double>(p.Y), weather_heat_height,
			weather_hot_core, weather_heat_deep_min, weather_hot_core_heat, WEATHER_LIMIT,
			true);
	// Preserve the legacy truncation while bounding the int16_t conversion.
	return weather::heat_value(std::trunc(heat));
	// ===
}

weather::humidity_t BiomeManager::calcBlockHumidity(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather)
{
	return calcBlockHumidity(p, seed, timeofday, totaltime, use_weather, 0);
}

weather::humidity_t BiomeManager::calcBlockHumidity(const v3pos_t &p, uint64_t seed,
		float timeofday, float totaltime, bool use_weather, pos_t surface_y)
{

	auto humidity =
			NoiseFractal2D(&(mapgen_params->bparams->np_humidity), p.X, p.Z, seed);
	// auto humidity = calcHumidityAtPoint(p);

	if (use_weather) {
		f32 seasonv = totaltime;
		seasonv /= 86400 * weather_humidity_days; // bad weather change speed (2 days)
		seasonv += (f32)p.Z / weather_humidity_width;
		humidity += weather_humidity_season * sin(seasonv * M_PI);
		humidity +=
				weather_humidity_daily * (sin(cycle_shift(timeofday, -0.1) * M_PI) - 0.5);
	}

	// Subtract in double before reducing the local height to float.
	const double y = static_cast<double>(p.Y);
	const float height_above_surface = static_cast<float>(
			weather::relative_height(y, static_cast<double>(surface_y)));
	const float humidity_height = static_cast<float>(weather_humidity_height);
	const float height_scale = std::max(64.0f, std::abs(humidity_height));
	const float cloud_y = static_cast<float>(cloud_height);
	const float cloud_base = cloud_y - std::max(80.0f, height_scale * 0.35f);
	const float cloud_top = cloud_y + std::max(180.0f, height_scale * 1.10f);

	// Keep the old vertical humidity bias, but cap it so extreme heights do not
	// dominate the climate map.
	if (humidity_height != 0.0f) {
		const float height_bias = y / humidity_height;
		humidity += rangelim(height_bias, -22.0f, 32.0f) * 0.45f;
	}

	const float low_air = 1.0f - biome_smoothstep(-height_scale * 2.0f, cloud_base, y);
	const float cloud_band = biome_smoothstep(cloud_base, cloud_y, y) *
							 (1.0f - biome_smoothstep(cloud_y, cloud_top, y));
	const float high_air =
			biome_smoothstep(cloud_top, cloud_top + height_scale * 4.8f, y);
	const float moist_air = biome_smoothstep(35.0f, 85.0f, humidity);
	const float morning_distance = biome_cycle_distance(timeofday, 0.24f);
	const float morning_time = 1.0f - biome_smoothstep(0.035f, 0.175f, morning_distance);
	const float morning_height =
			std::max(16.0f, static_cast<float>(weather_humidity_morning_height));
	const float morning_ground_layer =
			biome_smoothstep(-morning_height * 0.75f, -8.0f, height_above_surface) *
			(1.0f - biome_smoothstep(morning_height * 0.45f, morning_height,
							height_above_surface));

	humidity += low_air * 8.0f;
	humidity += cloud_band * (10.0f + moist_air * 38.0f);
	humidity += morning_time * morning_ground_layer *
				static_cast<float>(weather_humidity_morning) *
				(0.35f + moist_air * 0.65f);
	humidity *= 1.0f - high_air * 0.92f;
	humidity -= high_air * 8.0f;

	humidity = rangelim(humidity, 0, 100);

	return humidity;
}
