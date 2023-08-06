#include "mapnode.h"
#include "nodedef.h"
#include "server.h"
#include "serverenvironment.h"

// Trees use param2 for rotation, level1 is free
inline uint8_t get_tree_water_level(const MapNode &n)
{
	return n.getParam1();
}
inline void set_tree_water_level(MapNode &n, const uint8_t level)
{
	n.setParam1(level);
}

// Leaves use param1 for light, level2 is free
inline uint8_t get_leaves_water_level(const MapNode &n)
{
	return n.getParam2();
}

inline void set_leaves_water_level(MapNode &n, const uint8_t level)
{
	n.setParam2(level);
}

inline auto getLight(const auto &ndef, const auto &n)
{
	const auto lightingFlags = ndef->getLightingFlags(n);
	return std::max(n.getLight(LIGHTBANK_DAY, lightingFlags),
			n.getLight(LIGHTBANK_NIGHT, lightingFlags));
}

const v3pos_t leaves_grow_dirs[] = {
		// +right, +top, +back
		v3pos_t{0, 1, 0},  // 1 top
		v3pos_t{0, 0, 1},  // 2 back
		v3pos_t{0, 0, -1}, // 3 front
		v3pos_t{1, 0, 0},  // 4 right
		v3pos_t{-1, 0, 0}, // 5 left
		v3pos_t{0, -1, 0}, // 6 bottom
};

struct GrowParams
{
	int tree_water_max = 50; // todo: depend on humidity 10-100
	int tree_grow_water_min = 20;
	int tree_grow_heat_min = 7;
	int tree_grow_heat_max = 40;
	int tree_grow_light_max = 12;		   // grow more leaves around before grow tree up
	int tree_get_water_from_humidity = 70; // rain start
	int tree_get_water_max_from_humidity = 30; // max level to get from air
	int tree_grow_chance = 10;
	int leaves_water_max = 20; // todo: depend on humidity 2-20
	int leaves_grow_light_min = 8;
	int leaves_grow_water_min_top = 3;
	int leaves_grow_water_min_bottom = 4;
	int leaves_grow_water_min_side = 2;
	int leaves_grow_heat_max = 40;
	int leaves_grow_heat_min = 3;
	int leaves_grow_prefer_top = 0;
	int leaves_die_light_max = 7; // 8
	int leaves_die_heat_max = -1;
	int leaves_die_heat_min = 55;
	int leaves_die_chance = 5;
	int leaves_die_from_liquid = 1;
	int leaves_to_fruit_water_min = 8;
	int leaves_to_fruit_heat_min = 15;
	int leaves_to_fruit_light_min = 9;
	int leaves_to_fruit_chance = 10;

	GrowParams(const ContentFeatures &cf, bool grow_debug_fast = false)
	{
		if (cf.groups.contains("tree_water_max"))
			tree_water_max = cf.groups.at("tree_water_max");
		if (cf.groups.contains("tree_grow_water_min"))
			tree_grow_water_min = cf.groups.at("tree_grow_water_min");
		if (cf.groups.contains("tree_grow_heat_min"))
			tree_grow_heat_min = cf.groups.at("tree_grow_heat_min");
		if (cf.groups.contains("tree_grow_heat_max"))
			tree_grow_heat_max = cf.groups.at("tree_grow_heat_max");
		if (cf.groups.contains("tree_grow_light_max"))
			tree_grow_light_max = cf.groups.at("tree_grow_light_max");
		if (cf.groups.contains("tree_grow_chance"))
			tree_grow_chance = grow_debug_fast ? 0 : cf.groups.at("tree_grow_chance");
		if (cf.groups.contains("tree_get_water_from_humidity"))
			tree_get_water_from_humidity = cf.groups.at("tree_get_water_from_humidity");
		if (cf.groups.contains("tree_get_water_max_from_humidity"))
			tree_get_water_max_from_humidity =
					cf.groups.at("tree_get_water_max_from_humidity");
		if (cf.groups.contains("leaves_water_max"))
			leaves_water_max = cf.groups.at("leaves_water_max");
		if (cf.groups.contains("leaves_grow_light_min"))
			leaves_grow_light_min = cf.groups.at("leaves_grow_light_min");
		if (cf.groups.contains("leaves_grow_water_min_top"))
			leaves_grow_water_min_top = cf.groups.at("leaves_grow_water_min_top");
		if (cf.groups.contains("leaves_grow_water_min_bottom"))
			leaves_grow_water_min_bottom = cf.groups.at("leaves_grow_water_min_bottom");
		if (cf.groups.contains("leaves_grow_water_min_side"))
			leaves_grow_water_min_side = cf.groups.at("leaves_grow_water_min_side");
		if (cf.groups.contains("leaves_grow_heat_max"))
			leaves_grow_heat_max = cf.groups.at("leaves_grow_heat_max");
		if (cf.groups.contains("leaves_grow_prefer_top"))
			leaves_grow_prefer_top = cf.groups.at("leaves_grow_prefer_top");
		if (cf.groups.contains("leaves_grow_heat_min"))
			leaves_grow_heat_min = cf.groups.at("leaves_grow_heat_min");
		if (cf.groups.contains("leaves_die_light_max"))
			leaves_die_light_max = cf.groups.at("leaves_die_light_max");
		if (cf.groups.contains("leaves_die_heat_max"))
			leaves_die_heat_max = cf.groups.at("leaves_die_heat_max");
		if (cf.groups.contains("leaves_die_heat_min"))
			leaves_die_heat_min = cf.groups.at("leaves_die_heat_min");
		if (cf.groups.contains("leaves_die_chance"))
			leaves_die_chance = grow_debug_fast ? 0 : cf.groups.at("leaves_die_chance");
		if (cf.groups.contains("leaves_die_from_liquid"))
			leaves_die_from_liquid = cf.groups.at("leaves_die_from_liquid");
		if (cf.groups.contains("leaves_to_fruit_water_min"))
			leaves_to_fruit_water_min = cf.groups.at("leaves_to_fruit_water_min");
		if (cf.groups.contains("leaves_to_fruit_heat_min"))
			leaves_to_fruit_heat_min = cf.groups.at("leaves_to_fruit_heat_min");
		if (cf.groups.contains("leaves_to_fruit_light_min"))
			leaves_to_fruit_light_min = cf.groups.at("leaves_to_fruit_light_min");
		if (cf.groups.contains("leaves_to_fruit_chance"))
			leaves_to_fruit_chance = cf.groups.at("leaves_to_fruit_chance");
	}
};

class GrowTree : public ActiveBlockModifier
{
	std::unordered_map<content_t, content_t> tree_to_leaves;
	std::unordered_map<content_t, GrowParams> type_params;

	bool grow_debug_fast = false;
	//  bool grow_debug = false;
public:
	GrowTree(ServerEnvironment *env, NodeDefManager *ndef)
	{
		// g_settings->getBoolNoEx("grow_debug", grow_debug);
		g_settings->getBoolNoEx("grow_debug_fast", grow_debug_fast);

		std::vector<content_t> ids;
		ndef->getIds("group:grow_tree", ids);
		for (const auto &id : ids) {
			const auto &cf = ndef->get(id);
			type_params.emplace(id, GrowParams(cf, grow_debug_fast));
			if (!cf.liquid_alternative_source.empty())
				tree_to_leaves[id] = ndef->getId(cf.liquid_alternative_source);
		}
	}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:grow_tree"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {};
	}
	// u32 getNeighborsRange() override { return 3; }
	virtual float getTriggerInterval() override { return grow_debug_fast ? 0.1 : 5; }
	virtual u32 getTriggerChance() override { return grow_debug_fast ? 1 : 5; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		float heat = map->updateBlockHeat(env, p);
		int16_t n_water_level = get_tree_water_level(n);
		const auto c = n.getContent();
		const auto params = type_params.at(c);

		const auto n_water_level_orig = n_water_level;

		const auto save = [&]() {
			if (n_water_level_orig != n_water_level) {
				set_tree_water_level(n, n_water_level);
				map->setNode(p, n);
			}
		};
		const auto decrease = [&](auto &level, int amount = 1) -> auto {
			if (level <= amount)
				return false;
			level -= amount;
			return true;
		};

		const content_t leaves_c =
				tree_to_leaves.contains(c) ? tree_to_leaves.at(c) : CONTENT_IGNORE;

		const auto facedir = n.getFaceDir(ndef);
		bool allow_grow_up_by_rotation =
				(facedir >= 0 && facedir <= 3) || (facedir >= 20 && facedir <= 23);

		// dont grow to sides if can grow up
		bool allow_grow_tree = allow_grow_up_by_rotation;
		bool allow_grow_leaves = allow_grow_up_by_rotation;
		bool top_is_not_tree = false;
		bool have_tree_near = false;
		bool all_is_tree = true;
		size_t has_soil = 0;
		std::vector<v3pos_t> has_liquids;

		size_t i = 0;
		for (const auto &dir : leaves_grow_dirs) {
			const auto p_dir = p + dir;
			auto n_dir = map->getNodeTry(p_dir);
			if (!n_dir) {
				all_is_tree = false;
				// if (grow_debug) DUMP("getfail tr", p_dir.X, p_dir.Y, p_dir.Z);
				break;
			}

			auto c_dir = n_dir.getContent();
			const auto &cf = ndef->get(c_dir);
			const auto light_dir = getLight(ndef, n_dir);
			bool top = !i;
			bool bottom = i + 1 == sizeof(leaves_grow_dirs) / sizeof(leaves_grow_dirs[0]);

			bool is_leaves = c_dir == leaves_c;

			bool is_liquid = cf.groups.contains("liquid");
			if (is_liquid)
				has_liquids.emplace_back(p_dir);

			bool is_tree = cf.groups.contains("tree");
			if (all_is_tree && !is_tree)
				all_is_tree = false;

			if (top && !is_tree)
				top_is_not_tree = true;

			bool is_soil = cf.groups.contains("soil");
			has_soil += is_soil;

			if (!top && !bottom && is_tree) {
				have_tree_near = true;
			}

			const auto n_dir_facedir = n_dir.getFaceDir(ndef);
			bool dir_allow_grow_up_by_rotation =
					(n_dir_facedir >= 0 && n_dir_facedir <= 3) ||
					(n_dir_facedir >= 20 && n_dir_facedir <= 23);

			const bool allow_grow_by_light = light_dir <= params.tree_grow_light_max;
			bool up_all_leaves = true;
			// light recalc sometimes too rare
			if (top && !allow_grow_by_light) {
				for (pos_t li = 1; li <= LIGHT_SUN - params.tree_grow_light_max; ++li) {
					const auto p_up = p + v3pos_t{0, li, 0};
					const auto n_up = map->getNodeTry(p_up);
					if (!n_up || n_up.getContent() == CONTENT_AIR) {
						up_all_leaves = false;
						break;
					}
				}
			}
			bool can_grow_replace_leaves =
					allow_grow_up_by_rotation && (allow_grow_by_light || up_all_leaves);

			// DUMP(i, p.Y, top, allow_grow_by_light, up_all_leaves, can_grow_replace_leaves, up_all_leaves);

			if (c != c_dir &&
					(!params.tree_grow_heat_min || heat > params.tree_grow_heat_min) &&
					(!params.tree_grow_heat_max || heat < params.tree_grow_heat_max) &&
					n_water_level >= params.tree_grow_water_min &&
					(((allow_grow_tree &&
							  ((c_dir == leaves_c && can_grow_replace_leaves) ||
									  ((top || bottom) && is_liquid))) ||
							 (top && light_dir <= params.leaves_die_light_max)) ||
							(bottom &&
									(is_liquid || is_soil || cf.groups.contains("sand")))
#if 0
// TODO: directional grow   
							||(!top && !bottom && n_water_level >= max_water_in_tree &&
									light_dir <= 1
									//&& c_dir == CONTENT_AIR
									&& cf.groups.contains("soil"))
#endif
									) &&
					!myrand_range(0, params.tree_grow_chance)) {
				// dont grow too deep in liquid
				if (bottom && is_liquid && light_dir <= 0)
					continue;
				if (bottom && have_tree_near)
					continue;
				if (!decrease(n_water_level))
					break;

				// if (grow_debug) DUMP("tr->tr", p_dir.Y, c_dir, c,n_water_level, n_water_level_orig, light_dir);

				map->setNode(p_dir, {c, 1});
			} else if (((!top && !bottom && c_dir == c) || is_leaves)) {
				auto wl_dir = c_dir == leaves_c ? get_leaves_water_level(n_dir)
												: get_tree_water_level(n_dir);

				if (!is_leaves || (is_leaves && (!allow_grow_up_by_rotation ||
														(!top && top_is_not_tree))))

					if (wl_dir < (is_leaves ? params.leaves_water_max
												: params.tree_water_max) &&
						n_water_level > wl_dir
						/* !!!
												n_water_level > wl_dir
						   + (top ? -1 :bottom ? 1 : 0)
						*/
				) {

					{
						if (!decrease(n_water_level)) {
							// if (grow_debug) DUMP("pumpfail",
							// n_water_level, n_water_level_orig,
							// wl_dir, top, bottom, c_dir, c);
							break;
						}
						++wl_dir;
							is_leaves ? set_leaves_water_level(n_dir, wl_dir)
										  : set_tree_water_level(n_dir, wl_dir);
					}

					map->setNode(p_dir, n_dir);
				}
			}
			if ((top && is_leaves) || c_dir == c) {
				allow_grow_tree = false;
			}
			if (top && c_dir == c) {
				allow_grow_leaves = false;
			}

			if (allow_grow_leaves && leaves_c != CONTENT_IGNORE &&
					heat >= params.leaves_grow_heat_min &&
					heat <= params.leaves_grow_heat_max &&
					(n_water_level >= (top ? params.leaves_grow_water_min_top
										   : params.leaves_grow_water_min_side)) &&
					// can_grow_leaves(n_water_level, top, bottom) &&
					light_dir >= params.leaves_grow_light_min) {

				if (cf.buildable_to && !is_liquid) {
					if (!decrease(n_water_level))
						break;
					map->setNode(p_dir, {leaves_c, n_dir.param1, 1});

					if (const auto block = map->getBlock(getNodeBlockPos(p_dir)); block) {
						block->setLightingExpired(true);
					}
				}
			}

			++i;
		}

		// up-down distribute of rest
		{
			int16_t total_level = n_water_level;
			int8_t have_liquid = 1;
			bool have_top = false, have_bottom = false;
			const auto p_bottom = p + v3pos_t{0, -1, 0};
			auto n_bottom = map->getNodeTry(p_bottom);
			int16_t wl_bottom = 0;
			if (n_bottom.getContent() == c) {
				have_bottom = true;
				wl_bottom = get_tree_water_level(n_bottom);
				total_level += wl_bottom;
				++have_liquid;
				// if (grow_debug) DUMP("get bot", wl_bottom, total_level, (int)have_liquid, have_bottom);
			}

			const auto p_top = p + v3pos_t{0, 1, 0};
			auto n_top = map->getNodeTry(p_top);
			int16_t wl_top = 0;
			if (n_top.getContent() == c) {
				have_top = true;
				wl_top = get_tree_water_level(n_top);
				total_level += wl_top;
				++have_liquid;
				// if (grow_debug) DUMP("get top", wl_top, total_level, (int)have_liquid, have_top);
			}

			if (have_bottom) {
				const auto avg_level_for_bottom = ceil((float)total_level / have_liquid);
				// if (grow_debug) DUMP(avg_level_for_bottom, (int)have_liquid, total_level, have_bottom, have_top);
				const auto bottom_level =
						avg_level_for_bottom < params.tree_water_max
								? avg_level_for_bottom +
										  (avg_level_for_bottom >= total_level ? 0 : 1)
								: params.tree_water_max;
				total_level -= bottom_level;
				--have_liquid;
				if (wl_bottom != bottom_level) {
					// if (grow_debug) DUMP("setbot", bottom_level, total_level, avg_level_for_bottom);
				set_tree_water_level(n_bottom, bottom_level);
				map->setNode(p_bottom, n_bottom);
			}
			}
			if (have_top) {
				auto float_avg_level_for_top = (float)total_level / have_liquid;
				const int16_t avg_level_for_top =
						all_is_tree ? int(float_avg_level_for_top)
									: floor(float_avg_level_for_top);
				const auto top_level = avg_level_for_top; //- 1;
				total_level -= top_level;
				if (wl_top != top_level) {
					// if (grow_debug) DUMP("settop", top_level, total_level, avg_level_for_top);
				set_tree_water_level(n_top, top_level);
				map->setNode(p_top, n_top);
			}
			}
			// if (grow_debug) DUMP("total res self:", (int)total_level);
			n_water_level = total_level;
		}

		if (has_soil) {
			n_water_level = grow_debug_fast ? params.tree_water_max
											: 1; // TODO depend on humidity
												 // tODO: absorb water from water here
		}

		if (n_water_level < params.tree_water_max && has_soil) {
			if (!has_liquids.empty()) {
				// TODO: cached and random
				const auto neighbor_pos = has_liquids.front();
				auto neighbor = map->getNodeTry(neighbor_pos);
				auto level = neighbor.getLevel(ndef);

				// TODO: allow get all water if bottom of water !=
				// water
				if (level <= 1)
					return;
				auto amount = grow_debug_fast ? level - 1 : 1;
				if (n_water_level + amount > params.tree_water_max)
					amount = params.tree_water_max - n_water_level;
				level -= amount;

				neighbor.setLevel(ndef, level);

				if (!grow_debug_fast)
					map->setNode(neighbor_pos, neighbor);
				set_tree_water_level(n, n_water_level += amount);
				map->setNode(p, n);
				// if (grow_debug) DUMP("absorbwater", n_water_level, level, amount);
			} else {
				float humidity = map->updateBlockHumidity(env, p);
				if (params.tree_get_water_from_humidity &&
						humidity >= params.tree_get_water_from_humidity &&
						n_water_level < params.tree_get_water_max_from_humidity) {
					// if (grow_debug) DUMP("absorbair", n_water_level, humidity);

					++n_water_level;
			}
		}
		}

		save();
	}
};

class GrowLeaves : public ActiveBlockModifier
{
	std::unordered_map<content_t, content_t> leaves_to_fruit;
	std::unordered_map<content_t, GrowParams> type_params;
	bool grow_debug_fast = false;
	//  bool grow_debug = false;

	static bool can_grow_leaves(
			GrowParams params, int8_t level, bool is_top, bool is_bottom)
	{
		if (is_top)
			return level >= params.leaves_grow_water_min_top;
		if (is_bottom)
			return level >= params.leaves_grow_water_min_bottom;
		return level >= params.leaves_grow_water_min_side;
	}

public:
	GrowLeaves(ServerEnvironment *env, NodeDefManager *ndef)
	{
		// g_settings->getBoolNoEx("grow_debug", grow_debug);
		g_settings->getBoolNoEx("grow_debug_fast", grow_debug_fast);

		std::vector<content_t> ids;
		ndef->getIds("group:grow_leaves", ids);
		for (const auto &id : ids) {
			const auto &cf = ndef->get(id);
			type_params.emplace(id, GrowParams(cf));
			if (!cf.liquid_alternative_source.empty())
				leaves_to_fruit[id] = ndef->getId(cf.liquid_alternative_source);
		}
	}
	virtual const std::vector<std::string> getTriggerContents() const override
	{
		return {"group:grow_leaves"};
	}
	virtual const std::vector<std::string> getRequiredNeighbors(
			bool activate) const override
	{
		return {};
	}
	u32 getNeighborsRange() override { return 1; }
	virtual float getTriggerInterval() override { return grow_debug_fast ? 0.1 : 10; }
	virtual u32 getTriggerChance() override { return grow_debug_fast ? 1 : 10; }
	bool getSimpleCatchUp() override { return true; }
	virtual pos_t getMinY() override { return -MAX_MAP_GENERATION_LIMIT; };
	virtual pos_t getMaxY() override { return MAX_MAP_GENERATION_LIMIT; };
	virtual void trigger(ServerEnvironment *env, v3pos_t p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider, v3pos_t,
			bool activate) override
	{
		ServerMap *map = &env->getServerMap();
		const auto *ndef = env->getGameDef()->ndef();
		float heat = map->updateBlockHeat(env, p);
		const auto c = n.getContent();
		const auto params = type_params.at(c);

		int n_water_level = get_leaves_water_level(n);
		const auto n_water_level_orig = n_water_level;

		const auto l = getLight(ndef, n);

		uint8_t i = 0;

		bool top_is_full_liquid = false;
		bool have_tree_or_soil = false;
		bool have_air = false;
		bool allow_grow_fruit = leaves_to_fruit.contains(c);
		const content_t c_fruit =
				allow_grow_fruit ? leaves_to_fruit.at(c) : CONTENT_IGNORE;
		// TODO: choose pump and grow direction by leaves type
		for (const auto &dir : leaves_grow_dirs) {
			const auto p_dir = p + dir;
			auto n_dir = map->getNodeTry(p_dir);
			if (!n_dir) {
				have_tree_or_soil = true; // dont remove when map busy
				allow_grow_fruit = false;
				have_air = false;
				continue;
			}
			const auto light_dir = getLight(ndef, n_dir);

			auto c_dir = n_dir.getContent();

			const auto &cf = ndef->get(c_dir);
			bool is_tree = cf.groups.contains("tree");
			bool is_leaves = cf.groups.contains("leaves");
			bool is_liquid = cf.groups.contains("liquid");
			bool top = !i;
			bool bottom = i + 1 == sizeof(leaves_grow_dirs) / sizeof(leaves_grow_dirs[0]);

			top_is_full_liquid =
					top && is_liquid && n_dir.getMaxLevel(ndef) == n_dir.getLevel(ndef);

			/*todo: shapes:
			o    sphere
			|   cypress
			___
			\ /
			*/

			if ((c_dir == c_fruit) || (!top && !bottom && !is_leaves))
				allow_grow_fruit = false;

			if (!have_tree_or_soil)
				have_tree_or_soil =
						is_tree || is_leaves || cf.groups.contains("soil") || is_liquid;
			if (!have_air)
				have_air = c_dir == CONTENT_AIR;

			if ((!params.leaves_grow_heat_min || heat >= params.leaves_grow_heat_min) &&
					(!params.leaves_grow_heat_max ||
							heat <= params.leaves_grow_heat_max) &&
					can_grow_leaves(params, n_water_level, top, bottom) &&
					light_dir > params.leaves_grow_light_min && cf.buildable_to &&
					!is_liquid) {
				// if (grow_debug) DUMP("lv->lv  ", p.X, p.Y, p.Z, c_dir, c, l, n_water_level, n_water_level_orig, l, ndef->get(c_dir).name);
				map->setNode(p_dir, {c, n_dir.getParam1(), 1});
				--n_water_level;

				if (!myrand_range(0, 10))
				if (const auto block = map->getBlock(getNodeBlockPos(p_dir)); block) {
					block->setLightingExpired(true);
				}

			} else if (c_dir == c) {
				const auto l_dir = getLight(ndef, n_dir);

				auto wl_dir = get_leaves_water_level(n_dir);
				if (n_water_level > 1 && wl_dir < params.leaves_water_max && l_dir >= l &&
						// todo: all up by type?
						wl_dir < n_water_level - 1 //(top ? -1 :
												   // bottom ? 1 : -2)
				) {
					--n_water_level;
					set_leaves_water_level(n_dir, ++wl_dir);
					map->setNode(p_dir, n_dir);

					// if (grow_debug) DUMP("lv pumpup2", p.Y,
					// n_water_level, n_water_level_orig, wl_dir, top,
					// bottom, c_dir);

					// Prefer pump up
					// todo: its like cypress, by settings
					if (top && params.leaves_grow_prefer_top) {
						break;
					}
				}
			}

			if (n_water_level != n_water_level_orig) {
				set_leaves_water_level(n, n_water_level);
				map->setNode(p, n);
			}
			++i;
		}

		// DUMP(allow_grow_fruit, n_water_level, leaves_to_fruit_water_min, heat ,
		// leaves_to_fruit_heat_min);
		if (allow_grow_fruit && n_water_level >= params.leaves_to_fruit_water_min &&
				heat >= params.leaves_to_fruit_heat_min &&
				l >= params.leaves_to_fruit_light_min &&
				(grow_debug_fast || !myrand_range(0, params.leaves_to_fruit_chance))) {
			map->setNode(p, {c_fruit});
		} else if (
				(n_water_level >= 1 && // dont touch old static trees
						have_air &&
						((l < params.leaves_die_light_max &&
								 (l > 0 || !myrand_range(0, params.leaves_die_chance))) ||
								((params.leaves_die_heat_max &&
										 heat < params.leaves_die_heat_max) ||
										(params.leaves_die_heat_min &&
												heat > params.leaves_die_heat_min)))) ||
				((!have_tree_or_soil ||
						   (params.leaves_die_from_liquid && top_is_full_liquid)) &&
						!myrand_range(0, 10))) {
			map->removeNodeWithEvent(p, false);
		}
	}
};

void add_abm_grow_tree(ServerEnvironment *env, NodeDefManager *nodedef)
{
	bool grow = true;
	g_settings->getBoolNoEx("grow_tree", grow);
	if (grow) {
		env->addActiveBlockModifier(new GrowTree(env, nodedef));
		env->addActiveBlockModifier(new GrowLeaves(env, nodedef));
	}
}
