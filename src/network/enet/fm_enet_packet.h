// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "network/mtp/internal.h"
#include <enet/enet.h>
#include <memory>

namespace con
{
inline ENetPacket *makeEnetPacket(const ConnectionCommandPtr &command)
{
	// Retain the owning command: reliable packets can outlive their producer
	// and must keep their payload intact for retransmission.
	auto owner = std::make_unique<ConnectionCommandPtr>(command);
	const auto flags = ENET_PACKET_FLAG_NO_ALLOCATE |
					   (command->reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
	auto *packet = enet_packet_create(*command->data, command->data.getSize(), flags);
	if (!packet)
		return nullptr;
	packet->userData = owner.release();
	packet->freeCallback = [](ENetPacket *packet) {
		delete static_cast<ConnectionCommandPtr *>(packet->userData);
	};
	return packet;
}
} // namespace con
