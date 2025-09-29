
#include "http.h"
#include <filesystem>
#include <thread>
#include <fstream>
#include "debug/dump.h"
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

	std::ofstream(path, std::ios_base::binary) << res.data;
	if (!std::filesystem::exists(path)) {
		return uintmax_t{0};
	}
	return std::filesystem::file_size(path);
};

size_t multi_http_to_file(const std::string &name, const std::vector<std::string> &links,
		const std::string &path)
{
	static concurrent_set<std::string> http_failed;
	if (http_failed.contains(name)) {
		return std::filesystem::file_size(path);
	}

	if (std::filesystem::exists(path)) {
		return std::filesystem::file_size(path);
	}

	for (const auto &uri : links) {
		if (http_to_file(uri, path)) {
			return std::filesystem::file_size(path);
		}
	}

	http_failed.insert(name);

	warningstream
			<< "Not found " << name << "\n"
			<< "try to download manually: \n"
			<< "curl -o " << path << " "
			<< links[0]
			//<< "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem1/" << zipfile
			//<< " || " << "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem3/" << zipfile
			<< "\n";

	std::ofstream(path, std::ios_base::binary) << ""; // create zero file
	return std::filesystem::file_size(path);
};

size_t multi_http_to_file(const std::vector<std::string> &links, const std::string &path)
{
	static concurrent_set<std::string> http_failed;
	if (http_failed.contains(path)) {
		return std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
	}

	if (std::filesystem::exists(path)) {
		return std::filesystem::file_size(path);
	}

	for (const auto &uri : links) {
		if (const auto size = http_to_file(uri, path)) {
			return size;
		}
	}

	http_failed.emplace(path);

	warningstream
			<< "Not found " << path << "\n"
			<< "try to download manually: \n"
			<< "curl -o " << path << " "
			<< links[0]
			//<< "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem1/" << zipfile
			//<< " || " << "curl -o " << zipfull << " https://viewfinderpanoramas.org/dem3/" << zipfile
			<< "\n";

	std::ofstream(path, std::ios_base::binary) << ""; // create zero file
	return std::filesystem::file_size(path);
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
