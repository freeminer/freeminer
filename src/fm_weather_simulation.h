// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fm_weather.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace weather
{
inline float bounded(float value, float low, float high)
{
	return std::isfinite(value) ? std::clamp(value, low, high) : low;
}

inline heat_t heat_value(float value)
{
	return static_cast<heat_t>(std::lround(bounded(value, -32768.0f, 32767.0f)));
}

inline humidity_t humidity_value(float value)
{
	return static_cast<humidity_t>(std::lround(bounded(value, 0.0f, 100.0f)));
}

inline wind_t limit_wind(wind_t wind)
{
	if (!std::isfinite(wind.X) || !std::isfinite(wind.Y) || !std::isfinite(wind.Z))
		return {};
	const float length = wind.getLength();
	if (length > 8.0f)
		wind *= 8.0f / length;
	return wind;
}

inline float saturation(float heat)
{
	// Vapor pressure in kPa. Bound the approximation away from its singularity.
	heat = bounded(heat, -100.0f, 100.0f);
	return 0.6108f * std::exp(17.27f * heat / (heat + 237.3f));
}

inline float moisture(float heat, float humidity)
{
	return saturation(heat) * bounded(humidity, 0.0f, 100.0f) * 0.01f;
}

inline float pressure(float heat, float humidity)
{
	// Pressure anomaly only: the hydrostatic altitude gradient is balanced by
	// gravity and must not accelerate an otherwise uniform atmosphere upward.
	return -heat - humidity * 0.25f;
}

// Fractional month phase; December interpolates into January.
inline float month_phase(float time, float year_days)
{
	float phase = std::fmod(time / (86400.0f * std::max(1.0f, year_days)), 1.0f);
	if (!std::isfinite(phase))
		return 0.0f;
	if (phase < 0.0f)
		phase += 1.0f;
	return std::min(phase * 12.0f, 11.999999f);
}

inline std::optional<float> blend_monthly(std::optional<float> first,
		std::optional<float> second, std::optional<float> fallback, float fraction)
{
	if (!first)
		first = fallback;
	if (!second)
		second = fallback;
	if (!first)
		return second;
	if (!second)
		return first;
	return *first + (*second - *first) * fraction;
}

// Unsigned subtraction also handles the game clock wrapping. A timestamp from
// a future/previous clock epoch must not cause billions of catch-up seconds.
inline uint32_t step_seconds(uint32_t now, uint32_t last)
{
	const uint32_t elapsed = now - last;
	return elapsed < 0x80000000u ? std::min<uint32_t>(elapsed, 20) : 0;
}

// Integrate pressure acceleration and drag analytically. With a prevailing
// horizontal wind, drag relaxes toward that wind rather than toward zero.
// Constant forcing gives the same result when a time interval is subdivided.
inline wind_t advance_wind(
		wind_t wind, const wind_t &gradient, const wind_t *prevailing, float dt)
{
	if (!(dt > 0.0f) || !std::isfinite(dt))
		return limit_wind(wind);
	const float horizontal_drag = prevailing ? 0.1609438f : 0.08f;
	const float horizontal_blend = -std::expm1(-horizontal_drag * dt);
	const float vertical_blend = -std::expm1(-0.08f * dt);
	wind_t target = prevailing ? *prevailing : wind_t{};
	target.Y = 0.0f;
	target = limit_wind(target);
	wind.X += (target.X - wind.X) * horizontal_blend -
			  gradient.X * (0.015f * horizontal_blend / horizontal_drag);
	wind.Z += (target.Z - wind.Z) * horizontal_blend -
			  gradient.Z * (0.015f * horizontal_blend / horizontal_drag);
	wind.Y += -wind.Y * vertical_blend - gradient.Y * (0.015f * vertical_blend / 0.08f);
	return limit_wind(wind);
}

struct Sample
{
	float heat = 0.0f;
	float humidity = 0.0f;
	wind_t wind{};
};

// A fixed neighbor snapshot. Precompute the thermodynamic values once per
// snapshot, not once per simulation substep (up to 20 times per visit).
class Neighbor
{
public:
	Neighbor() = default;
	Neighbor(const Sample &value, const wind_t &direction, bool present = true) :
			m_value(value), m_direction(direction), m_present(present),
			m_moisture(moisture(value.heat, value.humidity)),
			m_pressure(pressure(value.heat, value.humidity))
	{
	}
	bool present() const { return m_present; }
	const Sample &value() const { return m_value; }
	const wind_t &direction() const { return m_direction; }
	float vapor() const { return m_moisture; }
	float pressure_anomaly() const { return m_pressure; }

private:
	Sample m_value;
	wind_t m_direction{}; // Unit direction, not a world position.
	bool m_present = false;
	float m_moisture = 0.0f;
	float m_pressure = 0.0f;
};

// One bounded substep. Transport vapor pressure rather than relative humidity:
// warming the same air lowers RH without inventing or deleting water.
inline Sample integrate(const Sample &state, const std::array<Neighbor, 6> &neighbors,
		float base_heat, float base_humidity, const wind_t *prevailing, float dt)
{
	float heat_diffusion = 0.0f, water_diffusion = 0.0f;
	float heat_weights = 0.0f, water_weights = 0.0f;
	float heat_advection = 0.0f, water_advection = 0.0f;
	wind_t gradient{};
	int count = 0;
	const float water = moisture(state.heat, state.humidity);
	const float center_pressure = pressure(state.heat, state.humidity);
	for (const auto &neighbor : neighbors) {
		if (!neighbor.present())
			continue;
		const auto &other = neighbor.value();
		const auto &dir = neighbor.direction();
		const float dh = other.heat - state.heat;
		const float dw = neighbor.vapor() - water;
		const float hw = dir.Y > 0	 ? (dh < 0 ? 1.8f : 0.35f)
						 : dir.Y < 0 ? (dh > 0 ? 1.8f : 0.35f)
									 : 1.0f;
		const float ww = dir.Y > 0	 ? (dw > 0 ? 1.6f : 0.45f)
						 : dir.Y < 0 ? (dw < 0 ? 1.6f : 0.45f)
									 : 1.0f;
		const float inflow = std::max(0.0f, -state.wind.dotProduct(dir));
		heat_diffusion += dh * hw;
		water_diffusion += dw * ww;
		heat_weights += hw;
		water_weights += ww;
		heat_advection += dh * inflow;
		water_advection += dw * inflow;
		gradient += dir * (neighbor.pressure_anomaly() - center_pressure);
		++count;
	}
	if (count) {
		heat_diffusion /= heat_weights;
		water_diffusion /= water_weights;
		heat_advection /= count;
		water_advection /= count;
		gradient /= count;
	}
	Sample result = state;
	result.wind = advance_wind(state.wind, gradient, prevailing, dt);
	result.heat += (heat_diffusion * 0.035f + heat_advection * 0.015f) * dt;
	const float relax = 1.0f - std::exp(-0.006f * dt);
	result.heat += (base_heat - result.heat) * relax;
	result.heat = bounded(result.heat, -32768.0f, 32767.0f);
	float next_water = water + (water_diffusion * 0.055f + water_advection * 0.020f) * dt;
	next_water += (moisture(base_heat, base_humidity) - next_water) * relax;
	result.humidity =
			bounded(100.0f * next_water / saturation(result.heat), 0.0f, 100.0f);
	return result;
}
} // namespace weather
