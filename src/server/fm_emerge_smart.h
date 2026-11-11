// Freeminer
// Native mapblock scheduler for /emerge_smart.

#pragma once

#include <cstdint>

namespace emerge_smart
{
// Limit background columns without stopping the job when players join.
constexpr uint32_t inFlightLimit(
		uint32_t configured, uint32_t players, uint32_t free_players, bool throttle)
{
	if (!configured)
		return 1;
	const uint64_t divisor =
			1 + uint64_t(throttle && players > free_players ? players - free_players : 0);
	const uint32_t limit = static_cast<uint32_t>(configured / divisor);
	return limit ? limit : 1;
}
}

struct lua_State;

void fm_register_emerge_smart_api(lua_State *L, int top);
