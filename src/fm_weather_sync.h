// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fm_weather.h"
#include "util/unordered_map_hash.h"

#include <optional>
#include <string>
#include <vector>

class Map;
class MapBlock;
class MsgpackPacketSafe;

namespace weather
{
constexpr uint16_t sync_protocol_version = 4;

struct Snapshot
{
	heat_t heat = 0;
	humidity_t humidity = 0;
	wind_t wind{};
	bool differs(const Snapshot &previous) const;
};

struct Update
{
	v3bpos_t pos;
	Snapshot value;
};

Snapshot snapshot(const MapBlock &block);
std::string encode_updates(const std::vector<Update> &updates);
// Returns true for a weather-only message, including an invalid/missing-block
// update. Such messages must never fall through to voxel loading or meshing.
bool apply_update(Map &map, MsgpackPacketSafe &packet);

// Protected by the RemoteClient lock. A bounded, center-out scan of nearby
// blocks, with a separate delivery cache that never marks voxel data as sent.
class ClientSync
{
public:
	bool begin(const v3bpos_t &center, pos_t radius, uint64_t now_ms);
	std::optional<v3bpos_t> next(uint64_t now_ms);
	bool changed(const v3bpos_t &pos, const Snapshot &value) const;
	void sent(const v3bpos_t &pos, const Snapshot &value);
	void forget(const v3bpos_t &pos) { m_sent.erase(pos); }
	size_t size() const { return m_sent.size(); }

private:
	v3bpos_t m_center{};
	pos_t m_radius = 0;
	pos_t m_layer = 0;
	size_t m_index = 0;
	uint64_t m_next_scan_ms = 0;
	bool m_initialized = false;
	unordered_map_v3bpos<Snapshot> m_sent;
};
} // namespace weather
