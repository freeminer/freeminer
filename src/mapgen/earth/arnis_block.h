#pragma once
#include "mapnode.h"
#include <nlohmann/json_fwd.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
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

// Rust's fastnbt::Value keeps the NBT tag on every value (notably BYTE_ARRAY,
// INT_ARRAY and LONG_ARRAY, which JSON alone cannot distinguish from lists).
// Keep an equivalent recursive value for backend-neutral palettes and schematics.
struct NbtValue
{
	enum class Tag : std::uint8_t
	{
		Null,
		Byte,
		Short,
		Int,
		Long,
		Float,
		Double,
		ByteArray,
		String,
		List,
		Compound,
		IntArray,
		LongArray,
	};

	Tag tag = Tag::Null;
	std::int64_t integer = 0;
	double decimal = 0.0;
	std::string text;
	std::vector<std::int8_t> bytes;
	std::vector<std::int32_t> ints;
	std::vector<std::int64_t> longs;
	std::vector<NbtValue> list;
	std::map<std::string, NbtValue> compound;

	static NbtValue byte(std::int8_t value);
	static NbtValue short_(std::int16_t value);
	static NbtValue int_(std::int32_t value);
	static NbtValue long_(std::int64_t value);
	static NbtValue float_(float value);
	static NbtValue double_(double value);
	static NbtValue byte_array(std::vector<std::int8_t> value);
	static NbtValue string(std::string value);
	static NbtValue list_value(std::vector<NbtValue> value);
	static NbtValue compound_value(std::map<std::string, NbtValue> value);
	static NbtValue int_array(std::vector<std::int32_t> value);
	static NbtValue long_array(std::vector<std::int64_t> value);
	static NbtValue from_json(const nlohmann::json &value);
	bool operator==(const NbtValue &other) const;
	bool operator==(const nlohmann::json &other) const;
};

class Block : public MapNode
{
public:
	// Content ID zero can belong to a registered node, including water.
	Block(content_t c = CONTENT_AIR) : MapNode{c} {}
	// Preserve Luanti's packed orientation/shape fields when an existing node is
	// read back for a material-only transformation (winter and ruin weathering).
	Block(const MapNode &node) : MapNode{node} {}

	content_t id() const { return getContent(); }
	std::optional<BlockProperties> properties() const;
};
class BlockWithProperties
{
public:
	using Properties = BlockProperties;
	using PropertyValue = NbtValue;

	Block block;
	// Java/Sponge block-state values.  MapNode has no portable generic state
	// channel, but retaining these makes the C++ model equivalent to Rust and
	// allows format-specific world writers to encode them. Immutable property
	// maps are interned and shared like Rust's Arc<Value> block states.
	std::shared_ptr<const Properties> properties;
	// Rust stores the full fastnbt::Value here. Most generated block states are
	// compounds of strings and use `properties`; retain other NBT-shaped values
	// too for library callers and palette construction.
	std::shared_ptr<const PropertyValue> nbt_properties;

	BlockWithProperties(Block b = {}) : block(b) {}
	BlockWithProperties(Block b, Properties p) :
			block(b), properties(intern_properties(std::move(p)))
	{
	}
	BlockWithProperties(Block b, std::shared_ptr<const Properties> p) :
			block(b), properties(std::move(p))
	{
	}
	BlockWithProperties(Block b, std::shared_ptr<const Properties> p,
			std::shared_ptr<const PropertyValue> value) :
			block(b), properties(std::move(p)), nbt_properties(std::move(value))
	{
	}
	static BlockWithProperties from_value(Block b, PropertyValue value);
	static BlockWithProperties from_value(Block b, const nlohmann::json &value);
	static std::shared_ptr<const PropertyValue> intern_value(PropertyValue value);
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

// Stable, type-sensitive representation used to intern recursive property values.
std::string canonical_property_value(const BlockWithProperties::PropertyValue &value);

// Rust's Block::properties() defaults for significant Java block states.
BlockWithProperties::Properties default_block_properties(const Block &block);

}
