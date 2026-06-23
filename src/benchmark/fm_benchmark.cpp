// Freeminer
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "benchmark/fm_benchmark.h"

#include "catch.h"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

static ConnectionBenchmarkOptions g_connection_benchmark_options;

static bool parseU16(std::string_view value, u16 *result)
{
	unsigned int parsed = 0;
	const auto *begin = value.data();
	const auto *end = value.data() + value.size();
	const auto [ptr, ec] = std::from_chars(begin, end, parsed);
	if (ec != std::errc() || ptr != end || parsed > 65535)
		return false;

	*result = static_cast<u16>(parsed);
	return true;
}

static bool parseConnectionBenchmarkMode(std::string_view value)
{
	if (value == "client-server" || value == "client_server" || value == "both") {
		g_connection_benchmark_options.mode = ConnectionBenchmarkMode::ClientServer;
		return true;
	}
	if (value == "client" || value == "client-only" || value == "client_only") {
		g_connection_benchmark_options.mode = ConnectionBenchmarkMode::ClientOnly;
		return true;
	}
	if (value == "server" || value == "server-only" || value == "server_only") {
		g_connection_benchmark_options.mode = ConnectionBenchmarkMode::ServerOnly;
		return true;
	}
	return false;
}

static bool parseConnectionBenchmarkEndpoint(std::string_view value)
{
	if (value.empty())
		return false;

	std::string_view host = value;
	std::string_view port;
	if (value.front() == '[') {
		const size_t close = value.find(']');
		if (close == std::string_view::npos)
			return false;
		host = value.substr(1, close - 1);
		if (close + 1 < value.size()) {
			if (value[close + 1] != ':')
				return false;
			port = value.substr(close + 2);
		}
	} else {
		const size_t first_colon = value.find(':');
		const size_t last_colon = value.rfind(':');
		if (first_colon != std::string_view::npos && first_colon == last_colon) {
			host = value.substr(0, first_colon);
			port = value.substr(first_colon + 1);
		}
	}

	if (host.empty())
		return false;

	g_connection_benchmark_options.server.assign(host);
	if (!port.empty())
		return parseU16(port, &g_connection_benchmark_options.port);
	return true;
}

static bool getOptionValue(int argc, char *argv[], int *index,
		std::string_view option, std::string_view *value)
{
	const std::string_view arg(argv[*index]);
	const std::string prefix = std::string(option) + "=";
	if (arg.compare(0, prefix.size(), prefix) == 0) {
		*value = arg.substr(prefix.size());
		return true;
	}

	if (arg != option)
		return false;

	if (*index + 1 >= argc)
		throw std::runtime_error(std::string(option) + " requires a value");

	*value = std::string_view(argv[++*index]);
	return true;
}

} // namespace

const ConnectionBenchmarkOptions &get_connection_benchmark_options()
{
	return g_connection_benchmark_options;
}

int parse_fm_benchmark_args(int argc, char *argv[], std::vector<char *> *catch_argv)
{
	catch_argv->push_back(argv[0]);

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg(argv[i]);
		std::string_view value;

		try {
			if (arg == "--connection-benchmark-client-only") {
				g_connection_benchmark_options.mode =
						ConnectionBenchmarkMode::ClientOnly;
				continue;
			}
			if (arg == "--connection-benchmark-server-only") {
				g_connection_benchmark_options.mode =
						ConnectionBenchmarkMode::ServerOnly;
				continue;
			}
			if (getOptionValue(argc, argv, &i, "--connection-benchmark-mode", &value)) {
				if (!parseConnectionBenchmarkMode(value)) {
					Catch::cerr() << "Invalid --connection-benchmark-mode: "
								  << value << '\n';
					return 1;
				}
				continue;
			}
			if (getOptionValue(argc, argv, &i, "--connection-benchmark-server", &value)) {
				if (!parseConnectionBenchmarkEndpoint(value)) {
					Catch::cerr() << "Invalid --connection-benchmark-server: "
								  << value << '\n';
					return 1;
				}
				continue;
			}
			if (getOptionValue(argc, argv, &i, "--connection-benchmark-port", &value)) {
				if (!parseU16(value, &g_connection_benchmark_options.port)) {
					Catch::cerr() << "Invalid --connection-benchmark-port: "
								  << value << '\n';
					return 1;
				}
				continue;
			}
		} catch (const std::exception &e) {
			Catch::cerr() << e.what() << '\n';
			return 1;
		}

		catch_argv->push_back(argv[i]);
	}

	return 0;
}
