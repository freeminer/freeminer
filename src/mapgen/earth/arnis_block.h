#pragma once
#include "mapnode.h"
#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace arnis
{
class Block : public MapNode
{
public:
	// Content ID zero can belong to a registered node, including water.
	Block(content_t c = CONTENT_AIR) : MapNode{c} {}

	content_t id() const { return getContent(); }
};
class BlockWithProperties
{
public:
	using Properties = std::unordered_map<std::string, std::string>;

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
			if (auto shared = it->second.lock())
				return shared;
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
		shard.states.insert_or_assign(std::move(key), shared);
		return shared;
	}
};

}
