#include <string>
#include <string_view>
#include <optional>
#include <tuple>
#include <cctype>
#include <algorithm>
#include <cstdint>
#include <charconv>

#include "colors.h"

std::optional<RGBTuple> color_text_to_rgb_tuple(std::string_view text)
{
	if (auto rgb = full_hex_color_to_rgb_tuple(text))
		return rgb;
	if (auto rgb = short_hex_color_to_rgb_tuple(text))
		return rgb;
	if (auto rgb = color_name_to_rgb_tuple(text))
		return rgb;
	return std::nullopt;
}
std::uint32_t rgb_distance(const RGBTuple &from, const RGBTuple &to)
{
	std::int32_t dr = static_cast<std::int32_t>(std::get<0>(from)) -
					  static_cast<std::int32_t>(std::get<0>(to));
	std::int32_t dg = static_cast<std::int32_t>(std::get<1>(from)) -
					  static_cast<std::int32_t>(std::get<1>(to));
	std::int32_t db = static_cast<std::int32_t>(std::get<2>(from)) -
					  static_cast<std::int32_t>(std::get<2>(to));
	std::int32_t dist = dr * dr + dg * dg + db * db;
	return static_cast<std::uint32_t>(dist);
}
 std::optional<RGBTuple> color_name_to_rgb_tuple(std::string_view text)
{
	if (text == "aqua" || text == "cyan")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 255, 255));
	if (text == "beige")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(187, 173, 142));
	if (text == "black")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 0, 0));
	if (text == "blue")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 0, 255));
	if (text == "brown")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(128, 64, 0));
	if (text == "fuchsia" || text == "magenta")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(255, 0, 255));
	if (text == "gray" || text == "grey")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(128, 128, 128));
	if (text == "green")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 128, 0));
	if (text == "lime")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 255, 0));
	if (text == "maroon")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(128, 0, 0));
	if (text == "navy")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 0, 128));
	if (text == "olive")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(128, 128, 0));
	if (text == "orange")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(255, 128, 0));
	if (text == "purple")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(128, 0, 128));
	if (text == "red")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(255, 0, 0));
	if (text == "silver")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(192, 192, 192));
	if (text == "teal")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(0, 128, 0));
	if (text == "white")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(255, 255, 255));
	if (text == "yellow")
		return std::make_optional(
				std::make_tuple<std::uint8_t, std::uint8_t, std::uint8_t>(255, 255, 0));
	return std::nullopt;
}
 std::optional<RGBTuple> short_hex_color_to_rgb_tuple(std::string_view text)
{
	if (text.size() != 4 || text.front() != '#' ||
			!std::all_of(text.begin() + 1, text.end(), [](char c) {
				return std::isxdigit(static_cast<unsigned char>(c)) != 0;
			})) {
		return std::nullopt;
	}

	unsigned int v;
	auto res = std::from_chars(text.data() + 1, text.data() + 2, v, 16);
	if (res.ec != std::errc())
		return std::nullopt;
	std::uint8_t r = static_cast<std::uint8_t>(v);
	r = static_cast<std::uint8_t>(r | (r << 4));

	res = std::from_chars(text.data() + 2, text.data() + 3, v, 16);
	if (res.ec != std::errc())
		return std::nullopt;
	std::uint8_t g = static_cast<std::uint8_t>(v);
	g = static_cast<std::uint8_t>(g | (g << 4));

	res = std::from_chars(text.data() + 3, text.data() + 4, v, 16);
	if (res.ec != std::errc())
		return std::nullopt;
	std::uint8_t b = static_cast<std::uint8_t>(v);
	b = static_cast<std::uint8_t>(b | (b << 4));

	return std::make_optional(std::make_tuple(r, g, b));
}
 std::optional<RGBTuple> full_hex_color_to_rgb_tuple(std::string_view text)
{
	if (text.size() != 7 || text.front() != '#' ||
			!std::all_of(text.begin() + 1, text.end(), [](char c) {
				return std::isxdigit(static_cast<unsigned char>(c)) != 0;
			})) {
		return std::nullopt;
	}

	unsigned int v;
	auto res = std::from_chars(text.data() + 1, text.data() + 3, v, 16);
	if (res.ec != std::errc())
		return std::nullopt;
	std::uint8_t r = static_cast<std::uint8_t>(v);

	res = std::from_chars(text.data() + 3, text.data() + 5, v, 16);
	if (res.ec != std::errc())
		return std::nullopt;
	std::uint8_t g = static_cast<std::uint8_t>(v);

	res = std::from_chars(text.data() + 5, text.data() + 7, v, 16);
	if (res.ec != std::errc())
		return std::nullopt;
	std::uint8_t b = static_cast<std::uint8_t>(v);

	return std::make_optional(std::make_tuple(r, g, b));
}
