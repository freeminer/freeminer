
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

size_t http_to_file(const std::string &url, const std::string &zipfull);
// Fetch exactly one HTTP byte range. Returns an empty string unless the
// server honoured the Range request with HTTP 206, preventing accidental
// downloads of complete multi-hundred-megabyte COG files.
std::string http_get_range(const std::string &url, std::uint64_t offset,
		std::uint64_t length);
size_t multi_http_to_file(const std::string &zipfile,
		const std::vector<std::string> &links, std::string zipfull = {});
size_t multi_http_to_file(
		const std::vector<std::string> &links, const std::string &zipfull);
size_t multi_http_to_file_cdn(const std::string &dir, const std::string &name,
		std::vector<std::string> links, const std::string &path = {});

std::string exec_to_string(const std::string &cmd);
