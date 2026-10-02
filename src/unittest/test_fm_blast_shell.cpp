// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "contrib/fm_blast_shell.h"
#include "contrib/fm_blast_angular.h"
#include <map>
#include <limits>
#include <tuple>

class TestFmBlastShell : public TestBase
{
public:
	TestFmBlastShell() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmBlastShell"; }
	void runTests(IGameDef *) override
	{
		TEST(testTenPercentSplit);
		TEST(testDeterministicTransfer);
		TEST(testActiveShellPool);
		TEST(testActiveShellConservation);
		TEST(testZeroEnergy);
		TEST(testConfiguredFractions);
		TEST(testFractionValidation);
		TEST(testAbsorptionSplit);
		TEST(testInitialStrengthBoundary);
		TEST(testNearAndFarShellSharing);
		TEST(testDirectionalLocalSplit);
		TEST(testCorePhaseControls);
		TEST(testNearestDirectionalTransfer);
		TEST(testAirTravel);
		TEST(testScatterEndpoints);
		TEST(testNewRayCoverage);
		TEST(testNoNewRays);
		TEST(testPersistentDirection);
		TEST(testExistingContinuationIsNotNew);
		TEST(testAutomaticNewRayOption);
		TEST(testNeighborDonors);
		TEST(testSharedNewRayFunding);
		TEST(testAngularUniformAir);
		TEST(testAngularMaterialShadow);
		TEST(testAngularSeparateContributions);
		TEST(testAngularSubdivisionIndependentLoss);
		TEST(testAngularCoreMixing);
	}

	static double localStrength(const FmBlastChildren &children)
	{
		double strength = 0.0;
		for (size_t i = 0; i < children.count; ++i)
			strength += children.nodes[i].strength;
		return strength;
	}

	void testTenPercentSplit()
	{
		const v3pos_t parents[] = {
				{0, 0, 0}, {8, 0, 0}, {8, 8, 0}, {8, 8, 8}, {-8, 3, -5}};
		for (const auto &parent : parents) {
			const int shell = parent == v3pos_t(0, 0, 0) ? 1 : 9;
			for (uint32_t seed = 0; seed < 100; ++seed) {
				const auto children = fm_blast_children(parent, shell, 12.0, 0.02, seed);
				UASSERT(children.count == (shell == 1 ? 26 : 1));
				UASSERT(std::abs(children.diffuse_strength - 1.2) < 1e-12);
				const auto &child = children.nodes[0];
				UASSERT(std::abs(localStrength(children) - 10.8) < 1e-12);
				const auto step = child.pos - parent;
				UASSERT(std::abs(step.X) <= 1 && std::abs(step.Y) <= 1 &&
						std::abs(step.Z) <= 1);
				UASSERT(std::max({std::abs(child.pos.X), std::abs(child.pos.Y),
								std::abs(child.pos.Z)}) == shell);
				UASSERT(child.step_cost >= 0.02 && child.step_cost < 0.035);
			}
		}
	}

	void testDeterministicTransfer()
	{
		const auto baseline = fm_blast_children({8, 3, 5}, 9, 12, 0.02, 0);
		for (uint32_t seed = 0; seed < 100; ++seed) {
			const auto children = fm_blast_children({8, 3, 5}, 9, 12, 0.02, seed);
			UASSERT(children.count == baseline.count);
			for (size_t i = 0; i < children.count; ++i) {
				UASSERT(children.nodes[i].pos == baseline.nodes[i].pos);
				UASSERT(children.nodes[i].strength == baseline.nodes[i].strength);
				UASSERT(children.nodes[i].step_cost == baseline.nodes[i].step_cost);
			}
		}
	}

	void testActiveShellPool()
	{
		using Key = std::tuple<pos_t, pos_t, pos_t>;
		// Only these two child positions are reached; sharing cannot add more.
		std::map<Key, double> weights{{Key{9, 0, 0}, 80.0}, {Key{9, 1, 0}, 10.0}};
		std::map<Key, double> steps;
		fm_blast_distribute_pool(weights, steps, 10.0, 0.02);
		UASSERT(weights.size() == 2);
		UASSERT(weights.at(Key{9, 0, 0}) == 85.0);
		UASSERT(weights.at(Key{9, 1, 0}) == 15.0);
		UASSERT(weights.count(Key{-9, 0, 0}) == 0);
		UASSERT(std::abs(steps.at(Key{9, 0, 0}) - 0.1) < 1e-12);
		// A fully pooled fraction still funds existing destinations with zero local energy.
		weights = {{Key{9, 0, 0}, 0.0}, {Key{9, 1, 0}, 0.0}};
		steps.clear();
		fm_blast_distribute_pool(weights, steps, 100.0, 0.0);
		UASSERT(weights.at(Key{9, 0, 0}) == 50.0);
		UASSERT(weights.at(Key{9, 1, 0}) == 50.0);
		weights.clear();
		steps.clear();
		fm_blast_distribute_pool(weights, steps, 100.0, 0.02);
		UASSERT(weights.empty() && steps.empty());
	}

	void testActiveShellConservation()
	{
		using Key = std::tuple<pos_t, pos_t, pos_t>;
		std::map<Key, double> weights{{Key{0, 0, 0}, 1000.0}};
		for (int shell = 1; shell <= 8; ++shell) {
			std::map<Key, double> next, steps;
			double diffuse = 0.0;
			for (const auto &[pos, strength] : weights) {
				const auto children = fm_blast_children(
						{std::get<0>(pos), std::get<1>(pos), std::get<2>(pos)}, shell,
						strength, 0.0, 42);
				diffuse += children.diffuse_strength;
				for (size_t i = 0; i < children.count; ++i) {
					const auto &child = children.nodes[i];
					next[{child.pos.X, child.pos.Y, child.pos.Z}] += child.strength;
				}
			}
			const auto reached = next.size();
			fm_blast_distribute_pool(next, steps, diffuse, 0.0);
			UASSERT(next.size() == reached);
			if (shell == 1) {
				UASSERT(next.size() == 26);
				for (const auto &[pos, strength] : next)
					UASSERT(std::abs(strength - 1000.0 / 26.0) < 1e-10);
			} else {
				UASSERT(!next.empty() && next.size() <= 9 * weights.size());
			}
			double total = 0.0;
			for (const auto &[pos, strength] : next) {
				UASSERT(strength > 0.0);
				total += strength;
			}
			UASSERT(std::abs(total - 1000.0) < 1e-8);
			weights = std::move(next);
		}
	}

	void testConfiguredFractions()
	{
		for (double fraction : {0.0, 0.1, 0.5, 1.0}) {
			const auto children =
					fm_blast_children({8, 0, 0}, 9, 100.0, 0.0, 42, fraction);
			UASSERT(children.count == 1);
			UASSERT(std::abs(children.diffuse_strength - 100.0 * fraction) < 1e-10);
			UASSERT(std::abs(localStrength(children) + children.diffuse_strength -
							 100.0) < 1e-10);
		}
	}

	void testFractionValidation()
	{
		UASSERT(fm_blast_fraction(-1.0, 0.1) == 0.0);
		UASSERT(fm_blast_fraction(2.0, 0.1) == 1.0);
		UASSERT(fm_blast_fraction(0.0, 0.1) == 0.0);
		UASSERT(fm_blast_fraction(1.0, 0.1) == 1.0);
		UASSERT(fm_blast_fraction(std::numeric_limits<double>::infinity(), 0.1) == 0.1);
		UASSERT(fm_blast_fraction(std::numeric_limits<double>::quiet_NaN(), 0.0) == 0.0);
	}

	void testAbsorptionSplit()
	{
		for (double ray_fraction : {0.0, 0.8, 1.0}) {
			const auto energy = fm_blast_split(100.0, 1.0 - ray_fraction);
			UASSERT(std::abs(energy.ray - 100.0 * ray_fraction) < 1e-10);
			// The absorbed position also receives one share from the current shell.
			const double share = energy.shell / 4.0;
			UASSERT(std::abs(energy.ray + share + 3.0 * share - 100.0) < 1e-10);
		}
	}

	void testInitialStrengthBoundary()
	{
		UASSERT(fm_blast_full_shell_radius(0.0) == 0.0);
		UASSERT(fm_blast_full_shell_radius(1.0) == 1.0);
		UASSERT(fm_blast_full_shell_radius(125.0) == 2.0);
		UASSERT(fm_blast_full_shell_radius(729.0) == 4.0);
		UASSERT(fm_blast_full_shell_radius(4913.0) == 8.0);
	}

	void testNearAndFarShellSharing()
	{
		using Key = std::tuple<pos_t, pos_t, pos_t>;
		const auto key = [](const v3pos_t &p) { return Key{p.X, p.Y, p.Z}; };
		for (int shell : {1, 4, 5}) {
			std::map<Key, double> weights{{Key{shell, 0, 0}, 90.0}}, steps;
			fm_blast_distribute_shell_pool(weights, steps, 10.0, 0.0, shell, 729.0, key);
			if (shell <= 4) {
				UASSERT(weights.size() == static_cast<size_t>(24 * shell * shell + 2));
				UASSERT(weights.at(Key{-shell, 0, 0}) > 0.0);
			} else {
				UASSERT(weights.size() == 1);
				UASSERT(weights.count(Key{-shell, 0, 0}) == 0);
			}
			double total = 0.0;
			for (const auto &[pos, strength] : weights)
				total += strength;
			UASSERT(std::abs(total - 100.0) < 1e-9);
		}
		// A larger initial explosion keeps full sharing on shell 5. A larger
		// current energy pool alone does not change a smaller explosion's boundary.
		std::map<Key, double> weights{{Key{5, 0, 0}, 90.0}}, steps;
		fm_blast_distribute_shell_pool(weights, steps, 10.0, 0.0, 5, 4913.0, key);
		UASSERT(weights.size() == 24 * 5 * 5 + 2);
		weights = {{Key{5, 0, 0}, 90.0}};
		steps.clear();
		fm_blast_distribute_shell_pool(weights, steps, 10000.0, 0.0, 5, 729.0, key);
		UASSERT(weights.size() == 1);
		weights = {{Key{4, 0, 0}, 90.0}};
		steps.clear();
		fm_blast_distribute_shell_pool(weights, steps, 0.0, 0.0, 4, 729.0, key);
		UASSERT(weights.size() == 1 && steps.empty());
	}

	void testDirectionalLocalSplit()
	{
		const auto children = fm_blast_children({8, 4, 4}, 9, 100.0, 0.0, 0, 0.2);
		UASSERT(children.count == 4);
		UASSERT(children.diffuse_strength == 20.0);
		UASSERT(children.nodes[0].pos == v3pos_t(9, 5, 5));
		UASSERT(std::abs(children.nodes[0].strength - 40.0) < 1e-10);
		for (size_t i = 1; i < children.count; ++i) {
			UASSERT(std::abs(children.nodes[i].strength - 40.0 / 3.0) < 1e-10);
			UASSERT(fm_blast_project(children.nodes[i].pos, 9, 8) == v3pos_t(8, 4, 4));
		}
	}

	void testCorePhaseControls()
	{
		UASSERT(fm_blast_core_radius(3.9, 729.0) == 3.0);
		UASSERT(fm_blast_core_radius(-1.0, 729.0) == 0.0);
		UASSERT(fm_blast_core_radius(std::numeric_limits<double>::quiet_NaN(), 729.0) ==
				4.0);
		for (int shell : {1, 3, 4, 8}) {
			const double fraction =
					fm_blast_shell_transfer_fraction(shell, 3.0, 0.9, 0.05);
			UASSERT(fraction == (shell <= 3 ? 0.9 : 0.05));
			const auto children = fm_blast_children({static_cast<pos_t>(shell - 1), 0, 0},
					shell, 100.0, 0.0, 42, fraction);
			UASSERT(std::abs(children.diffuse_strength - 100.0 * fraction) < 1e-10);
		}
		using Key = std::tuple<pos_t, pos_t, pos_t>;
		const auto key = [](const v3pos_t &p) { return Key{p.X, p.Y, p.Z}; };
		std::map<Key, double> weights{{Key{4, 0, 0}, 90.0}}, steps;
		// An explicit smaller core overrides the strength-derived boundary.
		fm_blast_distribute_shell_pool(weights, steps, 10.0, 0.0, 4, 729.0, key, 3.0);
		UASSERT(weights.size() == 1 && weights.at(Key{4, 0, 0}) == 100.0);
		weights = {{Key{5, 0, 0}, 90.0}};
		steps.clear();
		// An explicit larger core permits full-surface sharing beyond that boundary.
		fm_blast_distribute_shell_pool(weights, steps, 10.0, 0.0, 5, 729.0, key, 5.0);
		UASSERT(weights.size() == 24 * 5 * 5 + 2);
		UASSERT(weights.at(Key{-5, 0, 0}) > 0.0);
	}

	void testNearestDirectionalTransfer()
	{
		for (const auto &parent : {v3pos_t(20, 4, 9), v3pos_t(-20, -4, -9),
					 v3pos_t(4, 20, -10), v3pos_t(9, -4, 20), v3pos_t(20, 20, 20)}) {
			const auto children = fm_blast_children(parent, 21, 100.0, 0.0, 0, 0.0, 0.0);
			UASSERT(children.count == 1);
			UASSERT(children.nodes[0].strength == 100.0);
			const auto forward = children.nodes[0].pos;
			const auto step = forward - parent;
			UASSERT(std::abs(step.X) <= 1 && std::abs(step.Y) <= 1 &&
					std::abs(step.Z) <= 1);
			UASSERT(std::max({std::abs(forward.X), std::abs(forward.Y),
							std::abs(forward.Z)}) == 21);
			UASSERT(std::abs(forward.X - parent.X * 21.0 / 20.0) <= 0.5 + 1e-12);
			UASSERT(std::abs(forward.Y - parent.Y * 21.0 / 20.0) <= 0.5 + 1e-12);
			UASSERT(std::abs(forward.Z - parent.Z * 21.0 / 20.0) <= 0.5 + 1e-12);
		}
	}

	void testAirTravel()
	{
		v3pos_t pos(1, 0, 0);
		double strength = 100.0;
		for (int shell = 2; shell <= 101; ++shell) {
			const auto children =
					fm_blast_children(pos, shell, strength, 0.02, 42, 0.0, 0.0);
			UASSERT(children.count == 1);
			UASSERT(children.diffuse_strength == 0.0);
			UASSERT(children.nodes[0].strength == strength);
			strength = children.nodes[0].strength - children.nodes[0].step_cost;
			pos = children.nodes[0].pos;
		}
		// No exponential branching loss: only distance loss along 100 crossings.
		UASSERT(strength >= 100.0 - 100.0 * std::sqrt(3.0) * 0.02 - 1e-10);
		UASSERT(strength < 100.0);
	}

	void testScatterEndpoints()
	{
		for (double scatter : {0.0, 0.1, 0.4, 1.0}) {
			const auto children =
					fm_blast_children({8, 4, 4}, 9, 100.0, 0.0, 42, 0.2, scatter);
			UASSERT(children.count == (scatter == 0.0 ? 1 : scatter == 1.0 ? 3 : 4));
			UASSERT(std::abs(localStrength(children) + children.diffuse_strength -
							 100.0) < 1e-10);
			if (scatter > 0.0) {
				const size_t first = scatter == 1.0 ? 0 : 1;
				for (size_t i = first; i < children.count; ++i)
					UASSERT(std::abs(children.nodes[i].strength - 80.0 * scatter / 3.0) <
							1e-10);
			}
		}
	}

	void testNewRayCoverage()
	{
		using Key = std::tuple<pos_t, pos_t, pos_t>;
		for (int shell = 2; shell <= 16; ++shell) {
			std::map<Key, int> hits;
			size_t main_count = 0, newborn_count = 0;
			const pos_t radius = static_cast<pos_t>(shell - 1);
			for (pos_t x = -radius; x <= radius; ++x)
				for (pos_t y = -radius; y <= radius; ++y)
					for (pos_t z = -radius; z <= radius; ++z) {
						if (std::max({std::abs(x), std::abs(y), std::abs(z)}) != radius)
							continue;
						const v3pos_t parent(x, y, z);
						const auto children =
								fm_blast_children(parent, shell, 100.0, 0.0, 0, 0.0);
						UASSERT(children.count >= 1);
						UASSERT(children.nodes[0].pos ==
								fm_blast_project(parent, shell - 1, shell));
						UASSERT(children.nodes[0].strength ==
								(children.count == 1 ? 100.0 : 50.0));
						UASSERT(std::abs(localStrength(children) - 100.0) < 1e-10);
						++main_count;
						newborn_count += children.count - 1;
						for (size_t i = 0; i < children.count; ++i) {
							const auto pos = children.nodes[i].pos;
							UASSERT(fm_blast_project(pos, shell, shell - 1) == parent);
							UASSERT(std::max({std::abs(pos.X), std::abs(pos.Y),
											std::abs(pos.Z)}) == shell);
							const auto step = pos - parent;
							UASSERT(std::abs(step.X) <= 1 && std::abs(step.Y) <= 1 &&
									std::abs(step.Z) <= 1);
							const Key key{pos.X, pos.Y, pos.Z};
							UASSERT(++hits[key] == 1);
							if (i > 0)
								UASSERT(std::abs(children.nodes[i].strength -
												 50.0 / (children.count - 1)) < 1e-10);
						}
					}
			UASSERT(main_count == static_cast<size_t>(24 * radius * radius + 2));
			UASSERT(hits.size() == static_cast<size_t>(24 * shell * shell + 2));
			UASSERT(newborn_count == hits.size() - main_count);
		}
	}

	void testNoNewRays()
	{
		for (double scatter : {-1.0, 0.0, 0.35, 1.0}) {
			const auto children =
					fm_blast_children({8, 0, 0}, 9, 100.0, 0.0, 0, 0.2, scatter);
			UASSERT(children.count == 1);
			UASSERT(children.nodes[0].pos == v3pos_t(9, 0, 0));
			UASSERT(children.nodes[0].strength == 80.0);
			UASSERT(children.diffuse_strength == 20.0);
		}
	}

	void testPersistentDirection()
	{
		for (const auto &initial : {v3pos_t(20, 4, 9), v3pos_t(-20, -4, -9),
					 v3pos_t(4, 20, -10), v3pos_t(9, -4, 20)}) {
			const auto initial_direction = fm_blast_direction(initial, 20);
			auto direction = initial_direction;
			auto parent = initial;
			for (int shell = 21; shell <= 120; ++shell) {
				const auto children = fm_blast_children(
						parent, shell, 100.0, 0.0, 0, 0.0, 0.0, &direction);
				UASSERT(children.count == 1 && children.nodes[0].strength == 100.0);
				const auto &child = children.nodes[0];
				UASSERT(child.direction == initial_direction);
				UASSERT(std::abs(child.pos.X - initial_direction[0] * shell) <=
						0.5 + 1e-12);
				UASSERT(std::abs(child.pos.Y - initial_direction[1] * shell) <=
						0.5 + 1e-12);
				UASSERT(std::abs(child.pos.Z - initial_direction[2] * shell) <=
						0.5 + 1e-12);
				const auto step = child.pos - parent;
				UASSERT(std::abs(step.X) <= 1 && std::abs(step.Y) <= 1 &&
						std::abs(step.Z) <= 1);
				direction = child.direction;
				parent = child.pos;
			}
		}
	}

	void testExistingContinuationIsNotNew()
	{
		const auto occupied = [](const v3pos_t &pos) { return pos == v3pos_t(9, 4, 4); };
		const auto children = fm_blast_children(
				{8, 4, 4}, 9, 100.0, 0.0, 0, 0.0, -1.0, nullptr, occupied);
		UASSERT(children.count == 3);
		UASSERT(children.nodes[0].strength == 50.0);
		for (size_t i = 1; i < children.count; ++i) {
			UASSERT(!occupied(children.nodes[i].pos));
			UASSERT(children.nodes[i].strength == 25.0);
			UASSERT(children.nodes[i].direction ==
					fm_blast_direction(children.nodes[i].pos, 9));
		}
	}

	void testAutomaticNewRayOption()
	{
		// Exercise the same normalization used by Lua call option parsing.
		for (double option : {-1.0, -2.0, std::numeric_limits<double>::quiet_NaN(),
					 std::numeric_limits<double>::infinity()}) {
			const double parsed = fm_blast_new_ray_fraction(option);
			UASSERT(parsed == 0.5);
			const auto children =
					fm_blast_children({1, 1, 1}, 2, 100.0, 0.0, 0, 0.0, parsed);
			UASSERT(children.count == 7);
			UASSERT(children.nodes[0].strength == 50.0);
			UASSERT(std::abs(localStrength(children) - 100.0) < 1e-10);
		}
		UASSERT(fm_blast_new_ray_fraction(0.0) == 0.0);
		UASSERT(fm_blast_new_ray_fraction(0.35) == 0.35);
		UASSERT(fm_blast_new_ray_fraction(2.0) == 1.0);
		const auto disabled = fm_blast_children(
				{1, 1, 1}, 2, 100.0, 0.0, 0, 0.0, fm_blast_new_ray_fraction(0.0));
		UASSERT(disabled.count == 1 && disabled.nodes[0].strength == 100.0);
	}

	void testNeighborDonors()
	{
		const v3pos_t parent(8, 4, 4);
		const auto full_face = [](const v3pos_t &pos) { return pos.X == 8; };
		const auto donors = fm_blast_new_ray_donors(parent, 8, full_face);
		UASSERT(donors.count == 9 && donors.nodes[0] == parent);
		for (size_t i = 1; i < donors.count; ++i) {
			const auto step = donors.nodes[i] - parent;
			UASSERT(step.X == 0 && std::abs(step.Y) <= 1 && std::abs(step.Z) <= 1);
			UASSERT(std::abs(step.Y) + std::abs(step.Z) >= 1);
			for (size_t j = 0; j < i; ++j)
				UASSERT(donors.nodes[i] != donors.nodes[j]);
		}
		const auto sparse = [&](const v3pos_t &pos) {
			return pos == parent || pos == v3pos_t(8, 5, 4);
		};
		const auto fewer = fm_blast_new_ray_donors(parent, 8, sparse);
		UASSERT(fewer.count == 2);
		const auto blocked =
				fm_blast_new_ray_donors(parent, 8, [](const v3pos_t &) { return false; });
		UASSERT(blocked.count == 0);
	}

	void testSharedNewRayFunding()
	{
		const v3pos_t parent(8, 4, 4), target(9, 4, 4), other(9, 4, 5);
		const auto donors = fm_blast_new_ray_donors(
				parent, 8, [](const v3pos_t &pos) { return pos.X == 8; });
		double total = 0.0, newborn = 0.0;
		for (size_t i = 0; i < donors.count; ++i) {
			const double strength = 100.0 * (i + 1);
			double main = 0.0, emitted = 0.0;
			// Each donor funds two targets from one shared half-energy budget.
			fm_blast_fund_new_rays(donors.nodes[i], 9, strength, 0.02, -1.0,
					fm_blast_direction(donors.nodes[i], 8), {target, other},
					[&](const FmBlastChildren::Child &child) {
						emitted += child.strength;
						if (child.pos == target)
							newborn += child.strength;
						else if (child.pos != other)
							main += child.strength;
						UASSERT(child.step_cost > 0.0);
					});
			UASSERT(std::abs(emitted - strength) < 1e-10);
			UASSERT(std::abs(main - strength * 0.5) < 1e-10);
			total += emitted;
		}
		UASSERT(total == 4500.0);
		UASSERT(newborn == 1125.0); // Includes all nine donors, not only the parent.
		double retained = 0.0;
		fm_blast_fund_new_rays(parent, 9, 100.0, 0.0, -1.0, fm_blast_direction(parent, 8),
				{},
				[&](const FmBlastChildren::Child &child) { retained += child.strength; });
		UASSERT(retained == 100.0);
	}

	void testAngularUniformAir()
	{
		const double initial = 260.0, loss = 0.02;
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
				UASSERT(hit.energy > hit.cutoff(0.15));
				hit.append_survivors(hit.energy, patches);
			}
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
		for (const auto &[pos, hit] : hits) {
			// An opaque voxel blocks its entire angular region; a resistant
			// neighbor consumes 75% of the energy that crosses it.
			const double fraction = pos == v3pos_t(2, 0, 0)	  ? 0.0
									: pos == v3pos_t(2, 1, 0) ? 0.25
															  : 1.0;
			hit.append_survivors(hit.energy * fraction, patches);
			surviving += hit.energy * fraction;
		}
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
				hit.append_survivors(hit.energy, patches);
			}
			UASSERT(std::abs(energy - surviving) < 1e-8);
		}
	}

	void testAngularSeparateContributions()
	{
		const std::vector<FmBlastFootprint> source{
				{0, -0.2, 0.0, -0.2, 0.2, 10.0}, {0, 0.0, 0.2, -0.2, 0.2, 40.0}};
		const auto hits = fm_blast_angular_project(source, 1);
		UASSERT(hits.size() == 1);
		const auto &hit = hits.at(v3pos_t(1, 0, 0));
		std::vector<FmBlastFootprint> survivors;
		hit.append_survivors(hit.energy * 0.5, survivors);
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
			UASSERT(std::abs(hit.cutoff(0.15) - other.cutoff(0.15)) < 1e-10);
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

	void testZeroEnergy()
	{
		const auto children = fm_blast_children({8, 0, 0}, 9, 0.0, 0.02, 42);
		UASSERT(children.count == 1);
		UASSERT(children.nodes[0].strength == 0.0);
		UASSERT(children.diffuse_strength == 0.0);
	}
};

static TestFmBlastShell g_test_instance;
