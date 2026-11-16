// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "fm_gravity.h"
#include "network/fm_networkprotocol.h"
#include "util/msgpack_serialize.h"
#include <cmath>
#include <string>
#include <utility>

namespace fm_gravity_sync
{
template <typename Stream>
void packInit(
		msgpack::packer<Stream> &packer, const std::string &sources, float background)
{
	packer.pack(static_cast<packet_field_t>(TOCLIENT_INIT_GRAVITY_SOURCES));
	packer.pack(sources);
	packer.pack(static_cast<packet_field_t>(TOCLIENT_INIT_GRAVITY_BACKGROUND));
	packer.pack(background);
}

// Older init packets have neither field. Invalid or incomplete data changes nothing.
inline bool applyInit(const MsgpackPacket &packet, FmGravityField &field,
		float &movement_gravity, bool &gravity_background)
{
	std::string sources;
	float background;
	try {
		if (!packet_convert_safe(packet, TOCLIENT_INIT_GRAVITY_SOURCES, sources) ||
				!packet_convert_safe(
						packet, TOCLIENT_INIT_GRAVITY_BACKGROUND, background))
			return false;
	} catch (const msgpack::type_error &) {
		return false;
	}
	FmGravityField next;
	if (!std::isfinite(background) || !next.setSources(sources))
		return false;
	field = std::move(next);
	movement_gravity = background;
	gravity_background = background != 0.0f;
	return true;
}
} // namespace fm_gravity_sync
