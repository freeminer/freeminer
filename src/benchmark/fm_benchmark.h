// Freeminer
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"

#include <cstddef>
#include <string>
#include <vector>

enum class ConnectionBenchmarkMode
{
	ClientServer,
	ClientOnly,
	ServerOnly,
};

struct ConnectionBenchmarkOptions
{
	ConnectionBenchmarkMode mode = ConnectionBenchmarkMode::ClientServer;
	std::string server = "::1";
	u16 port = 0;
	u16 window = 1;
	std::vector<std::string> backends;
	std::vector<size_t> client_counts;
	std::vector<size_t> payload_sizes;
};

const ConnectionBenchmarkOptions &get_connection_benchmark_options();
int parse_fm_benchmark_args(int argc, char *argv[], std::vector<char *> *catch_argv);
