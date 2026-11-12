// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#pragma once

#include "server.h"
#include "server/mods.h"
#include "scripting_server.h"
// fm: Builtin scripts query mapgen settings while scripting is initialized.
#include "emerge.h"
#include "map_settings_manager.h"
// ===

class MockServer : public Server
{
public:
	/* Set `path_world` to a real existing folder if you plan to initialize scripting! */
	MockServer(const std::string &path_world = "fakepath") :
		Server(path_world, SubgameSpec("fakespec", "fakespec"), true,
			Address(), true, nullptr
		)
	{}

	/*
	 * Use this in unit tests to create scripting.
	 * Note that you still need to call script->loadBuiltin() and don't forget
	 * a try-catch for `ModError`.
	 */
	void createScripting() {
		// fm: Provide the mapgen state normally created by Server::init().
		m_emerge = std::make_unique<EmergeManager>(this, &m_test_metrics);
		m_map_settings = std::make_unique<MapSettingsManager>(m_path_world + "/map_meta");
		m_emerge->map_settings_mgr = m_map_settings.get();
		// ===
		m_script = std::make_unique<ServerScripting>(this);
		m_modmgr = std::make_unique<ServerModManager>(nullptr);
	}

	void start() = delete;
	void stop() = delete;

private:
	// fm:
	MetricsBackend m_test_metrics;
	std::unique_ptr<MapSettingsManager> m_map_settings;
	// ===
	void SendChatMessage(session_t peer_id, const ChatMessage &message) override {}
};
