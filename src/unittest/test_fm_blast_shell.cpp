// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "contrib/fm_blast_angular.h"
#include <limits>

class TestFmBlastShell : public TestBase
{
public:
	TestFmBlastShell() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmBlastShell"; }
	void runTests(IGameDef *) override
	{
		TEST(testAngularUniformAir);
		TEST(testAngularMaterialShadow);
		TEST(testAngularSeparateContributions);
		TEST(testAngularSubdivisionIndependentLoss);
		TEST(testAngularCoreMixing);
		TEST(testConfiguration);
		TEST(testPartialCoreMixing);
		TEST(testCachedAreasAndStorage);
		TEST(testCoordinateHash);
		TEST(testSurvivingEnergy);
		TEST(testOutcomes);
		TEST(testUsefulAirRange);
		TEST(testFixedNodeLoss);
	}
	void testAngularUniformAir()
	{
		// Check projection alone; fixed per-node absorption changes the densities.
		const double initial = 260.0, loss = 0.0;
		const double sphere = 4.0 * std::acos(-1.0);
		auto patches = fm_blast_angular_seed(initial);
		for (int shell = 1; shell <= 24; ++shell) {
			auto hits = fm_blast_angular_project(patches, shell);
			UASSERT(hits.size() == static_cast<size_t>(24 * shell * shell + 2));
			fm_blast_angular_distance(hits, loss);
			const double expected = initial - 26.0 * loss * shell;
			double energy = 0.0, area = 0.0;
			patches.clear();
			for (const auto &[pos, hit] : hits) {
				energy += hit.energy;
				area += hit.area;
				// Axis, edge, corner, old and new nodes all see the same intensity.
				UASSERT(std::abs(hit.energy / hit.area - expected / sphere) < 1e-8);
			}
			hits.append_survivors(patches);
			UASSERT(std::abs(energy - expected) < 1e-8);
			UASSERT(std::abs(area - sphere) < 1e-10);
			fm_blast_angular_coalesce(patches);
			// Empty space does not accumulate fragments at old voxel boundaries.
			UASSERT(patches.size() == 6);
		}
	}

	void testAngularMaterialShadow()
	{
		const double initial = 4000.0;
		auto hits = fm_blast_angular_project(fm_blast_angular_seed(initial), 2);
		std::vector<FmBlastFootprint> patches;
		double surviving = 0.0;
		for (auto &[pos, hit] : hits) {
			// An opaque voxel blocks its entire angular region; a resistant
			// neighbor consumes 75% of the energy that crosses it.
			const double fraction = pos == v3pos_t(2, 0, 0)	  ? 0.0
									: pos == v3pos_t(2, 1, 0) ? 0.25
															  : 1.0;
			hit.energy *= fraction;
			surviving += hit.energy;
		}
		hits.append_survivors(patches);
		for (int shell = 3; shell <= 20; ++shell) {
			fm_blast_angular_coalesce(patches);
			hits = fm_blast_angular_project(patches, shell);
			UASSERT(hits.count(v3pos_t(shell, 0, 0)) == 0);
			if (shell >= 4) {
				const auto &weak = hits.at(v3pos_t(shell, shell / 2, 0));
				const auto &clear = hits.at(v3pos_t(-shell, 0, 0));
				UASSERT(std::abs(weak.energy / weak.area -
								 0.25 * clear.energy / clear.area) < 1e-8);
			}
			double energy = 0.0;
			patches.clear();
			for (const auto &[pos, hit] : hits) {
				energy += hit.energy;
			}
			hits.append_survivors(patches);
			UASSERT(std::abs(energy - surviving) < 1e-8);
		}
	}

	void testAngularSeparateContributions()
	{
		const std::vector<FmBlastFootprint> source{
				{0, -0.2, 0.0, -0.2, 0.2, 10.0}, {0, 0.0, 0.2, -0.2, 0.2, 40.0}};
		auto hits = fm_blast_angular_project(source, 1);
		UASSERT(hits.size() == 1);
		auto &hit = hits.at(v3pos_t(1, 0, 0));
		std::vector<FmBlastFootprint> survivors;
		hit.energy *= 0.5;
		hits.append_survivors(survivors);
		fm_blast_angular_coalesce(survivors);
		UASSERT(survivors.size() == 2);
		UASSERT(survivors[0].density == 5.0 && survivors[1].density == 20.0);
		const auto next = fm_blast_angular_project(survivors, 10);
		const auto &left = next.at(v3pos_t(10, -1, 0));
		const auto &right = next.at(v3pos_t(10, 1, 0));
		UASSERT(std::abs(right.energy / right.area - 4.0 * left.energy / left.area) <
				1e-10);
	}

	void testAngularSubdivisionIndependentLoss()
	{
		const auto source = fm_blast_angular_seed(260.0);
		std::vector<FmBlastFootprint> split;
		for (const auto &p : source)
			for (int u = -1; u <= 0; ++u)
				for (int v = -1; v <= 0; ++v)
					split.push_back({p.face, static_cast<double>(u), u + 1.0,
							static_cast<double>(v), v + 1.0, p.density});
		auto a = fm_blast_angular_project(source, 9);
		auto b = fm_blast_angular_project(split, 9);
		fm_blast_angular_distance(a, 0.02);
		fm_blast_angular_distance(b, 0.02);
		UASSERT(a.size() == b.size());
		for (const auto &[pos, hit] : a) {
			const auto &other = b.at(pos);
			UASSERT(std::abs(hit.energy - other.energy) < 1e-10);
			UASSERT(std::abs(hit.distance_cost - other.distance_cost) < 1e-10);
			UASSERT(hit.can_travel(0.15) == other.can_travel(0.15));
		}
	}

	void testAngularCoreMixing()
	{
		auto seed = fm_blast_angular_seed(600.0);
		seed.erase(seed.begin());
		auto hits = fm_blast_angular_project(seed, 3);
		UASSERT(hits.count(v3pos_t(3, 0, 0)) == 0);
		fm_blast_angular_mix_core(hits, 3, 0.0);
		UASSERT(hits.count(v3pos_t(3, 0, 0)) == 0);
		fm_blast_angular_mix_core(hits, 3, 1.0);
		UASSERT(hits.size() == 218);
		double energy = 0.0;
		for (const auto &[pos, hit] : hits) {
			UASSERT(std::abs(hit.energy - 500.0 / 218.0) < 1e-10);
			energy += hit.energy;
		}
		UASSERT(std::abs(energy - 500.0) < 1e-9);
	}

	void testConfiguration()
	{
		UASSERT(fm_blast_full_shell_radius(729) == 4);
		UASSERT(fm_blast_core_radius(0, 729) == 0);
		UASSERT(fm_blast_core_radius(3.9, 729) == 3);
		UASSERT(fm_blast_core_radius(std::numeric_limits<double>::quiet_NaN(), 729) == 4);
		UASSERT(fm_blast_fraction(-1, .8) == 0);
		UASSERT(fm_blast_fraction(2, .8) == 1);
		UASSERT(fm_blast_fraction(std::numeric_limits<double>::infinity(), .8) == .8);
		const auto split = fm_blast_split(100, .2);
		UASSERT(split.ray == 80 && split.shell == 20);
	}

	void testPartialCoreMixing()
	{
		const std::vector<FmBlastFootprint> source{
				{0, -.2, 0, -.2, .2, 10}, {0, 0, .2, -.2, .2, 40}};
		for (const double fraction : {.01, .25, .9}) {
			auto hits = fm_blast_angular_project(source, 1);
			const double energy = hits.energy();
			fm_blast_angular_mix_core(hits, 1, fraction);
			const auto &hit = hits.at({1, 0, 0});
			const double background = energy * fraction / 26 / hit.area;
			double low = -1, high = -1, area = 0;
			for (const auto &patch : hits.regions(hit)) {
				area += patch.area();
				if (patch.u0 == -.2 && patch.u1 == 0 && patch.v0 == -.2 && patch.v1 == .2)
					low = patch.density;
				else if (patch.u0 == 0 && patch.u1 == .2 && patch.v0 == -.2 &&
						 patch.v1 == .2)
					high = patch.density;
				else
					UASSERT(std::abs(patch.density - background) < 1e-10);
			}
			UASSERT(std::abs(low - (10 * (1 - fraction) + background)) < 1e-10);
			UASSERT(std::abs(high - low - 30 * (1 - fraction)) < 1e-10);
			UASSERT(std::abs(area - FmBlastFootprint(0, -.5, .5, -.5, .5, 0).area()) <
					1e-10);
			UASSERT(std::abs(hits.energy() - energy) < 1e-10);
		}
	}

	void testCachedAreasAndStorage()
	{
		auto hits = fm_blast_angular_project(fm_blast_angular_seed(1000), 8);
		std::vector<bool> seen(hits.patches.size(), false);
		for (const auto &[pos, hit] : hits) {
			for (size_t i = hit.first; i < hit.first + hit.count; ++i) {
				UASSERT(!seen[i]);
				seen[i] = true;
				const auto &p = hits.patches[i];
				UASSERT(p.area() ==
						FmBlastFootprint(p.face, p.u0, p.u1, p.v0, p.v1, p.density)
								.area());
			}
		}
		UASSERT(std::all_of(seen.begin(), seen.end(), [](bool b) { return b; }));
		std::vector<FmBlastFootprint> patches;
		hits.append_survivors(patches);
		fm_blast_angular_coalesce(patches);
		UASSERT(patches.size() == 6);
		for (const auto &p : patches)
			UASSERT(std::abs(p.area() -
							 FmBlastFootprint(p.face, p.u0, p.u1, p.v0, p.v1, p.density)
									 .area()) < 1e-12);
	}

	void testCoordinateHash()
	{
		std::unordered_set<size_t> hashes;
		const auto hits = fm_blast_angular_project(fm_blast_angular_seed(1), 64);
		for (const auto &[pos, hit] : hits)
			hashes.insert(FmBlastPosHash{}(pos));
		UASSERT(hits.size() == 98306);
		UASSERT(hashes.size() > hits.size() * .99);
	}

	void testSurvivingEnergy()
	{
		auto hits = fm_blast_angular_project(fm_blast_angular_seed(260), 2);
		const double blocked = hits.at({2, 0, 0}).energy;
		hits.at({2, 0, 0}).energy = 0;
		hits.at({-2, 0, 0}).energy += 17;
		std::vector<FmBlastFootprint> patches;
		hits.append_survivors(patches);
		UASSERT(std::abs(hits.energy() - (260 - blocked + 17)) < 1e-10);
		const auto next = fm_blast_angular_project(patches, 3);
		UASSERT(std::abs(next.energy() - hits.energy()) < 1e-10);
		UASSERT(next.count({3, 0, 0}) == 0);
	}

	void testUsefulAirRange()
	{
		auto patches = fm_blast_angular_seed(729);
		int shell = 0;
		size_t visited = 0;
		while (!patches.empty() && shell < 25) {
			fm_blast_angular_coalesce(patches);
			auto hits = fm_blast_angular_project(patches, ++shell);
			fm_blast_angular_distance(hits, .1);
			visited += hits.size();
			for (auto &[pos, hit] : hits)
				if (!hit.can_travel(.15))
					hit.energy = 0;
			patches.clear();
			hits.append_survivors(patches);
		}
		UASSERT(patches.empty());
		UASSERT(shell > 4 && shell <= 24);
		UASSERT(visited < 100000);
		// Concentrated surviving rays can still travel far beyond the bulk core.
		patches = {{0, -.005, .005, -.005, .005, 100000}};
		for (pos_t radius = 1; radius <= 40; ++radius) {
			auto hits = fm_blast_angular_project(patches, radius);
			fm_blast_angular_distance(hits, .1);
			UASSERT(hits.at({radius, 0, 0}).can_travel(.15));
			patches.clear();
			hits.append_survivors(patches);
		}
	}

	void testFixedNodeLoss()
	{
		for (int shell : {1, 4, 10}) {
			auto hits = fm_blast_angular_project(fm_blast_angular_seed(260), shell);
			const auto before = hits;
			fm_blast_angular_distance(hits, .1);
			for (const auto &[pos, hit] : hits) {
				const double expected = std::max(0.0, before.at(pos).energy - .1);
				UASSERT(std::abs(hit.energy - expected) < 1e-12);
				UASSERT(std::abs(hit.distance_cost -
								 std::min(.1, before.at(pos).energy)) < 1e-12);
			}
			std::vector<FmBlastFootprint> patches;
			hits.append_survivors(patches);
			const auto next = fm_blast_angular_project(patches, shell + 1);
			UASSERT(std::abs(next.energy() - hits.energy()) < 1e-9);
		}
		// Several angular contributions still pay just one node's cost.
		auto hits = fm_blast_angular_project(
				{{0, -.2, 0, -.2, .2, 10}, {0, 0, .2, -.2, .2, 40}}, 1);
		const double before = hits.energy();
		fm_blast_angular_distance(hits, .1);
		fm_blast_angular_distance(hits, .1);
		UASSERT(std::abs(hits.energy() - (before - .2)) < 1e-12);
		std::vector<FmBlastFootprint> patches;
		hits.append_survivors(patches);
		UASSERT(patches.size() == 2);
		UASSERT(std::abs(patches[1].density / patches[0].density - 4) < 1e-12);
	}

	void testOutcomes()
	{
		for (bool solid : {false, true}) {
			UASSERT(fm_blast_can_propagate(FmBlastOutcome::Removed, solid));
			UASSERT(fm_blast_can_propagate(FmBlastOutcome::Transformed, solid));
			UASSERT(!fm_blast_can_propagate(FmBlastOutcome::Blocked, solid));
			UASSERT(fm_blast_can_propagate(FmBlastOutcome::CallbackPending, solid) ==
					!solid);
			UASSERT(fm_blast_can_propagate(FmBlastOutcome::Transparent, solid) == !solid);
		}
	}
};
static TestFmBlastShell g_test_instance;
