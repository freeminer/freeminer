#include "catch.h"
#include "database/database-dummy.h"
#include "database/database-leveldb.h"
#include "database/database-sqlite3.h"
#include "database/database-postgresql.h"
#include "database/database-redis.h"
#include "irr_v3d.h"
#include "porting.h"
#include "filesys.h"
#include "settings.h"
#include <array>
#include <cctype>
#include <exception>
#include <iomanip>
#include <memory>
#include <optional>
#include <random>
#include <algorithm>
#include <string_view>
#include <vector>

namespace
{

static bool g_benchmark_initialized = false;
static std::string g_test_directory;
static size_t g_database_instance_counter = 0;

static constexpr std::array<size_t, 5> g_benchmark_block_sizes = {
		256,
		1024,
		4 * 1024,
		16 * 1024,
		64 * 1024,
};
static constexpr size_t g_load_benchmark_blocks = 4096;

static std::string getBenchmarkDirectory()
{
	return porting::path_share + DIR_DELIM + ".benchmark_tmp";
}

static void initialize_benchmark_environment()
{
	if (g_benchmark_initialized)
		return;

	// Create temporary directory for benchmark databases
	g_test_directory = getBenchmarkDirectory();
	fs::CreateAllDirs(g_test_directory);

	g_benchmark_initialized = true;
}

static void cleanup_benchmark_environment()
{
	if (g_test_directory.empty())
		g_test_directory = getBenchmarkDirectory();

	// Clean up temporary directory
	if (fs::PathExists(g_test_directory))
		fs::RecursiveDelete(g_test_directory);

	g_database_instance_counter = 0;
	g_benchmark_initialized = false;
	g_test_directory.clear();
}

static std::string makeBenchmarkDatabasePath(std::string_view db_name)
{
	initialize_benchmark_environment();
	return g_test_directory + DIR_DELIM + std::string(db_name) + "_" +
		   std::to_string(g_database_instance_counter++);
}

static std::unique_ptr<MapDatabase> create_dummy_database()
{
	return std::make_unique<Database_Dummy>();
}

static std::unique_ptr<MapDatabase> create_leveldb_database()
{
	// Check if LevelDB is enabled
#if !USE_LEVELDB
	return nullptr;
#else
	std::string db_path = makeBenchmarkDatabasePath("leveldb_test");
	fs::CreateAllDirs(db_path);
	return std::make_unique<Database_LevelDB>(db_path);
#endif
}

static std::unique_ptr<MapDatabase> create_sqlite3_database()
{
#if !USE_SQLITE3
	return nullptr;
#else
	std::string db_path = makeBenchmarkDatabasePath("sqlite3_test");
	fs::CreateAllDirs(db_path);
	return std::make_unique<MapDatabaseSQLite3>(db_path);
#endif
}

static std::unique_ptr<MapDatabase> create_postgresql_database()
{
#if !USE_POSTGRESQL
	return nullptr;
#else
	try {
		/*
		sudo -u postgres createuser -P test
		sudo -u postgres createdb --owner=test test
		echo "localhost:5432:test:test:pass" >> ~/.pgpass
		chmod 0600 ~/.pgpass
		*/
		std::unique_ptr<MapDatabase> db = std::make_unique<MapDatabasePostgreSQL>(
				"host=localhost dbname=test user=test");
		if (!db->initialized()) {
			return nullptr;
		}
		return db;
	} catch (const std::exception &ex) {
		std::cerr << ex.what() << "\n";
		return nullptr;
	}
#endif
}

static std::unique_ptr<MapDatabase> create_redis_database()
{
#if !USE_REDIS
	return nullptr;
#else
	Settings conf;
	conf.set("redis_address", "localhost");
	conf.set("redis_hash", "test");
	try {
		std::unique_ptr<MapDatabase> db = std::make_unique<Database_Redis>(conf);
		return db;
	} catch (const std::exception &ex) {
		std::cerr << ex.what() << "\n";
		return nullptr;
	}
#endif
}

// Generate test data
static std::string generateTestData(size_t size = 1024)
{
	std::string data;
	data.reserve(size);

	// Generate pseudo-random printable ASCII data for consistent testing
	std::mt19937 gen(42);						  // Fixed seed for reproducible results
	std::uniform_int_distribution<> dis(32, 126); // Printable ASCII range

	for (size_t i = 0; i < size; ++i) {
		data += static_cast<char>(dis(gen));
	}

	return data;
}

static v3pos_t generateTestPosition(int index)
{
	// Generate deterministic positions for reproducible benchmarks
	// Use a simple hash-like function to distribute positions
	int x = (index * 1103515245 + 12345) & 0x7FFFFFFF;
	int y = (index * 1103515247 + 54321) & 0x7FFFFFFF;
	int z = (index * 1103515249 + 98765) & 0x7FFFFFFF;

	// Scale down and center around origin
	x = (x % 1000) - 500;
	y = (y % 1000) - 500;
	z = (z % 1000) - 500;

	return v3pos_t(x, y, z);
}

static std::string formatBlockSize(size_t bytes)
{
	if (bytes >= 1024 && bytes % 1024 == 0)
		return std::to_string(bytes / 1024) + " KiB";

	return std::to_string(bytes) + " B";
}

static std::string makeThroughputBenchmarkName(
		std::string_view operation, const std::string &db_name, size_t block_size)
{
	return "DB" + std::string(operation) + "_" + db_name + "_" +
		   std::to_string(block_size) + "B";
}

struct ThroughputBenchmarkMetadata
{
	std::string operation;
	std::string db_name;
	size_t block_size;
};

static std::optional<ThroughputBenchmarkMetadata> parseThroughputBenchmarkName(
		const std::string &name)
{
	constexpr std::string_view write_prefix = "DBWrite_";
	constexpr std::string_view read_prefix = "DBRead_";

	std::string operation;
	size_t prefix_size = 0;
	if (name.compare(0, write_prefix.size(), write_prefix) == 0) {
		operation = "write";
		prefix_size = write_prefix.size();
	} else if (name.compare(0, read_prefix.size(), read_prefix) == 0) {
		operation = "read";
		prefix_size = read_prefix.size();
	} else {
		return std::nullopt;
	}

	const auto separator = name.rfind('_');
	if (separator == std::string::npos || separator <= prefix_size ||
			separator + 2 > name.size() || name.back() != 'B') {
		return std::nullopt;
	}

	size_t block_size = 0;
	for (size_t i = separator + 1; i + 1 < name.size(); ++i) {
		const unsigned char c = static_cast<unsigned char>(name[i]);
		if (!std::isdigit(c))
			return std::nullopt;
		block_size = block_size * 10 + (c - '0');
	}

	if (block_size == 0)
		return std::nullopt;

	return ThroughputBenchmarkMetadata{
			operation,
			name.substr(prefix_size, separator - prefix_size),
			block_size,
	};
}

struct ThroughputBenchmarkResult
{
	std::string db_name;
	std::string operation;
	size_t block_size;
	double mean_ns;
	double operations_per_second;
	double mib_per_second;
};

class DatabaseBenchmarkThroughputListener : public Catch::EventListenerBase
{
public:
	using Catch::EventListenerBase::EventListenerBase;

	static std::string getDescription()
	{
		return "prints derived database benchmark throughput";
	}

	void benchmarkEnded(Catch::BenchmarkStats<> const &stats) override
	{
		const auto metadata = parseThroughputBenchmarkName(stats.info.name);
		if (!metadata)
			return;

		const double mean_ns = stats.mean.point.count();
		if (mean_ns <= 0.0)
			return;

		const double operations_per_second = 1000000000.0 / mean_ns;
		const double mib_per_second = operations_per_second *
									  static_cast<double>(metadata->block_size) /
									  (1024.0 * 1024.0);

		m_results.push_back({
				metadata->db_name,
				metadata->operation,
				metadata->block_size,
				mean_ns,
				operations_per_second,
				mib_per_second,
		});
	}

	void testRunEnded(Catch::TestRunStats const &) override
	{
		if (m_results.empty())
			return;

		auto &out = Catch::cerr();
		const auto old_flags = out.flags();
		const auto old_precision = out.precision();

		out << "\nDatabase read/write throughput "
			<< "(derived from Catch2 benchmark mean)\n";
		out << std::left << std::setw(14) << "database" << std::setw(8) << "op"
			<< std::right << std::setw(10) << "size" << std::setw(15) << "ops/sec"
			<< std::setw(15) << "MiB/sec" << std::setw(15) << "mean ns/op" << '\n';
		out << std::string(77, '-') << '\n';

		for (const auto &result : m_results) {
			out << std::left << std::setw(14) << result.db_name << std::setw(8)
				<< result.operation << std::right << std::setw(10)
				<< formatBlockSize(result.block_size) << std::setw(15) << std::fixed
				<< std::setprecision(0) << result.operations_per_second << std::setw(15)
				<< std::fixed << std::setprecision(2) << result.mib_per_second
				<< std::setw(15) << std::fixed << std::setprecision(1) << result.mean_ns
				<< '\n';
		}

		out.flags(old_flags);
		out.precision(old_precision);
	}

private:
	std::vector<ThroughputBenchmarkResult> m_results;
};

CATCH_REGISTER_LISTENER(DatabaseBenchmarkThroughputListener)

// Benchmark functions for each operation
template <typename DatabaseFactory>
static void benchmarkSaveBlock(
		DatabaseFactory factory, const std::string &db_name, size_t block_size)
{
	initialize_benchmark_environment();
	auto db = factory();
	if (!db)
		return;

	std::string test_data = generateTestData(block_size);
	size_t next_index = 0;
	db->beginSave();

	BENCHMARK_ADVANCED(makeThroughputBenchmarkName("Write", db_name, block_size))(
			Catch::Benchmark::Chronometer meter)
	{
		meter.measure([&](int) {
			const auto pos = generateTestPosition(next_index++);
			return db->saveBlock(pos, test_data);
		});
	};

	db->endSave();
}

template <typename DatabaseFactory>
static void benchmarkLoadBlock(
		DatabaseFactory factory, const std::string &db_name, size_t block_size)
{
	initialize_benchmark_environment();
	auto db = factory();
	if (!db)
		return;

	// Pre-populate database
	std::string test_data = generateTestData(block_size);
	db->beginSave();
	for (size_t i = 0; i < g_load_benchmark_blocks; ++i) {
		const auto pos = generateTestPosition(i);
		db->saveBlock(pos, test_data);
	}
	db->endSave();

	std::string loaded_data;
	BENCHMARK_ADVANCED(makeThroughputBenchmarkName("Read", db_name, block_size))(
			Catch::Benchmark::Chronometer meter)
	{
		meter.measure([&](int i) {
			const auto pos = generateTestPosition(i % g_load_benchmark_blocks);
			db->loadBlock(pos, &loaded_data);
			return loaded_data.size();
		});
	};
}

template <typename DatabaseFactory>
static void benchmarkDeleteBlock(DatabaseFactory factory, const std::string &db_name)
{
	BENCHMARK_ADVANCED("DeleteBlock_" + db_name)(Catch::Benchmark::Chronometer meter)
	{
		initialize_benchmark_environment();
		auto db = factory();
		if (!db) {
			meter.measure([](int) { return 0; }); // Skip if database not available
			return;
		}

		// Pre-populate database
		std::string test_data = generateTestData();
		db->beginSave();
		for (int i = 0; i < meter.runs(); ++i) {
			auto pos = generateTestPosition(i);
			db->saveBlock(pos, test_data);
		}
		db->endSave();

		meter.measure([&](int i) {
			auto pos = generateTestPosition(i);
			return db->deleteBlock(pos);
		});
	};
}

template <typename DatabaseFactory>
static void benchmarkListAllBlocks(
		DatabaseFactory factory, const std::string &db_name, size_t iterations)
{
	initialize_benchmark_environment();
	auto db = factory();
	if (!db)
		return;

	// Pre-populate database with reasonable amount of data
	size_t populate_count = std::min(iterations, static_cast<size_t>(1000));
	std::string test_data = generateTestData();
	db->beginSave();
	for (size_t i = 0; i < populate_count; ++i) {
		const auto pos = generateTestPosition(i);
		db->saveBlock(pos, test_data);
	}
	db->endSave();

	std::vector<v3pos_t> block_list;
	BENCHMARK_ADVANCED("ListAllBlocks_" + db_name)(Catch::Benchmark::Chronometer meter)
	{
		meter.measure([&] {
			block_list.clear();
			db->listAllLoadableBlocks(block_list);
			return block_list.size();
		});
	};
}

} // namespace

TEST_CASE("benchmark_database_operations")
{
	const auto iterations2 = 1000;

	const auto have_postgresql = !!create_postgresql_database();
	const auto have_redis = !!create_redis_database();
	SECTION("SaveBlock Operations")
	{
		for (const size_t block_size : g_benchmark_block_sizes) {
			benchmarkSaveBlock(create_dummy_database, "Dummy", block_size);
			benchmarkSaveBlock(create_leveldb_database, "LevelDB", block_size);
			benchmarkSaveBlock(create_sqlite3_database, "SQLite3", block_size);
			if (have_postgresql)
				benchmarkSaveBlock(create_postgresql_database, "Postgresql", block_size);
			if (have_redis)
				benchmarkSaveBlock(create_redis_database, "Redis", block_size);
		}
	}

	SECTION("LoadBlock Operations")
	{
		for (const size_t block_size : g_benchmark_block_sizes) {
			benchmarkLoadBlock(create_dummy_database, "Dummy", block_size);
			benchmarkLoadBlock(create_leveldb_database, "LevelDB", block_size);
			benchmarkLoadBlock(create_sqlite3_database, "SQLite3", block_size);
			if (have_postgresql)
				benchmarkLoadBlock(create_postgresql_database, "Postgresql", block_size);
			if (have_redis)
				benchmarkLoadBlock(create_redis_database, "Redis", block_size);
		}
	}

	SECTION("DeleteBlock Operations")
	{
		benchmarkDeleteBlock(create_dummy_database, "Dummy");
		benchmarkDeleteBlock(create_leveldb_database, "LevelDB");
		benchmarkDeleteBlock(create_sqlite3_database, "SQLite3");
		if (have_postgresql)
			benchmarkDeleteBlock(create_postgresql_database, "Postgresql");
		if (have_redis)
			benchmarkDeleteBlock(create_redis_database, "Redis");
	}

	SECTION("ListAllBlocks Operations")
	{
		benchmarkListAllBlocks(create_dummy_database, "Dummy", iterations2);
		benchmarkListAllBlocks(create_leveldb_database, "LevelDB", iterations2);
		benchmarkListAllBlocks(create_sqlite3_database, "SQLite3", iterations2);
		if (have_postgresql)
			benchmarkListAllBlocks(create_postgresql_database, "Postgresql", iterations2);
		if (have_redis)
			benchmarkListAllBlocks(create_redis_database, "Redis", iterations2);
	}
}

// Cleanup at the end
TEST_CASE("benchmark_database_cleanup")
{
	cleanup_benchmark_environment();
}
