// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <vector>
#include "irr_v3d.h"
#include "util/unordered_map_hash.h"

// Rectangles on the six faces of a unit cube describe persistent angular regions.
// Density is energy per steradian; splitting a region never changes its density.
struct FmBlastFootprint
{
	int face;
	double u0, u1, v0, v1;
	double density;

	double area() const
	{
		const auto integral = [](double u, double v) {
			return std::atan2(u * v, std::sqrt(1.0 + u * u + v * v));
		};
		return integral(u1, v1) - integral(u0, v1) - integral(u1, v0) + integral(u0, v0);
	}
};

inline double fm_blast_reference_area()
{
	// Preserve the old first-shell energy scale for air loss and cutoff settings.
	return 4.0 * std::acos(-1.0) / 26.0;
}

inline std::vector<FmBlastFootprint> fm_blast_angular_seed(double energy)
{
	std::vector<FmBlastFootprint> result;
	for (int face = 0; face < 6; ++face)
		result.push_back({face, -1.0, 1.0, -1.0, 1.0, energy / (4.0 * std::acos(-1.0))});
	return result;
}

struct FmBlastAngularHit
{
	std::vector<FmBlastFootprint> patches;
	double energy = 0.0;
	double area = 0.0;
	double distance_cost = 0.0;

	// Material loss or a local TNT boost changes every incoming contribution by
	// the same factor. Directions and angular boundaries are never merged here.
	void append_survivors(double remaining, std::vector<FmBlastFootprint> &out) const
	{
		if (remaining <= 0.0 || energy <= 0.0)
			return;
		const double scale = remaining / energy;
		for (auto patch : patches) {
			patch.density *= scale;
			if (patch.density > 0.0)
				out.push_back(patch);
		}
	}

	double cutoff(double minimum) const
	{
		return minimum * area / fm_blast_reference_area();
	}
};

using FmBlastAngularHits = unordered_map_v3pos<FmBlastAngularHit>;

// Intersect each footprint with the angular projection of every covered voxel.
// Face edges/corners contribute disjoint angular pieces to the same voxel.
inline FmBlastAngularHits fm_blast_angular_project(
		const std::vector<FmBlastFootprint> &patches, int shell)
{
	FmBlastAngularHits hits;
	if (shell <= 0)
		return hits;
	for (const auto &patch : patches) {
		const int first_u =
				std::max(-shell, static_cast<int>(std::floor(patch.u0 * shell + 0.5)));
		const int last_u =
				std::min(shell, static_cast<int>(std::floor(patch.u1 * shell + 0.5)));
		const int first_v =
				std::max(-shell, static_cast<int>(std::floor(patch.v0 * shell + 0.5)));
		const int last_v =
				std::min(shell, static_cast<int>(std::floor(patch.v1 * shell + 0.5)));
		for (int u = first_u; u <= last_u; ++u)
			for (int v = first_v; v <= last_v; ++v) {
				FmBlastFootprint child{patch.face, std::max(patch.u0, (u - 0.5) / shell),
						std::min(patch.u1, (u + 0.5) / shell),
						std::max(patch.v0, (v - 0.5) / shell),
						std::min(patch.v1, (v + 0.5) / shell), patch.density};
				if (child.u1 <= child.u0 || child.v1 <= child.v0)
					continue;
				const double area = child.area();
				if (area <= 0.0)
					continue;
				std::array<pos_t, 3> components;
				const int axis = patch.face / 2;
				components[axis] = static_cast<pos_t>(patch.face % 2 ? -shell : shell);
				components[(axis + 1) % 3] = static_cast<pos_t>(u);
				components[(axis + 2) % 3] = static_cast<pos_t>(v);
				auto &hit = hits[v3pos_t(components[0], components[1], components[2])];
				hit.patches.push_back(child);
				hit.area += area;
				hit.energy += child.density * area;
			}
	}
	return hits;
}

// Rejoin only adjacent regions with the same energy density. This removes
// temporary voxel boundaries in air without averaging away material shadows.
inline void fm_blast_angular_coalesce(std::vector<FmBlastFootprint> &patches)
{
	bool changed;
	do {
		const auto old_size = patches.size();
		for (int axis = 0; axis < 2; ++axis) {
			const auto key = [axis](const FmBlastFootprint &p) {
				return axis == 0 ? std::make_tuple(p.face, p.v0, p.v1, p.u0, p.u1)
								 : std::make_tuple(p.face, p.u0, p.u1, p.v0, p.v1);
			};
			std::sort(patches.begin(), patches.end(),
					[&](const auto &a, const auto &b) { return key(a) < key(b); });
			size_t count = 0;
			for (const auto &p : patches) {
				if (count > 0) {
					auto &last = patches[count - 1];
					const bool adjacent = axis == 0
												  ? last.v0 == p.v0 && last.v1 == p.v1 &&
															last.u1 == p.u0
												  : last.u0 == p.u0 && last.u1 == p.u1 &&
															last.v1 == p.v0;
					const double tolerance =
							1e-12 *
							std::max({1.0, std::abs(last.density), std::abs(p.density)});
					if (last.face == p.face && adjacent &&
							std::abs(last.density - p.density) <= tolerance) {
						const double a = last.area(), b = p.area();
						last.density = (a * last.density + b * p.density) / (a + b);
						if (axis == 0)
							last.u1 = p.u1;
						else
							last.v1 = p.v1;
						continue;
					}
				}
				patches[count++] = p;
			}
			patches.resize(count);
		}
		changed = patches.size() < old_size;
	} while (changed);
}

// Deliberate full-surface mixing is confined to the configured cubic core.
inline void fm_blast_angular_mix_core(
		FmBlastAngularHits &hits, int shell, double fraction)
{
	fraction = std::clamp(fraction, 0.0, 1.0);
	if (fraction <= 0.0)
		return;
	double total = 0.0;
	for (const auto &[pos, hit] : hits)
		total += hit.energy;
	if (total <= 0.0)
		return;
	auto surface = fm_blast_angular_project(fm_blast_angular_seed(0.0), shell);
	const double share = total * fraction / surface.size();
	for (auto &[pos, hit] : surface) {
		const auto old = hits.find(pos);
		hit.energy =
				share + (old != hits.end() ? old->second.energy * (1.0 - fraction) : 0.0);
		for (auto &patch : hit.patches)
			patch.density = hit.energy / hit.area;
	}
	hits = std::move(surface);
}

// Both loss and cutoff are per angular area, not per fragment or recipient cell.
inline void fm_blast_angular_distance(FmBlastAngularHits &hits, double loss)
{
	const double density_loss = std::max(0.0, loss) / fm_blast_reference_area();
	for (auto &[pos, hit] : hits) {
		const double before = hit.energy;
		hit.energy = 0.0;
		for (auto &patch : hit.patches) {
			patch.density = std::max(0.0, patch.density - density_loss);
			hit.energy += patch.density * patch.area();
		}
		hit.distance_cost = std::max(0.0, before - hit.energy);
	}
}
