
#include <cstddef>
#include <string>
#include <vector>

size_t http_to_file(const std::string &url, const std::string &zipfull);
size_t multi_http_to_file(const std::string &zipfile,
		const std::vector<std::string> &links, const std::string &zipfull);
size_t multi_http_to_file(
		const std::vector<std::string> &links, const std::string &zipfull);

std::string exec_to_string(const std::string &cmd);
