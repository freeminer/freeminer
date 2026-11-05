// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "map.h"

namespace epixel
{
// Fast map writes do not emit the node-change events clients need.
inline void notifyFallingNodeChange(Map &map, const v3pos_t &pos, MapEditEventType type)
{
	MapEditEvent event;
	event.type = type;
	event.setPositionModified(pos);
	if (type == MEET_ADDNODE)
		event.n = map.getNode(pos);
	map.dispatchEvent(event);
}
}
