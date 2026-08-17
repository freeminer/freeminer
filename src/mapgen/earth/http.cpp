
#include "http.h"
#include <filesystem>
#include <limits>
#include <thread>
#include <fstream>
#include "debug/dump.h"
#include "filesys.h"
#include "httpfetch.h"
#include "log.h"
#include "settings.h"
#include "threading/concurrent_set.h"

size_t http_to_file(const std::string &url, const std::string &path)
{
	HTTPFetchRequest req;
	req.url = url;
	req.connect_timeout = req.timeout = g_settings->getS32("curl_file_download_timeout");
	actionstream << "Downloading map from " << req.url << "\n";

	HTTPFetchResult res;

	if (1) {
		// TODO: why sync does not work?
		req.caller = HTTPFETCH_SYNC;
		httpfetch_sync(req, res);
	} else {
		req.caller = httpfetch_caller_alloc();
		httpfetch_async(req);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		HTTPFetchResult res;
		while (!httpfetch_async_get(req.caller, res)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		httpfetch_caller_free(req.caller);
	}

	actionstream << req.url << " " << res.succeeded << " " << res.response_code << " "
				 << res.data.size() << "\n";
	if (!res.succeeded || res.response_code >= 300) {
		return uintmax_t{0};
	}

	if (!res.data.size()) {
		return uintmax_t{0};
	}

	std::error_code ec;
	const std::filesystem::path file_path(path);
	if (!file_path.parent_path().empty())
		std::filesystem::create_directories(file_path.parent_path(), ec);

	std::ofstream output(path, std::ios_base::binary);
	output.write(res.data.data(), res.data.size());
	output.close();
	if (!output || !std::filesystem::exists(path)) {
		std::filesystem::remove(path, ec);
		return uintmax_t{0};
	}
	return std::filesystem::file_size(path);
};

size_t multi_http_to_file(
		const std::string &name, const std::vector<std::string> &links, std::string path)
{
	if (path.empty()) {
		path = porting::path_cache + DIR_DELIM + "earth" + "/" + name;
	}

	if (std::filesystem::exists(path)) {
		if (const auto size = std::filesystem::file_size(path))
			return size;
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}

	static concurrent_set<std::string> http_failed;
	if (http_failed.contains(name))
		return 0;

	for (const auto &uri : links) {
		if (const auto size = http_to_file(uri, path)) {
			return size;
		}
	}

	http_failed.insert(name);

	infostream
			<< "Not found " << name << "\n"
			<< "try to download manually: \n"
			<< "curl -o " << path << " "
			<< links[0]
			//<< "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem1/" << zipfile
			//<< " || " << "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem3/" << zipfile
			<< "\n";

	return 0;
};

size_t multi_http_to_file_cdn(const std::string &dir, const std::string &name,
		std::vector<std::string> links, const std::string &path)
{
	links.insert(links.begin(),
#if defined(__EMSCRIPTEN__)
			"/"
#else
			"https://cdn.freeminer.org/"
#endif
					+ dir + "/" + name);
	return multi_http_to_file(name, links, path);
}

std::string http_get_range(const std::string &url, std::uint64_t offset,
		std::uint64_t length)
{
	if (length == 0 || offset > std::numeric_limits<std::uint64_t>::max() - (length - 1))
		return {};
	HTTPFetchRequest req;
	req.url = url;
	req.caller = HTTPFETCH_SYNC;
	req.connect_timeout = req.timeout = g_settings->getS32("curl_file_download_timeout");
	req.extra_headers.emplace_back("Range: bytes=" + std::to_string(offset) + "-" +
			std::to_string(offset + length - 1));
	req.quiet = true;
	HTTPFetchResult res;
	httpfetch_sync(req, res);
	// A 200 response means Range was ignored. Do not return it: callers use
	// this primitive specifically to avoid materialising whole COGs in memory.
	if (!res.succeeded || res.response_code != 206 || res.data.empty())
		return {};
	return res.data;
}

size_t multi_http_to_file(const std::vector<std::string> &links, const std::string &path)
{
	if (std::filesystem::exists(path)) {
		if (const auto size = std::filesystem::file_size(path))
			return size;
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}

	static concurrent_set<std::string> http_failed;
	if (http_failed.contains(path))
		return 0;

	for (const auto &uri : links) {
		if (const auto size = http_to_file(uri, path)) {
			return size;
		}
	}

	http_failed.emplace(path);

	infostream
			<< "Not found " << path << "\n"
			<< "try to download manually: \n"
			<< "curl -o " << path << " "
			<< links[0]
			//<< "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem1/" << zipfile
			//<< " || " << "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem3/" << zipfile
			<< "\n";

	return 0;
};

std::string exec_to_string(const std::string &cmd)
{
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
	if (!pipe) {
		DUMP("Cmd failed: ", cmd);
		return {};
	}

	std::array<uint8_t, 1000000> buffer;
	std::stringstream result;
	size_t sz = 0;
	while ((sz = fread((char *)buffer.data(), 1, buffer.size(), pipe.get())) > 0) {
		result << std::string{(char *)buffer.data(), sz};
	}
	return result.str();
}
