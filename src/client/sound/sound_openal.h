/*
sound_openal.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
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

#include "client/sound.h"
#include <memory>

namespace sound { class SoundManagerSingleton; }
using sound::SoundManagerSingleton;

extern std::shared_ptr<SoundManagerSingleton> g_sound_manager_singleton;

std::shared_ptr<SoundManagerSingleton> createSoundManagerSingleton();
std::unique_ptr<ISoundManager> createOpenALSoundManager(
		SoundManagerSingleton *smg,
		std::unique_ptr<SoundFallbackPathProvider> fallback_path_provider);
