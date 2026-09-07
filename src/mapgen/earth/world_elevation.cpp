#include "world_elevation.h"
#include "http.h"
#include "log.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <png.h>

void WorldElevation::load(const std::string &folder)
{
	const std::string name = "World_elevation_map.png";
	const std::string path = folder + "/" + name;
	if (!multi_http_to_file_cdn("earth", name, {}, path))
		return;

	png_image image{};
	image.version = PNG_IMAGE_VERSION;
	// Free libpng state on all exits, including allocation failures.
	struct Cleanup
	{
		png_image &image;
		~Cleanup() { png_image_free(&image); }
	} cleanup{image};
	if (!png_image_begin_read_from_file(&image, path.c_str())) {
		warningstream << "World elevation: " << image.message << '\n';
		return;
	}
	if (!image.width || !image.height || image.width > 21600 || image.height > 10800 ||
			image.width != 2 * image.height) {
		warningstream << "World elevation: invalid image dimensions\n";
		return;
	}
	// Keep one byte per pixel instead of expanding the global map to RGBA.
	image.format = PNG_FORMAT_GRAY;
	std::vector<uint8_t> data(PNG_IMAGE_SIZE(image));
	if (!png_image_finish_read(&image, nullptr, data.data(), 0, nullptr)) {
		warningstream << "World elevation: " << image.message << '\n';
		return;
	}
	width = image.width;
	height = image.height;
	pixels = std::move(data);
}

std::optional<float> WorldElevation::get(const std::string &folder, float lat, float lon)
{
	if (!std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 ||
			lon < -180 || lon > 180)
		return {};
	// No network or image allocation until the first eligible request. Failed
	// loads are also remembered so repeated terrain samples do not retry I/O.
	std::call_once(load_once, [&] { load(folder); });
	if (pixels.empty())
		return {};

	// Equirectangular pixel centres, north at the top. Wrap the date line and
	// clamp the poles so all four interpolation samples stay in bounds.
	const double x = (double(lon) + 180.0) / 360.0 * width - 0.5;
	const double y = std::clamp(
			(90.0 - double(lat)) / 180.0 * height - 0.5, 0.0, double(height - 1));
	const auto ix = static_cast<int64_t>(std::floor(x));
	const auto x0 = static_cast<uint32_t>((ix + width) % width);
	const auto x1 = (x0 + 1) % width;
	const auto y0 = static_cast<uint32_t>(y);
	const auto y1 = std::min(y0 + 1, height - 1);
	const float dx = x - std::floor(x), dy = y - y0;
	const auto sample = [&](uint32_t px, uint32_t py) {
		const float value = pixels[size_t(py) * width + px] - 143.0f;
		// Approximate metres: the source places sea level at #8f8f8f,
		// with black at the Mariana Trench and white at Mount Everest.
		return value < 0 ? value * (11000.0f / 143.0f) : value * (8848.0f / 112.0f);
	};
	return (sample(x0, y0) * (1 - dx) + sample(x1, y0) * dx) * (1 - dy) +
		   (sample(x0, y1) * (1 - dx) + sample(x1, y1) * dx) * dy;
}
