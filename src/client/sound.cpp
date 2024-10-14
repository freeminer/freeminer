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

#include "sound.h"

#include "filesys.h"
#include "log.h"
#include "porting.h"
#include "util/numeric.h"
#include <algorithm>
#include <string>
#include <vector>

std::vector<std::string> SoundFallbackPathProvider::
		getLocalFallbackPathsForSoundname(const std::string &name)
{
	std::vector<std::string> paths;

	// only try each name once
	if (m_done_names.count(name))
		return paths;
	m_done_names.insert(name);

	addThePaths(name, paths);

	// remove duplicates
	std::sort(paths.begin(), paths.end());
	auto end = std::unique(paths.begin(), paths.end());
	paths.erase(end, paths.end());

	return paths;
}

void SoundFallbackPathProvider::addAllAlternatives(const std::string &common,
		std::vector<std::string> &paths)
{
	paths.reserve(paths.size() + 11);
	for (auto &&ext : {".ogg", ".0.ogg", ".1.ogg", ".2.ogg", ".3.ogg", ".4.ogg",
			".5.ogg", ".6.ogg", ".7.ogg", ".8.ogg", ".9.ogg", }) {
		paths.push_back(common + ext);
	}
}

void SoundFallbackPathProvider::addThePaths(const std::string &name,
		std::vector<std::string> &paths)
{
	addAllAlternatives(porting::path_share + DIR_DELIM + "sounds" + DIR_DELIM + name, paths);
	addAllAlternatives(porting::path_user + DIR_DELIM + "sounds" + DIR_DELIM + name, paths);
}

void ISoundManager::reportRemovedSound(sound_handle_t id)
{
	if (id <= 0)
		return;

	freeId(id);
	m_removed_sounds.push_back(id);
}

sound_handle_t ISoundManager::allocateId(u32 num_owners)
{
	const auto lock = m_occupied_ids.lock_unique_rec();

	while (m_occupied_ids.find(m_next_id) != m_occupied_ids.end()
			|| m_next_id == SOUND_HANDLE_T_MAX) {
		m_next_id = static_cast<sound_handle_t>(
				myrand() % static_cast<u32>(SOUND_HANDLE_T_MAX - 1) + 1);
	}
	sound_handle_t id = m_next_id++;
	m_occupied_ids.emplace(id, num_owners);
	return id;
}

void ISoundManager::freeId(sound_handle_t id, u32 num_owners)
{
	const auto lock = m_occupied_ids.lock_unique_rec();

	auto it = m_occupied_ids.find(id);
	if (it == m_occupied_ids.end())
		return;
	if (it->second <= num_owners)
		m_occupied_ids.erase(it);
	else
		it->second -= num_owners;
}
