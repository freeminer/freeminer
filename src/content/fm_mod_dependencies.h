// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "mods.h"
#include "filesys.h"
#include "settings.h"
#include "log.h"
#include "util/string.h"

#include <unordered_map>
#include <unordered_set>
#include <functional>

// Find installed dependencies without changing world.mt.
inline std::vector<ModSpec> fm_select_dependency_mods(
		const std::vector<ModSpec> &selected,
		const std::unordered_map<std::string, std::string> &mod_paths,
		const std::string &settings_path)
{
	Settings conf;
	conf.readConfigFile(settings_path.c_str());
	std::unordered_map<std::string, std::vector<ModSpec>> available;
	// getModsInPath stores mods in a name-keyed set, losing duplicates.
	std::function<void(const std::string &, const std::string &, int)> scan;
	scan = [&](const std::string &path, const std::string &vpath, int depth) {
		for (const auto &entry : fs::GetDirListing(path)) {
			if (!entry.dir || entry.name.empty() || entry.name[0] == '.')
				continue;
			ModSpec mod(entry.name, fs::AbsolutePath(path + DIR_DELIM + entry.name),
					depth, vpath + "/" + entry.name);
			if (fs::IsFile(mod.path + DIR_DELIM "modpack.conf") ||
					fs::IsFile(mod.path + DIR_DELIM "modpack.txt")) {
				scan(mod.path, mod.virtual_path, depth + 1);
				continue;
			}
			if (!parseModContents(mod))
				continue;
			available[mod.name].push_back(std::move(mod));
		}
	};
	for (const auto &path : mod_paths)
		scan(path.second, path.first, 0);

	std::unordered_set<std::string> selected_names;
	std::vector<const ModSpec *> pending;
	for (const auto &mod : selected) {
		selected_names.insert(mod.name);
		pending.push_back(&mod);
	}

	std::vector<ModSpec> result;
	while (!pending.empty()) {
		const ModSpec &mod = *pending.back();
		pending.pop_back();
		for (const auto *dependencies : {&mod.depends, &mod.optdepends}) {
			for (const auto &name : *dependencies) {
				if (selected_names.count(name))
					continue;
				auto it = available.find(name);
				if (it == available.end())
					continue;
				const std::string key = "load_mod_" + name;
				std::string requested = conf.exists(key) ? conf.get(key) : "";
				if (requested == "false" || requested == "nil")
					continue;
				const ModSpec *candidate = nullptr;
				for (const auto &option : it->second) {
					if (!requested.empty() && !is_yes(requested) &&
							requested != option.virtual_path)
						continue;
					if (candidate) {
						candidate = nullptr;
						warningstream << "Multiple installed candidates for dependency \""
									  << name
									  << "\"; select one in the world configuration."
									  << std::endl;
						break;
					}
					candidate = &option;
				}
				if (!candidate)
					continue;
				selected_names.insert(name);
				result.push_back(*candidate);
				pending.push_back(candidate);
			}
		}
	}
	return result;
}
