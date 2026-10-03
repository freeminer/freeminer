// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

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

enum class FmBlastOutcome
{
	Transparent,
	Removed,
	Transformed,
	Blocked,
	CallbackPending
};

inline bool fm_blast_can_propagate(FmBlastOutcome outcome, bool solid)
{
	return outcome == FmBlastOutcome::Removed || outcome == FmBlastOutcome::Transformed ||
		   (!solid && (outcome == FmBlastOutcome::Transparent ||
							  outcome == FmBlastOutcome::CallbackPending));
}
