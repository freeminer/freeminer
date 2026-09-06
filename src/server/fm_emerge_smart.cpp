// Freeminer
// Reliable, bounded mapgen chunk generation for large regions.

/*
freeminer.conf examples for one /emerge_smart job:

# 10 mapgen threads
server_async_threads = 1
num_emerge_threads = 10
emerge_smart_in_flight = 10
emergequeue_limit_total = 1000
emergequeue_limit_diskonly = 1000
emergequeue_limit_generate = 1000

# 100 mapgen threads (requires a thread-safe mapgen and approximately 100 GB RAM)
server_async_threads = 1
num_emerge_threads = 100
emerge_smart_in_flight = 100
emergequeue_limit_total = 1000
emergequeue_limit_diskonly = 1000
emergequeue_limit_generate = 1000

server_async_threads controls concurrent manager jobs, not mapgen workers.
*/

#include "fm_emerge_smart.h"

#include "constants.h"
#include "emerge.h"
#include "log.h"
#include "mapblock.h"
#include "remoteplayer.h"
#include "script/lua_api/l_base.h"
#include "server.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/string.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr u16 GENERATE_DEFAULT_IN_FLIGHT = 4;
constexpr u16 GENERATE_MAX_IN_FLIGHT = 100;
constexpr unsigned GENERATE_MAX_ERROR_RETRIES = 3;
constexpr u32 GENERATE_DEFAULT_MAX_CANCEL_RETRIES = 60;
constexpr u32 GENERATE_MAX_CANCEL_RETRIES = 10000;
constexpr s32 GENERATE_DEFAULT_TASK_TIMEOUT_SECONDS = 600;
constexpr s32 GENERATE_MIN_TASK_TIMEOUT_SECONDS = 1;
constexpr s32 GENERATE_MAX_TASK_TIMEOUT_SECONDS = 86400;
constexpr auto GENERATE_HEARTBEAT_INTERVAL = std::chrono::seconds(60);
constexpr auto GENERATE_RETRY_MAX_DELAY = std::chrono::seconds(5);
constexpr auto GENERATE_MANAGER_POLL_INTERVAL = std::chrono::milliseconds(100);
constexpr size_t EMERGE_ACTION_COUNT = static_cast<size_t>(EMERGE_GENERATED) + 1;

struct GenerateTask
{
	v3bpos_t blockpos;
	unordered_set_v3bpos changed_blocks;
	u64 cancel_retries = 0;
	u64 queue_retries = 0;
	unsigned error_retries = 0;
	u64 ring = 0;
	bool queued = false;
	bool done = false;
	TimePoint requested_at{};
	TimePoint retry_at{};
	TimePoint last_slow_log{};
	TimePoint deadline{};
};

struct GenerateJob
{
	std::string player_name;
	v3bpos_t center_block;
	v3bpos_t center_chunk;
	v3bpos_t chunk_size;
	u64 max_ring = 0;
	u64 total = 0;
	u64 ring = 0;
	u64 next_ring_index = 0;
	u64 completed = 0;
	u64 failed = 0;
	u64 emerged_blocks = 0;
	u64 last_progress_percent = 0;
	u32 changed_since = 0;
	u16 in_flight_limit = GENERATE_DEFAULT_IN_FLIGHT;
	u32 max_cancel_retries = GENERATE_DEFAULT_MAX_CANCEL_RETRIES;
	std::chrono::seconds task_timeout{GENERATE_DEFAULT_TASK_TIMEOUT_SECONDS};
	std::array<u64, EMERGE_ACTION_COUNT> actions{};
	std::atomic_bool cancelled{false};
	TimePoint started_at = Clock::now();
	TimePoint last_heartbeat = started_at;
	std::vector<std::shared_ptr<GenerateTask>> active;
};

struct GenerateCompletion
{
	std::shared_ptr<GenerateJob> job;
	std::shared_ptr<GenerateTask> task;
	v3bpos_t blockpos;
	EmergeAction action = EMERGE_ERRORED;
};

std::string blockpos_string(const v3bpos_t &p)
{
	std::ostringstream os;
	os << "(" << p.X << "," << p.Y << "," << p.Z << ")";
	return os.str();
}

v3bpos_t containing_chunk(const v3bpos_t &blockpos, const v3bpos_t &chunk_size)
{
	const auto chunk = EmergeManager::getContainingChunk(blockpos, chunk_size);
	return v3bpos_t(chunk.X, chunk.Y, chunk.Z);
}

u64 ring_size(u64 ring)
{
	return ring == 0 ? 1 : ring * 8;
}

std::pair<int64_t, int64_t> onion_position(u64 ring, u64 index)
{
	if (ring == 0)
		return {0, 0};

	const int64_t r = static_cast<int64_t>(ring);
	const u64 side = ring * 2;
	if (index < side)
		return {-r + static_cast<int64_t>(index), -r};
	index -= side;
	if (index < side)
		return {r, -r + static_cast<int64_t>(index)};
	index -= side;
	if (index < side)
		return {r - static_cast<int64_t>(index), r};
	index -= side;
	return {-r, r - static_cast<int64_t>(index)};
}

std::chrono::milliseconds retry_delay(u64 retry)
{
	const u64 shift = std::min<u64>(retry > 0 ? retry - 1 : 0, 6);
	return std::min(std::chrono::milliseconds(100 * (u64{1} << shift)),
			std::chrono::duration_cast<std::chrono::milliseconds>(
					GENERATE_RETRY_MAX_DELAY));
}

bool parse_params(const std::string &params, int64_t &radius, int &in_flight)
{
	std::istringstream input(params);
	if (!(input >> radius) || radius < 0)
		return false;
	input >> std::ws;
	if (input.eof())
		return true;
	if (!(input >> in_flight) || in_flight < 1)
		return false;
	input >> std::ws;
	return input.eof();
}

} // namespace

class FmEmergeGenerateState;

struct GenerateCallbackContext
{
	std::weak_ptr<FmEmergeGenerateState> state;
	std::weak_ptr<GenerateJob> job;
	std::shared_ptr<GenerateTask> task;
};

class FmEmergeGenerateState : public std::enable_shared_from_this<FmEmergeGenerateState>
{
public:
	explicit FmEmergeGenerateState(Server *server) : m_server(server) {}

	~FmEmergeGenerateState() { shutdown(); }

	bool prepare(const std::string &player_name, const std::string &params,
			std::string &message)
	{
		int64_t radius_value = -1;
		int in_flight_value = 0;
		if (!parse_params(params, radius_value, in_flight_value)) {
			message = "Usage: /emerge_smart radius [in_flight]";
			return false;
		}

		RemotePlayer *player = m_server->getEnv().getPlayer(player_name);
		PlayerSAO *sao = player ? player->getPlayerSAO() : nullptr;
		if (!sao) {
			message = "Player not found.";
			return false;
		}

		if (radius_value > std::numeric_limits<pos_t>::max()) {
			message = "Radius is too large for this build.";
			return false;
		}

		if (in_flight_value == 0) {
			s16 configured = GENERATE_DEFAULT_IN_FLIGHT;
			g_settings->getS16NoEx("emerge_smart_in_flight", configured);
			in_flight_value = configured;
		}
		in_flight_value =
				std::clamp(in_flight_value, 1, static_cast<int>(GENERATE_MAX_IN_FLIGHT));

		const auto configured_chunk_size =
				m_server->getEnv().getServerMap().getMapgenParams()->chunksize;
		const v3bpos_t chunk_size(configured_chunk_size.X, configured_chunk_size.Y,
				configured_chunk_size.Z);
		if (chunk_size.X <= 0 || chunk_size.Y <= 0 || chunk_size.Z <= 0) {
			message = "Mapgen chunksize is invalid.";
			return false;
		}

		const pos_t radius = static_cast<pos_t>(radius_value);
		const u64 horizontal_chunk_nodes =
				static_cast<u64>(std::min(chunk_size.X, chunk_size.Z)) * MAP_BLOCKSIZE;
		const u64 max_ring = (static_cast<u64>(radius) + horizontal_chunk_nodes - 1) /
							 horizontal_chunk_nodes;
		if (max_ring > (std::numeric_limits<u64>::max() - 1) / 2) {
			message = "Radius produces too many mapgen chunk rings.";
			return false;
		}
		const u64 side = max_ring * 2 + 1;
		if (side > std::numeric_limits<u64>::max() / side) {
			message = "Radius produces too many mapgen chunk columns.";
			return false;
		}

		auto job = std::make_shared<GenerateJob>();
		s32 task_timeout = GENERATE_DEFAULT_TASK_TIMEOUT_SECONDS;
		g_settings->getS32NoEx("emerge_smart_task_timeout", task_timeout);
		task_timeout = std::clamp(task_timeout, GENERATE_MIN_TASK_TIMEOUT_SECONDS,
				GENERATE_MAX_TASK_TIMEOUT_SECONDS);
		u32 max_cancel_retries = GENERATE_DEFAULT_MAX_CANCEL_RETRIES;
		g_settings->getU32NoEx("emerge_smart_max_cancel_retries", max_cancel_retries);
		max_cancel_retries = std::min(max_cancel_retries, GENERATE_MAX_CANCEL_RETRIES);

		job->player_name = player_name;
		job->center_block = getNodeBlockPos(floatToInt(sao->getBasePosition(), BS));
		job->center_chunk = containing_chunk(job->center_block, chunk_size);
		job->chunk_size = chunk_size;
		job->changed_since = ServerMap::time_life.load(std::memory_order_relaxed);
		job->max_ring = max_ring;
		job->total = side * side;
		job->in_flight_limit = static_cast<u16>(in_flight_value);
		job->max_cancel_retries = max_cancel_retries;
		job->task_timeout = std::chrono::seconds(task_timeout);

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_stopping) {
				message = "Server is shutting down.";
				return false;
			}
			m_job = job;
		}

		std::ostringstream response;
		response << "Started generation of " << job->total << " mapgen chunk columns ("
				 << chunk_size.X * MAP_BLOCKSIZE << "x" << chunk_size.Y * MAP_BLOCKSIZE
				 << "x" << chunk_size.Z * MAP_BLOCKSIZE
				 << " nodes per generated chunk), center first, " << job->in_flight_limit
				 << " columns in flight.";
		message = response.str();
		m_start_log = "[earth] " + message + " Player=" + player_name +
					  ", center_block=" + blockpos_string(job->center_block) +
					  ", center_chunk=" + blockpos_string(job->center_chunk) +
					  ", radius=" + std::to_string(radius) +
					  ", rings=" + std::to_string(max_ring);
		return true;
	}

	void run_task()
	{
		actionstream << m_start_log << std::endl;
		try {
			run();
		} catch (const std::exception &e) {
			errorstream << "[earth] Generation task stopped: " << e.what() << std::endl;
			stop_after_failure();
		} catch (...) {
			errorstream << "[earth] Generation task stopped with an unknown error"
						<< std::endl;
			stop_after_failure();
		}
	}

	void shutdown()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stopping = true;
		cancel_job_locked();
		m_wake.notify_all();
	}

private:
	static void emerge_callback(v3bpos_t blockpos, EmergeAction action, void *param)
	{
		std::unique_ptr<GenerateCallbackContext> context(
				static_cast<GenerateCallbackContext *>(param));
		auto state = context->state.lock();
		auto job = context->job.lock();
		if (!state || !job || job->cancelled.load(std::memory_order_acquire))
			return;
		state->post_completion({std::move(job), context->task, blockpos, action});
	}

	void post_completion(GenerateCompletion completion)
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_stopping || completion.job->cancelled.load(std::memory_order_relaxed))
				return;
			m_completions.push_back(std::move(completion));
		}
		m_wake.notify_one();
	}

	void cancel_job_locked()
	{
		if (m_job)
			m_job->cancelled.store(true, std::memory_order_release);
		m_job.reset();
		m_completions.clear();
	}

	void run()
	{
		for (;;) {
			std::deque<GenerateCompletion> completions;
			std::shared_ptr<GenerateJob> job;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_stopping || m_server->asyncTasksStopping()) {
					m_stopping = true;
					cancel_job_locked();
					return;
				}
				completions.swap(m_completions);
				job = m_job;
			}

			const TimePoint now = Clock::now();
			for (const auto &completion : completions)
				process_completion(completion, now);

			if (job && !job->cancelled.load(std::memory_order_relaxed)) {
				const auto active = job->active;
				for (const auto &task : active) {
					if (task->done)
						continue;
					if (task->deadline <= now) {
						fail_task(job, task,
								"mapblock attempt timed out after " +
										std::to_string(job->task_timeout.count()) + "s");
						continue;
					}
					if (!task->queued && task->retry_at <= now &&
							!queue_task(job, task, now)) {
						fail_task(job, task, "retry moved outside mapgen limit");
					}
				}
				fill_in_flight(job, now);
				if (!job->cancelled.load(std::memory_order_relaxed))
					log_heartbeat(job, now);
			}

			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_stopping || m_server->asyncTasksStopping()) {
				m_stopping = true;
				cancel_job_locked();
				return;
			}
			if (!m_job && m_completions.empty()) {
				return;
			}
			m_wake.wait_for(lock, GENERATE_MANAGER_POLL_INTERVAL);
		}
	}

	void stop_after_failure()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stopping = true;
		cancel_job_locked();
	}

	void notify(const std::shared_ptr<GenerateJob> &job, const std::string &message)
	{
		m_server->notifyPlayer(job->player_name.c_str(), utf8_to_wide(message));
	}

	void schedule_retry(
			const std::shared_ptr<GenerateTask> &task, u64 retry, TimePoint now)
	{
		task->queued = false;
		task->retry_at = now + retry_delay(retry);
	}

	bool queue_task(const std::shared_ptr<GenerateJob> &job,
			const std::shared_ptr<GenerateTask> &task, TimePoint now)
	{
		if (job->cancelled.load(std::memory_order_relaxed) || task->done)
			return false;

		if (m_server->getEnv().getServerMap().blockpos_over_mapgen_limit(
					task->blockpos)) {
			warningstream << "[earth] Region mapblock is outside mapgen_limit: "
						  << blockpos_string(task->blockpos) << std::endl;
			return false;
		}

		auto *context = new GenerateCallbackContext{shared_from_this(), job, task};
		const bool queued = m_server->getEmergeManager()->enqueueBlockEmergeEx(
				task->blockpos, PEER_ID_INEXISTENT, BLOCK_EMERGE_ALLOW_GEN,
				emerge_callback, context);
		if (!queued) {
			delete context;
			task->queue_retries++;
			schedule_retry(task, task->queue_retries, now);
			if (task->queue_retries == 1 || task->queue_retries % 10 == 0) {
				warningstream << "[earth] Region mapblock queue full; retry "
							  << task->queue_retries << " scheduled for "
							  << blockpos_string(task->blockpos) << std::endl;
			}
			return true;
		}

		task->queued = true;
		task->requested_at = now;
		task->last_slow_log = now;
		return true;
	}

	void log_progress(const std::shared_ptr<GenerateJob> &job)
	{
		if (job->total == 0 || job->completed >= job->total)
			return;

		const u64 percent = static_cast<u64>(
				(static_cast<long double>(job->completed) * 100.0L) / job->total);
		if (percent <= job->last_progress_percent)
			return;

		job->last_progress_percent = percent;
		const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				Clock::now() - job->started_at)
									 .count();
		std::ostringstream os;
		os << "Generation: " << percent << "% (" << job->completed << "/" << job->total
		   << " mapgen chunk columns, ring " << job->ring << "/" << job->max_ring << ", "
		   << job->active.size() << " in flight, " << job->failed << " failed).";
		actionstream << "[earth] " << os.str() << " Elapsed: " << elapsed << "s"
					 << std::endl;
		notify(job, os.str());
	}

	void erase_task(const std::shared_ptr<GenerateJob> &job,
			const std::shared_ptr<GenerateTask> &task)
	{
		job->active.erase(std::remove(job->active.begin(), job->active.end(), task),
				job->active.end());
	}

	void complete_task(const std::shared_ptr<GenerateJob> &job,
			const std::shared_ptr<GenerateTask> &task, bool failed)
	{
		if (task->done)
			return;
		task->done = true;
		task->queued = false;
		erase_task(job, task);
		job->completed++;
		if (failed)
			job->failed++;
		log_progress(job);
	}

	void finish_job(const std::shared_ptr<GenerateJob> &job)
	{
		if (job->cancelled.exchange(true))
			return;

		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				Clock::now() - job->started_at)
									 .count();
		std::ostringstream os;
		os << "Generation done: " << job->completed << " mapgen chunk columns, "
		   << job->emerged_blocks << " mapblocks in " << elapsed << "ms";
		if (job->failed)
			os << " (" << job->failed << " failed)";
		os << ".";

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_job == job)
				m_job.reset();
		}

		actionstream << "[earth] " << os.str() << " Player=" << job->player_name
					 << std::endl;
		notify(job, os.str());
	}

	void fail_task(const std::shared_ptr<GenerateJob> &job,
			const std::shared_ptr<GenerateTask> &task, const std::string &reason)
	{
		errorstream << "[earth] Region mapgen chunk column failed at "
					<< blockpos_string(task->blockpos) << ": " << reason << std::endl;
		complete_task(job, task, true);
	}

	void queue_column_updates(const std::shared_ptr<GenerateTask> &task)
	{
		ServerMap &map = m_server->getEnv().getServerMap();
		for (const v3bpos_t &blockpos : task->changed_blocks) {
			map.lighting_modified_add(blockpos);
			map.changed_blocks_for_merge.emplace(blockpos);
		}
	}

	void remember_changed_chunk(const std::shared_ptr<GenerateJob> &job,
			const std::shared_ptr<GenerateTask> &task, const v3bpos_t &blockpos)
	{
		const v3bpos_t chunk_min = containing_chunk(blockpos, job->chunk_size);
		for (bpos_t x = 0; x < job->chunk_size.X; x++)
			for (bpos_t y = 0; y < job->chunk_size.Y; y++)
				for (bpos_t z = 0; z < job->chunk_size.Z; z++)
					task->changed_blocks.emplace(
							chunk_min.X + x, chunk_min.Y + y, chunk_min.Z + z);
	}

	void process_completion(const GenerateCompletion &completion, TimePoint now)
	{
		const auto &job = completion.job;
		const auto &task = completion.task;
		if (job->cancelled.load(std::memory_order_relaxed) || task->done)
			return;

		task->queued = false;
		job->emerged_blocks++;
		const auto action_index = static_cast<size_t>(completion.action);
		if (action_index < job->actions.size())
			job->actions[action_index]++;

		if (completion.action == EMERGE_CANCELLED) {
			task->cancel_retries++;
			if (task->cancel_retries > job->max_cancel_retries) {
				fail_task(job, task,
						"emerge remained cancelled after " +
								std::to_string(job->max_cancel_retries) + " retries");
				return;
			}
			schedule_retry(task, task->cancel_retries, now);
			if (task->cancel_retries == 1 || task->cancel_retries % 10 == 0) {
				warningstream << "[earth] Region mapblock emerge cancelled; retry "
							  << task->cancel_retries << " scheduled for "
							  << blockpos_string(task->blockpos) << std::endl;
			}
			return;
		}

		if (completion.action == EMERGE_ERRORED) {
			task->error_retries++;
			if (task->error_retries > GENERATE_MAX_ERROR_RETRIES) {
				fail_task(job, task,
						"emerge failed after " +
								std::to_string(GENERATE_MAX_ERROR_RETRIES) + " retries");
				return;
			}
			schedule_retry(task, task->error_retries, now);
			warningstream << "[earth] Region mapblock emerge error; retry "
						  << task->error_retries << "/" << GENERATE_MAX_ERROR_RETRIES
						  << " scheduled for " << blockpos_string(task->blockpos)
						  << std::endl;
			return;
		}

		bool changed = completion.action == EMERGE_GENERATED;
		if (completion.action == EMERGE_FROM_MEMORY ||
				completion.action == EMERGE_FROM_DISK) {
			MapBlockPtr block = m_server->getEnv().getServerMap().getBlock(
					completion.blockpos, false, true);
			const u32 changed_timestamp =
					block ? block->m_changed_timestamp.load(std::memory_order_relaxed)
						  : 0;
			changed = task->cancel_retries > 0 ||
					  (changed_timestamp != 0 && changed_timestamp >= job->changed_since);
		}

		// bool changed = completion.action >= EMERGE_FROM_MEMORY;

		if (changed)
			remember_changed_chunk(job, task, completion.blockpos);

		task->cancel_retries = 0;
		task->queue_retries = 0;
		task->error_retries = 0;

		const v3bpos_t chunk_min = containing_chunk(completion.blockpos, job->chunk_size);
		const int64_t top_y_wide =
				static_cast<int64_t>(chunk_min.Y) + job->chunk_size.Y - 1;
		if (top_y_wide > std::numeric_limits<bpos_t>::max()) {
			fail_task(job, task, "vertical mapgen chunk coordinate overflow");
			return;
		}
		const bpos_t top_y = static_cast<bpos_t>(top_y_wide);
		bool top_is_air = true;
		bool top_is_available = true;
		v3bpos_t missing_block;
		ServerMap &map = m_server->getEnv().getServerMap();
		for (bpos_t x = 0; x < job->chunk_size.X && top_is_available; x++)
			for (bpos_t z = 0; z < job->chunk_size.Z; z++) {
				const v3bpos_t blockpos(chunk_min.X + x, top_y, chunk_min.Z + z);
				MapBlockPtr block = map.getBlock(blockpos, false, true);
				if (!block || !block->isGenerated()) {
					top_is_available = false;
					missing_block = blockpos;
					break;
				}
				if (!block->isAir())
					top_is_air = false;
			}

		if (!top_is_available) {
			task->blockpos = missing_block;
			task->deadline = now + job->task_timeout;
			if (!queue_task(job, task, now))
				fail_task(job, task, "upper mapgen chunk layer is outside mapgen limit");
			return;
		}

		task->blockpos = v3bpos_t(chunk_min.X, top_y, chunk_min.Z);
		if (top_is_air) {
			queue_column_updates(task);
			complete_task(job, task, false);
			return;
		}

		const int64_t next_y_wide = static_cast<int64_t>(chunk_min.Y) + job->chunk_size.Y;
		if (next_y_wide > std::numeric_limits<bpos_t>::max()) {
			fail_task(job, task, "vertical mapgen chunk coordinate overflow");
			return;
		}
		task->blockpos.Y = static_cast<bpos_t>(next_y_wide);
		task->deadline = now + job->task_timeout;
		if (!queue_task(job, task, now))
			fail_task(job, task, "outside mapgen limit");
	}

	void add_ring_task(const std::shared_ptr<GenerateJob> &job, TimePoint now)
	{
		if (job->next_ring_index >= ring_size(job->ring))
			return;

		const auto offset = onion_position(job->ring, job->next_ring_index++);
		const int64_t block_x = static_cast<int64_t>(job->center_chunk.X) +
								offset.first * job->chunk_size.X;
		const int64_t block_z = static_cast<int64_t>(job->center_chunk.Z) +
								offset.second * job->chunk_size.Z;
		if (block_x < std::numeric_limits<bpos_t>::min() ||
				block_x > std::numeric_limits<bpos_t>::max() ||
				block_z < std::numeric_limits<bpos_t>::min() ||
				block_z > std::numeric_limits<bpos_t>::max()) {
			auto task = std::make_shared<GenerateTask>();
			task->ring = job->ring;
			job->active.push_back(task);
			complete_task(job, task, true);
			return;
		}

		auto task = std::make_shared<GenerateTask>();
		task->ring = job->ring;
		task->blockpos.X = static_cast<bpos_t>(block_x);
		task->blockpos.Z = static_cast<bpos_t>(block_z);
		task->deadline = now + job->task_timeout;
		job->active.push_back(task);

		ServerMap &map = m_server->getEnv().getServerMap();
		const v3bpos_t horizontal_blockpos(task->blockpos.X, 0, task->blockpos.Z);
		if (map.blockpos_over_mapgen_limit(horizontal_blockpos)) {
			fail_task(job, task, "horizontal position outside mapgen limit");
			return;
		}

		const int64_t sample_x_wide =
				static_cast<int64_t>(task->blockpos.X) * MAP_BLOCKSIZE +
				static_cast<int64_t>(job->chunk_size.X) * MAP_BLOCKSIZE / 2;
		const int64_t sample_z_wide =
				static_cast<int64_t>(task->blockpos.Z) * MAP_BLOCKSIZE +
				static_cast<int64_t>(job->chunk_size.Z) * MAP_BLOCKSIZE / 2;
		if (sample_x_wide < std::numeric_limits<pos_t>::min() ||
				sample_x_wide > std::numeric_limits<pos_t>::max() ||
				sample_z_wide < std::numeric_limits<pos_t>::min() ||
				sample_z_wide > std::numeric_limits<pos_t>::max()) {
			fail_task(job, task, "terrain sample outside node coordinate range");
			return;
		}
		const pos_t sample_x = static_cast<pos_t>(sample_x_wide);
		const pos_t sample_z = static_cast<pos_t>(sample_z_wide);
		const pos_t ground = m_server->getEmergeManager()->getGroundLevelAtPoint(
				v2pos_t(sample_x, sample_z));
		const bpos_t base_y = getContainerPos(ground, MAP_BLOCKSIZE) - 1;
		task->blockpos = containing_chunk(
				v3bpos_t(task->blockpos.X, base_y, task->blockpos.Z), job->chunk_size);

		if (!queue_task(job, task, now))
			fail_task(job, task, "terrain base is outside mapgen limit");
	}

	void fill_in_flight(const std::shared_ptr<GenerateJob> &job, TimePoint now)
	{
		while (!job->cancelled.load(std::memory_order_relaxed)) {
			const u64 size = ring_size(job->ring);
			if (job->active.empty() && job->next_ring_index >= size) {
				if (job->ring >= job->max_ring) {
					finish_job(job);
					return;
				}
				job->ring++;
				job->next_ring_index = 0;
				continue;
			}

			if (job->active.size() >= job->in_flight_limit ||
					job->next_ring_index >= size)
				return;
			add_ring_task(job, now);
		}
	}

	void log_heartbeat(const std::shared_ptr<GenerateJob> &job, TimePoint now)
	{
		if (now - job->last_heartbeat < GENERATE_HEARTBEAT_INTERVAL)
			return;
		job->last_heartbeat = now;

		const auto elapsed =
				std::chrono::duration_cast<std::chrono::seconds>(now - job->started_at)
						.count();
		actionstream << "[earth] Generation heartbeat: " << job->completed << "/"
					 << job->total << " mapgen chunk columns, ring " << job->ring << "/"
					 << job->max_ring << ", active=" << job->active.size()
					 << ", emerge_queue=" << m_server->getEmergeManager()->getQueueSize()
					 << ", elapsed=" << elapsed
					 << "s, actions cancelled/errored/memory/disk/generated="
					 << job->actions[EMERGE_CANCELLED] << "/"
					 << job->actions[EMERGE_ERRORED] << "/"
					 << job->actions[EMERGE_FROM_MEMORY] << "/"
					 << job->actions[EMERGE_FROM_DISK] << "/"
					 << job->actions[EMERGE_GENERATED] << std::endl;

		for (const auto &task : job->active) {
			if (!task->queued || now - task->last_slow_log < GENERATE_HEARTBEAT_INTERVAL)
				continue;
			task->last_slow_log = now;
			const auto waiting = std::chrono::duration_cast<std::chrono::seconds>(
					now - task->requested_at)
										 .count();
			warningstream << "[earth] Region mapgen chunk still emerging after "
						  << waiting << "s: " << blockpos_string(task->blockpos)
						  << ", ring=" << task->ring << std::endl;
		}
	}

	Server *m_server;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	std::shared_ptr<GenerateJob> m_job;
	std::deque<GenerateCompletion> m_completions;
	std::string m_start_log;
	bool m_stopping = false;
};

class FmEmergeGenerate : public std::enable_shared_from_this<FmEmergeGenerate>
{
public:
	explicit FmEmergeGenerate(Server *server);
	~FmEmergeGenerate();

	bool start(const std::string &player_name, const std::string &params,
			std::string &message);

private:
	Server *m_server;
	std::shared_ptr<FmEmergeGenerateState> m_state;
};

FmEmergeGenerate::FmEmergeGenerate(Server *server) :
		m_server(server), m_state(std::make_shared<FmEmergeGenerateState>(server))
{
}

FmEmergeGenerate::~FmEmergeGenerate() = default;

bool FmEmergeGenerate::start(
		const std::string &player_name, const std::string &params, std::string &message)
{
	if (!m_state->prepare(player_name, params, message))
		return false;

	try {
		m_server->enqueueAsyncTask(
				[self = shared_from_this()]() { self->m_state->run_task(); });
		return true;
	} catch (const std::exception &e) {
		message = std::string("Could not start generation task: ") + e.what();
	} catch (...) {
		message = "Could not start generation task.";
	}
	m_state->shutdown();
	return false;
}

namespace
{

int l_emerge_smart(lua_State *L)
{
	const std::string player_name = luaL_checkstring(L, 1);
	const std::string params = luaL_checkstring(L, 2);
	Server *server = ModApiBase::getServer(L);
	auto task = std::make_shared<FmEmergeGenerate>(server);

	std::string message;
	const bool success = task->start(player_name, params, message);
	lua_pushboolean(L, success);
	lua_pushlstring(L, message.c_str(), message.size());
	return 2;
}

} // namespace

void fm_register_emerge_smart_api(lua_State *L, int top)
{
	ModApiBase::registerFunction(L, "emerge_smart", l_emerge_smart, top);
}
