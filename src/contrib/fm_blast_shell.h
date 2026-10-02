// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "irr_v3d.h"

using FmBlastDirection = std::array<double, 3>;

inline FmBlastDirection fm_blast_direction(const v3pos_t &pos, int shell)
{
	if (shell <= 0)
		return {};
	return {static_cast<double>(pos.X) / shell, static_cast<double>(pos.Y) / shell,
			static_cast<double>(pos.Z) / shell};
}

inline v3pos_t fm_blast_direction_destination(
		const FmBlastDirection &direction, int shell)
{
	return {static_cast<pos_t>(std::round(direction[0] * shell)),
			static_cast<pos_t>(std::round(direction[1] * shell)),
			static_cast<pos_t>(std::round(direction[2] * shell))};
}

struct FmBlastDirectionState
{
	FmBlastDirection direction{};
	double strength = -1.0;
};

struct FmBlastChildren
{
	struct Child
	{
		v3pos_t pos;
		double strength;
		double step_cost;
		FmBlastDirection direction;
	};
	std::array<Child, 26> nodes;
	size_t count = 0;
	double diffuse_strength = 0.0;
};

// Share only among already-reached destinations; never introduce new positions.
template <typename Weights, typename Steps>
inline void fm_blast_distribute_pool(
		Weights &weights, Steps &steps, double strength, double distance_loss)
{
	if (strength <= 0.0 || weights.empty())
		return;
	const double share = strength / static_cast<double>(weights.size());
	for (auto &weight : weights) {
		weight.second += share;
		steps[weight.first] += share * distance_loss;
	}
}

// Initial blast strength is a cube of the nominal diameter: (2 * radius + 1)^3.
inline double fm_blast_full_shell_radius(double initial_strength)
{
	if (!std::isfinite(initial_strength) || initial_strength <= 0.0)
		return 0.0;
	return std::max(1.0, std::floor((std::cbrt(initial_strength) - 1.0) * 0.5));
}

inline double fm_blast_core_radius(double requested, double initial_strength)
{
	return std::isfinite(requested) ? std::max(0.0, std::floor(requested))
									: fm_blast_full_shell_radius(initial_strength);
}

inline double fm_blast_shell_transfer_fraction(
		int shell, double core_radius, double core_fraction, double outer_fraction)
{
	return shell <= core_radius ? core_fraction : outer_fraction;
}

// Near the origin share over the complete shell. Farther out, keep sharing
// within the destinations reached by surviving parents.
template <typename Weights, typename Steps, typename MakeKey>
inline void fm_blast_distribute_shell_pool(Weights &weights, Steps &steps,
		double strength, double distance_loss, int shell, double initial_strength,
		const MakeKey &make_key, double core_radius = -1.0)
{
	if (strength <= 0.0 || shell <= 0)
		return;
	if (shell <= (core_radius >= 0.0 ? core_radius
									 : fm_blast_full_shell_radius(initial_strength))) {
		const auto seed = [&](int x, int y, int z) {
			weights.try_emplace(make_key(v3pos_t(x, y, z)), 0.0);
		};
		for (int x = -shell; x <= shell; x += 2 * shell)
			for (int y = -shell; y <= shell; ++y)
				for (int z = -shell; z <= shell; ++z)
					seed(x, y, z);
		for (int y = -shell; y <= shell; y += 2 * shell)
			for (int x = 1 - shell; x < shell; ++x)
				for (int z = -shell; z <= shell; ++z)
					seed(x, y, z);
		for (int z = -shell; z <= shell; z += 2 * shell)
			for (int x = 1 - shell; x < shell; ++x)
				for (int y = 1 - shell; y < shell; ++y)
					seed(x, y, z);
	}
	fm_blast_distribute_pool(weights, steps, strength, distance_loss);
}

inline double fm_blast_fraction(double value, double fallback)
{
	return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : fallback;
}

struct FmBlastEnergySplit
{
	double ray;
	double shell;
};

inline FmBlastEnergySplit fm_blast_split(double strength, double shell_fraction)
{
	const double shell = strength * shell_fraction;
	return {strength - shell, shell};
}

// Keep automatic mode distinct from an explicit zero (no new rays).
inline double fm_blast_new_ray_fraction(double requested)
{
	return std::isfinite(requested) && requested >= 0.0
				   ? fm_blast_fraction(requested, 0.5)
				   : 0.5;
}

// Project a shell node along its center direction, with deterministic rounding.
inline v3pos_t fm_blast_project(const v3pos_t &pos, int from_shell, int to_shell)
{
	const auto component = [&](pos_t value) {
		return static_cast<pos_t>(
				std::round(static_cast<double>(value) * to_shell / from_shell));
	};
	return {component(pos.X), component(pos.Y), component(pos.Z)};
}

// Pool a configurable share for shell distribution. Split the local remainder
// toward the center-to-parent direction and only into newly appearing rays.
inline FmBlastChildren fm_blast_children(const v3pos_t &parent, int shell,
		double strength, double distance_loss, uint32_t /*seed*/,
		double shell_fraction = 0.1, double scatter_fraction = -1.0,
		const FmBlastDirection *inherited_direction = nullptr,
		const std::function<bool(const v3pos_t &)> &has_continuation = {})
{
	FmBlastChildren result;
	const auto shell_distance = [](const v3pos_t &p) {
		return std::max({std::abs(static_cast<int>(p.X)), std::abs(static_cast<int>(p.Y)),
				std::abs(static_cast<int>(p.Z))});
	};
	if (shell <= 0 || shell_distance(parent) != shell - 1)
		return result;

	std::array<v3pos_t, 26> choices;
	size_t count = 0;
	for (pos_t x = -1; x <= 1; ++x)
		for (pos_t y = -1; y <= 1; ++y)
			for (pos_t z = -1; z <= 1; ++z) {
				const v3pos_t child = parent + v3pos_t(x, y, z);
				if (shell_distance(child) == shell)
					choices[count++] = child;
			}
	if (count == 0)
		return result;
	const auto energy = fm_blast_split(strength, fm_blast_fraction(shell_fraction, 0.1));
	result.diffuse_strength = energy.shell;
	const auto add_child = [&](size_t index, const v3pos_t &pos, double amount) {
		const v3pos_t step = pos - parent;
		const double cost = distance_loss *
							std::sqrt(static_cast<double>(
									step.X * step.X + step.Y * step.Y + step.Z * step.Z));
		result.nodes[index] = {pos, amount, cost, fm_blast_direction(pos, shell)};
	};
	// There is no center-to-parent direction at the origin: seed all 26 neighbors.
	if (shell == 1) {
		result.count = count;
		for (size_t i = 0; i < count; ++i)
			add_child(i, choices[i], energy.ray / count);
	} else {
		const auto direction = inherited_direction
									   ? *inherited_direction
									   : fm_blast_direction(parent, shell - 1);
		const v3pos_t forward = fm_blast_direction_destination(direction, shell);
		// A new direction has no main continuation from the previous shell.
		// Inward projection gives each new node exactly one geometrical parent;
		// blocked/missing parents cannot be replaced by neighboring active rays.
		size_t newborn_count = 0;
		for (size_t i = 0; i < count; ++i) {
			const auto child = choices[i];
			if (child != forward && fm_blast_project(child, shell, shell - 1) == parent &&
					(!has_continuation || !has_continuation(child)))
				choices[newborn_count++] = child;
		}
		const double requested = fm_blast_new_ray_fraction(scatter_fraction);
		const double newborn_fraction = newborn_count > 0 ? requested : 0.0;
		const double newborn_strength = energy.ray * newborn_fraction;
		if (newborn_fraction < 1.0) {
			add_child(result.count++, forward, energy.ray - newborn_strength);
			result.nodes[result.count - 1].direction = direction;
		}
		if (newborn_fraction > 0.0) {
			const double share = newborn_strength / newborn_count;
			for (size_t i = 0; i < newborn_count; ++i)
				add_child(result.count++, choices[i], share);
		}
	}
	return result;
}

struct FmBlastDonors
{
	std::array<v3pos_t, 9> nodes;
	size_t count = 0;
};

// Use the directional parent and its eight nearest active surface neighbors.
// Search only immediate neighbors, so a gap cannot pull energy from distant rays.
template <typename IsActive>
inline FmBlastDonors fm_blast_new_ray_donors(
		const v3pos_t &parent, int shell, const IsActive &is_active)
{
	FmBlastDonors donors;
	if (!is_active(parent))
		return donors;
	donors.nodes[donors.count++] = parent;
	std::vector<v3pos_t> neighbors;
	for (pos_t x = -1; x <= 1; ++x)
		for (pos_t y = -1; y <= 1; ++y)
			for (pos_t z = -1; z <= 1; ++z) {
				const v3pos_t pos = parent + v3pos_t(x, y, z);
				if (pos != parent &&
						std::max({std::abs(static_cast<int>(pos.X)),
								std::abs(static_cast<int>(pos.Y)),
								std::abs(static_cast<int>(pos.Z))}) == shell &&
						is_active(pos))
					neighbors.push_back(pos);
			}
	const auto distance = [&](const v3pos_t &pos) {
		const auto step = pos - parent;
		return step.X * step.X + step.Y * step.Y + step.Z * step.Z;
	};
	std::stable_sort(neighbors.begin(), neighbors.end(),
			[&](const auto &a, const auto &b) { return distance(a) < distance(b); });
	for (size_t i = 0; i < std::min<size_t>(8, neighbors.size()); ++i)
		donors.nodes[donors.count++] = neighbors[i];
	return donors;
}

// Spend one donor's bounded budget across all newborn rays it supports.
// Call after collecting targets from every directional parent in this shell.
template <typename Emit>
inline void fm_blast_fund_new_rays(const v3pos_t &parent, int shell, double strength,
		double distance_loss, double requested, const FmBlastDirection &direction,
		const std::vector<v3pos_t> &targets, const Emit &emit)
{
	const double fraction = targets.empty() ? 0.0 : fm_blast_new_ray_fraction(requested);
	const double budget = strength * fraction;
	const auto add = [&](const v3pos_t &pos, double amount,
							 const FmBlastDirection &child_direction) {
		const auto step = pos - parent;
		const double cost = distance_loss *
							std::sqrt(static_cast<double>(
									step.X * step.X + step.Y * step.Y + step.Z * step.Z));
		emit(FmBlastChildren::Child{pos, amount, cost, child_direction});
	};
	if (fraction < 1.0)
		add(fm_blast_direction_destination(direction, shell), strength - budget,
				direction);
	if (fraction > 0.0) {
		const double share = budget / targets.size();
		for (const auto &target : targets)
			add(target, share, fm_blast_direction(target, shell));
	}
}
