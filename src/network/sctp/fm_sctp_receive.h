// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "util/pointer.h"
#include <optional>
#include <string>

namespace con_sctp
{
inline std::optional<SharedBuffer<u8>> assembleSctpMessage(
		std::string &partial, const u8 *data, size_t size, bool complete)
{
	if (complete && partial.empty())
		return SharedBuffer<u8>(data, size);
	partial.append(reinterpret_cast<const char *>(data), size);
	if (!complete)
		return std::nullopt;
	SharedBuffer<u8> message(
			reinterpret_cast<const u8 *>(partial.data()), partial.size());
	partial.clear();
	return message;
}
} // namespace con_sctp
