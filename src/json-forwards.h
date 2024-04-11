/*
Minetest
Copyright (C) 2023 DS

This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "config.h"

// FM TODO (fix with submodule)
#if 1 || USE_SYSTEM_JSONCPP
#include <json/version.h>
#include <json/allocator.h>
#include <json/config.h>
#include <json/forwards.h>
#else
#include <json/json-forwards.h>
#endif
