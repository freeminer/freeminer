// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "test.h"
#include "fm_far_calc.h"
#include "client/fm_far_mesh_update.h"
#include <unordered_map>
#include <unordered_set>

class TestFmFarView : public TestBase
{
public:
	TestFmFarView() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFmFarView"; }
	void runTests(IGameDef *)
	{
		TEST(testProjection);
		TEST(testExtremeZoom);
		TEST(testSelectiveColumns);
		TEST(testGrid);
		TEST(testSurface);
		TEST(testPublication);
		TEST(testZoomOutChunks);
	}

private:
	static farmesh::View viewAt(const v3opos_t &position = {},
			const v3opos_t &direction = {0, 0, 1}, double degrees = 24,
			unsigned limit = farmesh::View::max_levels)
	{
		const double fov_y = degrees * (3.141592653589793 / 180.0);
		return farmesh::View::capture(position, direction,
				2 * std::atan(1.6 * std::tan(fov_y / 2)), fov_y, 72, limit);
	}

	void testProjection()
	{
		const auto view = viewAt();
		UASSERTEQ(unsigned, view.levels, 2);
		UASSERTEQ(unsigned, viewAt({}, {0, 0, 1}, 72).levels, 0);
		UASSERT(view.intersects(v3bpos_t(0, 0, 16), 1));
		UASSERT(!view.intersects(v3bpos_t(0, 0, -16), 1));
		UASSERT(!view.intersects(v3bpos_t(1000, 0, 16), 1));
		// Its center is outside the preload frustum, but its bounds overlap it.
		UASSERT(view.intersects(v3bpos_t(7, 0, 16), 8));
		UASSERT(viewAt({}, {0, 1, 0}).intersects(v3bpos_t(0, 16, 0), 1));
		UASSERT(!view.changed(view));
		UASSERT(view.changed(viewAt({}, {1, 0, 0})));
		UASSERT(view.changed(viewAt({}, {0, 0, 1}, 72)));
	}

	void testExtremeZoom()
	{
		const auto view = viewAt({}, {0, 0, 1}, 1);
		UASSERTEQ(unsigned, view.levels, 7);
		UASSERTEQ(unsigned, viewAt({}, {0, 0, 1}, 1, 0).levels, 0);
		UASSERTEQ(unsigned, viewAt({}, {0, 0, 1}, 1, 2).levels, 2);
		UASSERTEQ(unsigned, viewAt({}, {0, 0, 1}, 1, 4).levels, 4);
		UASSERTEQ(unsigned, viewAt({}, {0, 0, 1}, 1, 99).levels, 7);
		const auto capped = viewAt({}, {0, 0, 1}, 1, 2);
		const v3bpos_t player(0, 0, 0), distant(0, 0, 512);
		const auto normal = farmesh::getFarParams(player, 1, 8192, 1, distant);
		const auto old_detail =
				farmesh::getFarParams(player, 1, 8192, 1, distant, false, &capped);
		const auto new_detail =
				farmesh::getFarParams(player, 1, 8192, 1, distant, false, &view);
		UASSERT(normal && old_detail && new_detail);
		UASSERT(new_detail->step >= 1);
		UASSERT(new_detail->step < old_detail->step);
		UASSERT(int(normal->step) - new_detail->step > 2);

		UASSERTEQ(unsigned, view.refinementLevels(v3opos_t(0, 0, 512), 0.25), 7);
		// This small cell is wholly in the 20% preload margin at 1 degree.
		UASSERTEQ(unsigned, view.refinementLevels(v3opos_t(8, 0, 512), 0.25), 5);
		UASSERTEQ(unsigned, view.refinementLevels(v3opos_t(16, 0, 512), 0.25), 0);
		UASSERT(view.changed(viewAt({}, {0, 0, 1}, 1, 4)));
		const auto up = viewAt({}, {0, 1, 0}, 1);
		UASSERT(up.intersectsColumn(v3bpos_t(0, 0, 0), 1, -8, 8));
		UASSERT(!up.intersectsColumn(v3bpos_t(32, 0, 0), 1, -8, 8));
		const auto down = viewAt({}, {0, -1, 0}, 1);
		UASSERT(down.intersectsColumn(v3bpos_t(0, 0, 0), 1, -8, 8));
		UASSERT(!down.intersectsColumn(v3bpos_t(32, 0, 0), 1, -8, 8));
	}

	void testSelectiveColumns()
	{
		const auto narrow = viewAt({}, {0, -0.5, 1}, 1);
		auto everywhere = narrow;
		// A zero plane never rejects a cell: this models the old all-direction
		// surface refinement, with the same level cap and minimum mesh size.
		everywhere.planes.fill({});
		everywhere.view_planes.fill({});
		const auto count = [](const farmesh::View *view) {
			size_t cells = 0;
			farmesh::runFarAll(
					v3bpos_t(0, 0, 0), 1, 256, 1, 1, false, 0,
					[&](const v3bpos_t &, const bpos_t &, const block_step_t &) {
						++cells;
						return false;
					},
					view);
			return cells;
		};
		const auto selected = count(&narrow);
		UASSERT(selected > count(nullptr));
		UASSERT(selected < count(&everywhere));
		UASSERT(!narrow.intersectsColumn(v3bpos_t(0, 0, -64), 1, -32, 96));
	}

	using Grid = std::unordered_map<v3bpos_t, farmesh::tree_result_t>;
	static Grid grid(const v3bpos_t &player, uint8_t cell_pow, const farmesh::View *view)
	{
		Grid result;
		farmesh::runFarAll(
				player, cell_pow, 256, cell_pow, 0, false, 0,
				[&](const v3bpos_t &pos, const bpos_t &size, const block_step_t &step) {
					result.emplace(pos, farmesh::tree_result_t{pos, size, step});
					return false;
				},
				view);
		return result;
	}

	void testGrid()
	{
		const v3bpos_t player(-9, 5, -3);
		for (const double degrees : {24.0, 1.0}) {
			const auto view =
					viewAt(v3opos_t(player.X, player.Y, player.Z) * MAP_BLOCKSIZE,
							{0, 0, 1}, degrees);
			for (const uint8_t cell_pow : {0, 1, 2}) {
				const auto normal = grid(player, cell_pow, nullptr);
				const auto zoom = grid(player, cell_pow, &view);
				uint64_t normal_volume = 0, zoom_volume = 0;
				bool refined = false;
				for (const auto &[pos, cell] : normal) {
					const uint64_t width = uint64_t(cell.size) << cell_pow;
					normal_volume += width * width * width;
				}
				for (const auto &[pos, cell] : zoom) {
					const pos_t width = pos_t(cell.size) << cell_pow;
					zoom_volume += uint64_t(width) * width * width;
					// Check both ends of the cell, including negative coordinates.
					for (const auto sample :
							{pos, pos + v3bpos_t(width - 1, width - 1, width - 1)}) {
						const auto selected = farmesh::getFarParams(
								player, cell_pow, 256, cell_pow, sample, false, &view);
						UASSERT(selected);
						UASSERT(selected->pos == pos);
						UASSERTEQ(int, selected->step, cell.step);
						const auto storage = farmesh::getFarParams(
								player, cell_pow, 256, cell_pow, sample, true, &view);
						UASSERT(storage);
						UASSERTEQ(int, storage->step, cell.step);
						const auto base = farmesh::getFarParams(
								player, cell_pow, 256, cell_pow, sample);
						UASSERT(base);
						UASSERT(cell.step <= base->step);
						UASSERT(unsigned(base->step - cell.step) <= view.levels);
						UASSERT((cell.step == 0) == (base->step == 0));
						if (!view.intersects(base->pos, pos_t(base->size) << cell_pow)) {
							UASSERT(base->pos == cell.pos);
							UASSERTEQ(int, cell.step, base->step);
						}
						refined |= cell.step < base->step;
					}
				}
				UASSERT(refined);
				UASSERTEQ(uint64_t, zoom_volume, normal_volume);
			}
		}
	}

	void testSurface()
	{
		const v3bpos_t player(0, 0, 0);
		for (const double degrees : {24.0, 1.0}) {
			const auto view = viewAt({}, {0, -0.5, 1}, degrees);
			for (const uint8_t cell_pow : {0, 1, 2}) {
				for (const pos_t surface_y : {-24, 8, 48}) {
					Grid reached;
					pos_t min_x = 0, min_z = 0, max_x = 0, max_z = 0;
					farmesh::runFarAll(
							player, cell_pow, 256, cell_pow, 1, false, 0,
							[&](const v3bpos_t &pos, const bpos_t &size,
									const block_step_t &) {
								const pos_t width = pos_t(size) << cell_pow;
								min_x = std::min(min_x, pos_t(pos.X));
								min_z = std::min(min_z, pos_t(pos.Z));
								max_x = std::max<pos_t>(max_x, pos.X + width);
								max_z = std::max<pos_t>(max_z, pos.Z + width);
								const auto cell = farmesh::getFarParams(player, cell_pow,
										256, cell_pow, v3bpos_t(pos.X, surface_y, pos.Z),
										false, &view);
								UASSERT(cell);
								reached.insert_or_assign(cell->pos, *cell);
								return false;
							},
							&view);
					// Every surface cell is reached even though the 2-D traversal
					// cannot test the actual height against the view frustum.
					for (pos_t z = min_z; z < max_z; z += 1 << cell_pow)
						for (pos_t x = min_x; x < max_x; x += 1 << cell_pow) {
							const auto cell = farmesh::getFarParams(player, cell_pow, 256,
									cell_pow, v3bpos_t(x, surface_y, z), false, &view);
							UASSERT(cell);
							UASSERT(reached.contains(cell->pos));
							UASSERTEQ(int, reached.at(cell->pos).step, cell->step);
						}
				}
			}
		}
	}

	void testZoomOutChunks()
	{
		const v3bpos_t player(-9, 5, -3);
		for (const uint8_t cell_pow : {1, 2}) {
			const auto normal = grid(player, cell_pow, nullptr);
			auto visible = normal;
			const auto step = [](const auto &cell) { return cell.step; };
			for (const auto direction :
					{v3opos_t(0, 0, 1), v3opos_t(1, 0, 0), v3opos_t(0, 0, -1)}) {
				const auto view =
						viewAt(v3opos_t(player.X, player.Y, player.Z) * MAP_BLOCKSIZE,
								direction);
				const auto zoom = grid(player, cell_pow, &view);
				UASSERT(farmesh::publishReadyGrid(
						zoom, visible, [](const auto &) { return true; }, step,
						cell_pow));
				auto missing = normal.end();
				for (auto it = normal.begin(); it != normal.end(); ++it) {
					const auto fine = farmesh::getFarParams(
							player, cell_pow, 256, cell_pow, it->first, false, &view);
					if (fine && fine->step < it->second.step) {
						missing = it;
						break;
					}
				}
				UASSERT(missing != normal.end());
				UASSERT(!farmesh::publishReadyGrid(
						normal, visible,
						[&](const auto &cell) { return cell.pos != missing->first; },
						step, cell_pow));
				for (const auto &[pos, cell] : zoom)
					UASSERT(farmesh::findCoveringCell(visible, pos, step, cell_pow) !=
							visible.end());

				// Rebuilding a cached parent still needs every source block,
				// including those whose origins are not mesh-grid origins.
				std::unordered_set<v3bpos_t> sources;
				farmesh::forEachMeshSource(missing->first, missing->second.step, cell_pow,
						[&](const v3bpos_t &pos) {
							const auto source = farmesh::getFarParams(
									player, cell_pow, 256, cell_pow, pos, true);
							UASSERT(source);
							UASSERT(source->pos == pos);
							UASSERTEQ(int, source->step, missing->second.step);
							sources.insert(pos);
						});
				UASSERTEQ(size_t, sources.size(), size_t(1) << (3 * cell_pow));
				UASSERT(farmesh::publishReadyGrid(
						normal, visible, [](const auto &) { return true; }, step,
						cell_pow));
				UASSERTEQ(size_t, visible.size(), normal.size());
				for (const auto &[pos, cell] : normal) {
					UASSERT(visible.contains(pos));
					UASSERTEQ(int, visible.at(pos).step, cell.step);
				}
			}
		}
	}

	void testPublication()
	{
		const v3bpos_t player(0, 0, 0);
		auto visible = grid(player, 1, nullptr);
		const auto view = viewAt();
		const auto pending = grid(player, 1, &view);
		auto missing = pending.begin();
		for (auto it = pending.begin(); it != pending.end(); ++it) {
			const auto base = farmesh::getFarParams(player, 1, 256, 1, it->first);
			if (base && it->second.step < base->step) {
				missing = it;
				break;
			}
		}
		const auto ready = [&](const auto &cell) { return cell.pos != missing->first; };
		const auto step = [](const auto &cell) { return cell.step; };
		UASSERT(!farmesh::publishReadyGrid(pending, visible, ready, step, 1));
		// No requested fine cell loses its displayed ancestor while waiting.
		for (const auto &[pos, cell] : pending)
			UASSERT(farmesh::findCoveringCell(visible, pos, step, 1) != visible.end());
		UASSERT(farmesh::publishReadyGrid(
				pending, visible, [](const auto &) { return true; }, step, 1));
		UASSERTEQ(size_t, visible.size(), pending.size());
		for (const auto &[pos, cell] : pending) {
			UASSERT(visible.contains(pos));
			UASSERTEQ(int, visible.at(pos).step, cell.step);
		}
	}
};

static TestFmFarView g_test_fm_far_view;
