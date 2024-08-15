/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

/*
	This class is the game's environment.
	It contains:
	- The map
	- Players
	- Other objects
	- The current time in the game
	- etc.
*/

#include <list>
#include <queue>
#include <map>
#include <atomic>
#include <mutex>
#include <optional>
#include "irr_v3d.h"
#include "util/basic_macros.h"
#include "line3d.h"

class IGameDef;
class Map;
struct PointedThing;
class RaycastState;
struct Pointabilities;

class Environment
{
public:
	// Environment will delete the map passed to the constructor
	Environment(IGameDef *gamedef);
	virtual ~Environment() = default;
	DISABLE_CLASS_COPY(Environment);

	/*
		Step everything in environment.
		- Move players
		- Step mobs
		- Run timers of map
	*/
	virtual void step(f32 dtime) = 0;

	virtual Map &getMap() = 0;

	u32 getDayNightRatio();

	// 0-23999
	virtual void setTimeOfDay(u32 time);
	u32 getTimeOfDay();
	float getTimeOfDayF();

	void stepTimeOfDay(float dtime);

	void setTimeOfDaySpeed(float speed);

	void setDayNightRatioOverride(bool enable, u32 value);

	u32 getDayCount();

	/*!
	 * Returns false if the given line intersects with a
	 * non-air node, true otherwise.
	 * \param pos1 start of the line
	 * \param pos2 end of the line
	 * \param p output, position of the first non-air node
	 * the line intersects
	 */
	bool line_of_sight(v3opos_t pos1, v3f pos2, v3pos_t *p = nullptr);

	/*!
	 * Gets the objects pointed by the shootline as
	 * pointed things.
	 * If this is a client environment, the local player
	 * won't be returned.
	 * @param[in]  shootline_on_map the shootline for
	 * the test in world coordinates
	 *
	 * @param[out] objects          found objects
	 */
	virtual void getSelectedActiveObjects(const core::line3d<opos_t> &shootline_on_map,
			std::vector<PointedThing> &objects,
			const std::optional<Pointabilities> &pointabilities) = 0;

	/*!
	 * Returns the next node or object the shootline meets.
	 * @param state current state of the raycast
	 * @result output, will contain the next pointed thing
	 */
	void continueRaycast(RaycastState *state, PointedThing *result);

	// counter used internally when triggering ABMs
	u32 m_added_objects;

	IGameDef *getGameDef() { return m_gamedef; }

protected:
	std::atomic<float> m_time_of_day_speed;

	/*
	 * Below: values managed by m_time_lock
	 */
	// Time of day in milli-hours (0-23999), determines day and night
	u32 m_time_of_day;
	// Time of day in 0...1
	float m_time_of_day_f;
	// Stores the skew created by the float -> u32 conversion
	// to be applied at next conversion, so that there is no real skew.
	float m_time_conversion_skew = 0.0f;
	// Overriding the day-night ratio is useful for custom sky visuals
	bool m_enable_day_night_ratio_override = false;
	u32 m_day_night_ratio_override = 0.0f;
	// Days from the server start, accounts for time shift
	// in game (e.g. /time or bed usage)
	std::atomic<u32> m_day_count;
	/*
	 * Above: values managed by m_time_lock
	 */

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change server settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool m_cache_enable_shaders;
	float m_cache_active_block_mgmt_interval;
	float m_cache_abm_interval;
	float m_cache_nodetimer_interval;
	float m_cache_abm_time_budget;

	IGameDef *m_gamedef;

private:
	std::mutex m_time_lock;
};
