// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS

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
