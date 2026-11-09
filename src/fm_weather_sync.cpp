// SPDX-License-Identifier: GPL-3.0-or-later
#include "fm_weather_sync.h"

#include "face_position_cache.h"
#include "fm_weather_simulation.h"
#include "map.h"
#include "mapblock.h"
#include "network/fm_networkprotocol.h"
#include "util/msgpack_serialize.h"

namespace weather
{
namespace
{
bool inside(const v3bpos_t &pos, const v3bpos_t &center, pos_t radius)
{
	// Widen before subtracting so cache pruning is safe even across teleports
	// between opposite ends of the position range.
	return std::abs(int64_t(pos.X) - center.X) <= radius &&
		   std::abs(int64_t(pos.Y) - center.Y) <= radius &&
		   std::abs(int64_t(pos.Z) - center.Z) <= radius;
}
} // namespace

bool Snapshot::differs(const Snapshot &previous) const
{
	return heat != previous.heat || humidity != previous.humidity ||
		   (wind - previous.wind).getLengthSQ() > 0.01f;
}

Snapshot snapshot(const MapBlock &block)
{
	return {
			heat_value(static_cast<float>(block.heat.load()) + block.heat_add.load()),
			humidity_value(static_cast<float>(block.humidity.load()) +
						   block.humidity_add.load()),
			limit_wind(block.wind.load()),
	};
}

std::string encode_updates(const std::vector<Update> &updates)
{
	msgpack::sbuffer buffer;
	msgpack::packer<msgpack::sbuffer> pk(&buffer);
	pk.pack_array(updates.size());
	for (const auto &update : updates) {
		pk.pack_map(5);
		PACK_PK(pk, TOCLIENT_BLOCKDATA_POS, update.pos);
		PACK_PK(pk, TOCLIENT_BLOCKDATA_HEAT, update.value.heat);
		PACK_PK(pk, TOCLIENT_BLOCKDATA_HUMIDITY, update.value.humidity);
		PACK_PK(pk, TOCLIENT_BLOCKDATA_WIND, update.value.wind);
		PACK_PK(pk, TOCLIENT_BLOCKDATA_WEATHER_ONLY, true);
	}
	return {buffer.data(), buffer.size()};
}

bool apply_update(Map &map, MsgpackPacketSafe &packet)
{
	if (!packet.contains(TOCLIENT_BLOCKDATA_WEATHER_ONLY))
		return false;
	try {
		bool weather_only = false;
		if (!packet.convert_safe(TOCLIENT_BLOCKDATA_WEATHER_ONLY, weather_only) ||
				!weather_only)
			return false;
		v3bpos_t pos;
		Snapshot value;
		block_step_t step = 0;
		packet.convert_safe(TOCLIENT_BLOCKDATA_STEP, step);
		if (step || !packet.convert_safe(TOCLIENT_BLOCKDATA_POS, pos) ||
				!packet.convert_safe(TOCLIENT_BLOCKDATA_HEAT, value.heat) ||
				!packet.convert_safe(TOCLIENT_BLOCKDATA_HUMIDITY, value.humidity) ||
				!packet.convert_safe(TOCLIENT_BLOCKDATA_WIND, value.wind))
			return true;
		// Never create a block for an update that overtook initial block data,
		// or for a block that the client has already unloaded.
		const auto block = map.getBlock(pos, false, true);
		if (!block)
			return true;
		block->heat = value.heat;
		block->humidity = humidity_value(value.humidity);
		block->heat_add = 0;
		block->humidity_add = 0;
		block->wind = limit_wind(value.wind);
	} catch (const msgpack::type_error &) {
		// Decode everything before changing state; malformed updates are atomic.
	}
	return true;
}

bool ClientSync::begin(const v3bpos_t &center, pos_t radius, uint64_t now_ms)
{
	radius = std::clamp<pos_t>(radius, 0, 8);
	if (!m_initialized || m_center != center || m_radius != radius) {
		m_center = center;
		m_radius = radius;
		m_layer = 0;
		m_index = 0;
		m_next_scan_ms = 0;
		// Preserve overlapping deliveries while moving; a block boundary must
		// not resend an entire otherwise unchanged weather neighborhood.
		for (auto it = m_sent.begin(); it != m_sent.end();) {
			if (!inside(it->first, center, radius))
				it = m_sent.erase(it);
			else
				++it;
		}
		m_initialized = true;
	}
	return now_ms >= m_next_scan_ms;
}

std::optional<v3bpos_t> ClientSync::next(uint64_t now_ms)
{
	if (!m_initialized || now_ms < m_next_scan_ms)
		return std::nullopt;
	while (m_layer <= m_radius) {
		const auto &positions = FacePositionCache::getFacePositions(m_layer);
		if (m_index < positions.size()) {
			const auto &offset = positions[m_index++];
			return m_center + v3bpos_t(offset.X, offset.Y, offset.Z);
		}
		++m_layer;
		m_index = 0;
	}
	m_layer = 0;
	m_next_scan_ms = now_ms + 2000;
	return std::nullopt;
}

bool ClientSync::changed(const v3bpos_t &pos, const Snapshot &value) const
{
	const auto it = m_sent.find(pos);
	return it == m_sent.end() || value.differs(it->second);
}

void ClientSync::sent(const v3bpos_t &pos, const Snapshot &value)
{
	if (m_initialized && inside(pos, m_center, m_radius))
		m_sent.insert_or_assign(pos, value);
}
} // namespace weather
