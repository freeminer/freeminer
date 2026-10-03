// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "irr_v3d.h"
#include "mapnode.h"
#include "fm_blast_shell.h"

// Local to explosions: mix every coordinate bit, including signs and X parity.
struct FmBlastPosHash
{
	size_t operator()(const v3pos_t &p) const
	{
		const auto mix = [](uint64_t x) {
			x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
			x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
			return x ^ (x >> 31);
		};
		return mix(mix(static_cast<uint64_t>(static_cast<int64_t>(p.X))) ^
				   mix(static_cast<uint64_t>(static_cast<int64_t>(p.Y)) +
						   0x9e3779b97f4a7c15ULL) ^
				   mix(static_cast<uint64_t>(static_cast<int64_t>(p.Z)) +
						   0x3c6ef372fe94f82aULL));
	}
};
template <typename T>
using FmBlastMap = std::unordered_map<v3pos_t, T, FmBlastPosHash>;
using FmBlastSet = std::unordered_set<v3pos_t, FmBlastPosHash>;

struct FmBlastFootprint
{
	int face = 0;
	double u0 = 0, u1 = 0, v0 = 0, v1 = 0;
	double density = 0;
	double solid_angle = 0;

	FmBlastFootprint() = default;
	FmBlastFootprint(
			int face_, double u0_, double u1_, double v0_, double v1_, double density_) :
			face(face_), u0(u0_), u1(u1_), v0(v0_), v1(v1_), density(density_)
	{
		const auto integral = [](double u, double v) {
			return std::atan2(u * v, std::sqrt(1.0 + u * u + v * v));
		};
		if (u1 > u0 && v1 > v0)
			solid_angle = std::max(0.0, integral(u1, v1) - integral(u0, v1) -
												integral(u1, v0) + integral(u0, v0));
	}
	double area() const { return solid_angle; }
};

inline std::vector<FmBlastFootprint> fm_blast_angular_seed(double energy)
{
	std::vector<FmBlastFootprint> result;
	result.reserve(6);
	for (int face = 0; face < 6; ++face)
		result.emplace_back(face, -1.0, 1.0, -1.0, 1.0, energy / (4.0 * std::acos(-1.0)));
	return result;
}

// One record owns transport geometry and the engine's current node-hit state.
struct FmBlastAngularHit
{
	size_t first = 0, count = 0;
	double energy = 0.0;
	double projected_energy = 0.0;
	double area = 0.0;
	double distance_cost = 0.0;
	MapNode node;
	bool loaded = false;
	bool protected_node = false;
	bool absorbed_tnt = false;
	FmBlastOutcome outcome = FmBlastOutcome::Blocked;
	// The cutoff is energy per node, not density: tiny distant hits must expire.
	bool can_travel(double minimum) const { return energy > minimum; }
};

struct FmBlastAngularHits
{
	FmBlastMap<FmBlastAngularHit> hits;
	std::vector<FmBlastFootprint> patches;
	auto begin() { return hits.begin(); }
	auto end() { return hits.end(); }
	auto begin() const { return hits.begin(); }
	auto end() const { return hits.end(); }
	size_t size() const { return hits.size(); }
	size_t count(const v3pos_t &pos) const { return hits.count(pos); }
	auto &at(const v3pos_t &pos) { return hits.at(pos); }
	const auto &at(const v3pos_t &pos) const { return hits.at(pos); }
	std::span<FmBlastFootprint> regions(const FmBlastAngularHit &hit)
	{
		return std::span(patches).subspan(hit.first, hit.count);
	}
	std::span<const FmBlastFootprint> regions(const FmBlastAngularHit &hit) const
	{
		return std::span(patches).subspan(hit.first, hit.count);
	}
	double energy() const
	{
		double total = 0;
		for (const auto &[pos, hit] : hits)
			total += hit.energy;
		return total;
	}
	void append_survivors(std::vector<FmBlastFootprint> &out) const
	{
		out.reserve(out.size() + patches.size());
		for (const auto &[pos, hit] : hits) {
			if (hit.energy <= 0.0 || hit.projected_energy <= 0.0)
				continue;
			const double scale = hit.energy / hit.projected_energy;
			for (auto patch : regions(hit)) {
				patch.density *= scale;
				if (patch.density > 0.0)
					out.push_back(patch);
			}
		}
	}
};

struct FmBlastPatchEntry
{
	v3pos_t pos;
	FmBlastFootprint patch;
};
inline FmBlastAngularHits fm_blast_pack(const std::vector<FmBlastPatchEntry> &entries)
{
	FmBlastAngularHits result;
	result.hits.reserve(entries.size());
	for (const auto &entry : entries) {
		auto &hit = result.hits[entry.pos];
		++hit.count;
		hit.area += entry.patch.area();
		hit.energy += entry.patch.density * entry.patch.area();
	}
	size_t offset = 0;
	for (auto &[pos, hit] : result) {
		hit.first = offset;
		offset += hit.count;
		hit.count = 0;
		hit.projected_energy = hit.energy;
	}
	result.patches.resize(offset);
	for (const auto &entry : entries) {
		auto &hit = result.at(entry.pos);
		result.patches[hit.first + hit.count++] = entry.patch;
	}
	return result;
}

inline FmBlastAngularHits fm_blast_angular_project(
		const std::vector<FmBlastFootprint> &patches, int shell)
{
	std::vector<FmBlastPatchEntry> entries;
	if (shell <= 0)
		return {};
	entries.reserve(patches.size() * 4);
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
				FmBlastFootprint child(patch.face, std::max(patch.u0, (u - 0.5) / shell),
						std::min(patch.u1, (u + 0.5) / shell),
						std::max(patch.v0, (v - 0.5) / shell),
						std::min(patch.v1, (v + 0.5) / shell), patch.density);
				if (child.area() <= 0.0)
					continue;
				std::array<pos_t, 3> p;
				const int axis = patch.face / 2;
				p[axis] = static_cast<pos_t>(patch.face % 2 ? -shell : shell);
				p[(axis + 1) % 3] = static_cast<pos_t>(u);
				p[(axis + 2) % 3] = static_cast<pos_t>(v);
				entries.push_back({v3pos_t(p[0], p[1], p[2]), child});
			}
	}
	return fm_blast_pack(entries);
}

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
						const double area = last.area() + p.area();
						last.density =
								(last.area() * last.density + p.area() * p.density) /
								area;
						if (axis == 0)
							last.u1 = p.u1;
						else
							last.v1 = p.v1;
						last.solid_angle = area;
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

// Split base minus cut into disjoint rectangles. Both lie on one cube face.
inline void fm_blast_subtract(const FmBlastFootprint &base, const FmBlastFootprint &cut,
		std::vector<FmBlastFootprint> &out)
{
	const double u0 = std::max(base.u0, cut.u0), u1 = std::min(base.u1, cut.u1);
	const double v0 = std::max(base.v0, cut.v0), v1 = std::min(base.v1, cut.v1);
	if (u0 >= u1 || v0 >= v1) {
		out.push_back(base);
		return;
	}
	const auto add = [&](double a, double b, double c, double d) {
		if (b > a && d > c)
			out.emplace_back(base.face, a, b, c, d, base.density);
	};
	add(base.u0, u0, base.v0, base.v1);
	add(u1, base.u1, base.v0, base.v1);
	add(u0, u1, base.v0, v0);
	add(u0, u1, v1, base.v1);
}

inline void fm_blast_angular_mix_core(
		FmBlastAngularHits &hits, int shell, double fraction)
{
	fraction = fm_blast_fraction(fraction, 0.0);
	if (fraction <= 0.0 || hits.energy() <= 0.0)
		return;
	auto surface = fm_blast_angular_project(fm_blast_angular_seed(0.0), shell);
	const double share = hits.energy() * fraction / surface.size();
	std::vector<FmBlastPatchEntry> entries;
	entries.reserve(hits.patches.size() + surface.patches.size());
	std::vector<FmBlastFootprint> gaps, next_gaps;
	for (const auto &[pos, full] : surface) {
		const double background = share / full.area;
		const auto old = hits.hits.find(pos);
		for (auto base : surface.regions(full)) {
			base.density = background;
			gaps.clear();
			gaps.push_back(base);
			if (old != hits.end() && fraction < 1.0) {
				for (auto patch : hits.regions(old->second)) {
					if (patch.face != base.face)
						continue;
					// Preserve every old density; add the pooled background once.
					const double retained =
							old->second.projected_energy > 0.0
									? old->second.energy / old->second.projected_energy
									: 0.0;
					patch.density =
							patch.density * retained * (1.0 - fraction) + background;
					entries.push_back({pos, patch});
					next_gaps.clear();
					for (const auto &gap : gaps)
						fm_blast_subtract(gap, patch, next_gaps);
					gaps.swap(next_gaps);
				}
			}
			for (const auto &gap : gaps)
				entries.push_back({pos, gap});
		}
	}
	hits = fm_blast_pack(entries);
}

inline void fm_blast_angular_distance(FmBlastAngularHits &hits, double loss)
{
	const double node_loss = std::max(0.0, loss);
	for (auto &[pos, hit] : hits) {
		// Charge once per node, regardless of angular area or patch count.
		// append_survivors applies this loss proportionally to all contributions.
		hit.distance_cost = std::min(hit.energy, node_loss);
		hit.energy -= hit.distance_cost;
	}
}
