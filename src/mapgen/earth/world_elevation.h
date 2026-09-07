// World elevation map by Avsa (NASA/GEBCO source data), CC BY-SA 4.0.
// https://commons.wikimedia.org/wiki/File:World_elevation_map.png
// https://creativecommons.org/licenses/by-sa/4.0/
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class WorldElevation
{
public:
	std::optional<float> get(const std::string &folder, float lat, float lon);

private:
	std::once_flag load_once;
	std::vector<uint8_t> pixels;
	uint32_t width = 0, height = 0;
	void load(const std::string &folder);
};
