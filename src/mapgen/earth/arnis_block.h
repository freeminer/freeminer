#pragma once
#include "mapnode.h"
#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace arnis
{
using BlockProperties = std::unordered_map<std::string, std::string>;

class Block : public MapNode
{
public:
	// Content ID zero can belong to a registered node, including water.
	Block(content_t c = CONTENT_AIR) : MapNode{c} {}

	content_t id() const { return getContent(); }
	std::optional<BlockProperties> properties() const;
};
class BlockWithProperties
{
public:
	using Properties = BlockProperties;

	Block block;
	// Java/Sponge block-state values.  MapNode has no portable generic state
	// channel, but retaining these makes the C++ model equivalent to Rust and
	// allows format-specific world writers to encode them. Immutable property
	// maps are interned and shared like Rust's Arc<Value> block states.
	std::shared_ptr<const Properties> properties;

	BlockWithProperties(Block b = {}) : block(b) {}
	BlockWithProperties(Block b, Properties p) :
			block(b), properties(intern_properties(std::move(p)))
	{
	}
	BlockWithProperties(Block b, std::shared_ptr<const Properties> p) :
			block(b), properties(std::move(p))
	{
	}
	const Properties &property_values() const
	{
		static const Properties empty;
		return properties ? *properties : empty;
	}
	static BlockWithProperties simple(Block b) { return BlockWithProperties{b}; }

	static std::shared_ptr<const Properties> intern_properties(Properties value)
	{
		if (value.empty())
			return {};
		// Most block states recur many times while a building is emitted. Check
		// a bounded worker-local weak cache before sorting/serializing the map or
		// touching the shared intern table.
		struct LocalStateCache
		{
			std::unordered_map<std::size_t, std::vector<std::weak_ptr<const Properties>>>
					states;
			std::size_t entry_count = 0;

			std::shared_ptr<const Properties> find(
					std::size_t hash, const Properties &value)
			{
				const auto found = states.find(hash);
				if (found == states.end())
					return {};
				auto &entries = found->second;
				for (auto it = entries.begin(); it != entries.end();) {
					if (auto shared = it->lock()) {
						if (*shared == value)
							return shared;
						++it;
					} else {
						it = entries.erase(it);
						--entry_count;
					}
				}
				if (entries.empty())
					states.erase(found);
				return {};
			}

			void remember(
					std::size_t hash, const std::shared_ptr<const Properties> &value)
			{
				if (entry_count >= 512) {
					for (auto bucket = states.begin(); bucket != states.end();) {
						auto &entries = bucket->second;
						for (auto it = entries.begin(); it != entries.end();) {
							if (it->expired()) {
								it = entries.erase(it);
								--entry_count;
							} else {
								++it;
							}
						}
						if (entries.empty())
							bucket = states.erase(bucket);
						else
							++bucket;
					}
					if (entry_count >= 512) {
						auto bucket = states.begin();
						bucket->second.erase(bucket->second.begin());
						--entry_count;
						if (bucket->second.empty())
							states.erase(bucket);
					}
				}
				states[hash].push_back(value);
				++entry_count;
			}
		};
		static thread_local LocalStateCache local_cache;
		std::size_t local_hash = value.size();
		for (const auto &[name, state] : value) {
			std::size_t pair_hash = std::hash<std::string>{}(name);
			pair_hash ^= std::hash<std::string>{}(state) + 0x9e3779b97f4a7c15ULL +
						 (pair_hash << 6) + (pair_hash >> 2);
			local_hash ^= pair_hash * 0x9e3779b97f4a7c15ULL;
		}
		if (auto shared = local_cache.find(local_hash, value))
			return shared;
		// Match Rust's weak, sharded intern table: block sections own the states,
		// and the cache must not keep them alive after the last placed block is
		// released. This matters for long-lived mapgen worker threads, whose
		// thread-local strong caches would otherwise retain every state they saw.
		struct StateShard
		{
			std::mutex mutex;
			std::unordered_map<std::string, std::weak_ptr<const Properties>> states;
			std::size_t sweep_at = 64;
		};
		struct StateCache
		{
			std::array<StateShard, 16> shards;
		};
		static StateCache cache;

		std::vector<std::pair<std::string, std::string>> ordered(
				value.begin(), value.end());
		std::sort(ordered.begin(), ordered.end());
		std::string key;
		for (const auto &[name, state] : ordered) {
			key += std::to_string(name.size()) + ':' + name;
			key += std::to_string(state.size()) + ':' + state;
		}
		std::size_t shard_hash = 0;
		for (const unsigned char byte : key)
			shard_hash = shard_hash * 31u + byte;
		auto &shard = cache.shards[shard_hash % cache.shards.size()];
		std::lock_guard lock(shard.mutex);
		if (const auto it = shard.states.find(key); it != shard.states.end())
			if (auto shared = it->second.lock()) {
				local_cache.remember(local_hash, shared);
				return shared;
			}
		if (shard.states.size() >= shard.sweep_at) {
			for (auto it = shard.states.begin(); it != shard.states.end();) {
				if (it->second.expired())
					it = shard.states.erase(it);
				else
					++it;
			}
			shard.sweep_at = std::max<std::size_t>(64, shard.states.size() * 2);
		}
		auto shared = std::make_shared<const Properties>(std::move(value));
		shard.states.insert_or_assign(key, shared);
		local_cache.remember(local_hash, shared);
		return shared;
	}
};

// Rust's Block::properties() defaults for significant Java block states.
BlockWithProperties::Properties default_block_properties(const Block &block);

}
