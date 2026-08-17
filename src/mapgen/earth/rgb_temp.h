// write function on c++ : params color in rgb encoding temperature from blue to red and returns temperature in celsius

/*
I'm considering how to solve for inverse mapping by finding the parameter \( t \) that minimizes error. A brute-force search over the range [0, 1] with small steps or Newton's method could work, but implementing a numerical search sounds more robust. I can sample 1,000 points and refine via binary search for better matching. I’ll provide both a straightforward linear method and a more intricate jet inverse approach using C++17 with relevant examples. I’ll format everything in markdown!**Coding RGB to Temperature Conversion**

I’m writing code that includes the `cmath` and `algorithm` libraries. First, I’ll define a function, `rgbToTemperatureLinear`, that converts RGB values to a temperature range based on given minimum and maximum temperatures. I’ll also implement `rgbToTemperatureJet`, which uses formulas to calculate values for red, green, and blue based on a temperature parameter \( t \). Next, I’ll execute a coarse search with 2,000 samples to find the best \( t \), and refine that with smaller steps to minimize the squared error.**Refining the Temperature Mapping**

Next, I plan to refine the search around the best \( t \) value found earlier by using either the golden-section method or a simpler approach with a loop from 0 to 5. I’ll search within the range defined by \( t_{\text{best}} \) plus and minus a small delta, using a finer step of \( K = 200 \) to compute any necessary updates. As I decrease the delta for convergence, I'll ensure the final result is clear: returning the calculated temperature. 

I'll also include usage examples and note important considerations like potential ambiguities in mapping and the presence of noise.</div>Below are two C++ functions you can use to convert an RGB color (assumed to encode a temperature on a blue→red scale) back to a temperature in Celsius.

- The first function (rgbToCelsiusLinear) assumes a simple linear gradient from pure blue (0,0,255) at the cold end to pure red (255,0,0) at the hot end. It is cheap and analytic.
- The second function (rgbToCelsiusJet) handles a common “rainbow/jet-like” thermal map (blue→cyan→green→yellow→red) by numerically inverting the forward jet colormap. This is a more realistic thermal palette but requires sampling/search so it’s a bit heavier.

Both functions take r,g,b in 0..255 and minTemp/maxTemp in Celsius (defaults given) and return a double temperature in Celsius.

Code (C++17):
*/
#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>

// clamp helper
static inline double clamp01(double x)
{
	return std::max(0.0, std::min(1.0, x));
}

// 1) Simple linear blue->red gradient inversion
// Maps: blue (0,0,255) -> t=0  ; red (255,0,0) -> t=1
// r,g,b are 0..255, returns temperature in Celsius between minTemp and maxTemp.
double rgbToCelsiusLinear(
		int r, int g, int b, double minTemp = -30.0, double maxTemp = 50.0)
{
	double rn = clamp01(r / 255.0);
	double bn = clamp01(b / 255.0);

	// projection onto the R<-B gradient: t = (r_norm - b_norm + 1) / 2
	double t = (rn - bn + 1.0) / 2.0;
	t = clamp01(t);

	return minTemp + t * (maxTemp - minTemp);
}

// 2) Invert a typical "jet" / rainbow colormap numerically.
// Forward jet mapping (t in [0,1]) used here:
// r = clamp(1.5 - |4t - 3|)
// g = clamp(1.5 - |4t - 2|)
// b = clamp(1.5 - |4t - 1|)
static inline std::array<double, 3> jetColor(double t)
{
	t = clamp01(t);
	double r = clamp01(1.5 - std::fabs(4.0 * t - 3.0));
	double g = clamp01(1.5 - std::fabs(4.0 * t - 2.0));
	double b = clamp01(1.5 - std::fabs(4.0 * t - 1.0));
	return {r, g, b};
}

// invert jet colormap by coarse-to-fine search
double rgbToCelsiusJet(int r, int g, int b, double minTemp = -30.0, double maxTemp = 50.0)
{
	const double rn = clamp01(r / 255.0);
	const double gn = clamp01(g / 255.0);
	const double bn = clamp01(b / 255.0);

	// coarse sampling to get initial guess
	int coarseN = 800; // coarse samples (adjust for speed/precision)
	double bestT = 0.0;
	double bestErr = std::numeric_limits<double>::infinity();

	for (int i = 0; i < coarseN; ++i) {
		double t = double(i) / double(coarseN - 1);
		auto c = jetColor(t);
		double err = (c[0] - rn) * (c[0] - rn) + (c[1] - gn) * (c[1] - gn) +
					 (c[2] - bn) * (c[2] - bn);
		if (err < bestErr) {
			bestErr = err;
			bestT = t;
		}
	}

	// refine around bestT with a few finer searches
	double delta = 1.0 / double(coarseN - 1);
	for (int pass = 0; pass < 4; ++pass) {
		double start = clamp01(bestT - delta);
		double end = clamp01(bestT + delta);
		int fineN = 200;
		double localBestT = bestT;
		double localBestErr = bestErr;
		for (int i = 0; i < fineN; ++i) {
			double t = start + (end - start) * double(i) / double(fineN - 1);
			auto c = jetColor(t);
			double err = (c[0] - rn) * (c[0] - rn) + (c[1] - gn) * (c[1] - gn) +
						 (c[2] - bn) * (c[2] - bn);
			if (err < localBestErr) {
				localBestErr = err;
				localBestT = t;
			}
		}
		bestT = localBestT;
		bestErr = localBestErr;
		delta *= 0.25; // shrink search radius
	}

	double temp = minTemp + bestT * (maxTemp - minTemp);
	return temp;
}

/*
Usage examples:
- If your color map is exactly linear blue->red use rgbToCelsiusLinear.
- If your color map is a rainbow/thermal (blue→cyan→green→yellow→red) use rgbToCelsiusJet.

Notes and caveats:
- The mapping from color to temperature is not unique without knowing the exact colormap and the numeric temperature range. You must supply the min and max temperatures that correspond to the cold and hot ends of the colormap.
- Real images may contain noise, gamma correction or encoding differences; you may need to pre-correct for gamma or color-space conversions if colors appear non-linear.
- If your colormap is different, provide its forward mapping and invert it similarly (analytic or numeric search).
- If you expect many conversions, precompute a lookup table (LUT) from colors to temperatures for speed.

If you tell me which exact colormap and numeric temperature range you use (and whether input is linear RGB or sRGB), I can tailor the function exactly to that mapping.
*/