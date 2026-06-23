// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Luanti Authors

#include "benchmark/benchmark.h"
#include "benchmark/fm_benchmark.h"

#include "catch.h"

int run_catch2_benchmarks(int argc, char *argv[])
{
	std::vector<char *> catch_argv;
	const int parse_status = parse_fm_benchmark_args(argc, argv, &catch_argv);
	if (parse_status != 0)
		return parse_status;

	Catch::Session session;

	int status = session.applyCommandLine(
			static_cast<int>(catch_argv.size()), catch_argv.data());
	if (status != 0)
		return status;

	auto config = session.configData();
	config.skipBenchmarks = false;
	session.useConfigData(config);

	return session.run();
}
