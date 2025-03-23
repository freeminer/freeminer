// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 cx384

#pragma once

#include <memory>
#include <string>
#include "irr_v3d.h"
#include "scripting_pause_menu.h"

class Client;
class RenderingEngine;
class InputHandler;
class ISoundManager;
class GUIFormSpecMenu;

/*
This object intend to contain the core fromspec functionality.
It includes:
  - methods to show specific formspec menus
  - storing the opened fromspec
  - handling fromspec related callbacks
 */
struct GameFormSpec
{
	void init(Client *client, RenderingEngine *rendering_engine, InputHandler *input);

	~GameFormSpec();

	void showFormSpec(const std::string &formspec, const std::string &formname);
	void showCSMFormSpec(const std::string &formspec, const std::string &formname);
	// Used by the Lua pause menu environment to show formspecs.
	// Currently only used for the in-game settings menu.
	void showPauseMenuFormSpec(const std::string &formspec, const std::string &formname);
	void showNodeFormspec(const std::string &formspec, const v3s16 &nodepos);
	void showPlayerInventory();
	void showDeathFormspecLegacy();
	// Shows the hardcoded "main" pause menu.
	void showPauseMenu();

	void update();
	void disableDebugView();

	bool handleCallbacks();

#ifdef __ANDROID__
	// Returns false if no formspec open
	bool handleAndroidUIInput();
#endif

private:
	Client *m_client;
	RenderingEngine *m_rendering_engine;
	InputHandler *m_input;
	std::unique_ptr<PauseMenuScripting> m_pause_script;

	// Default: "". If other than "": Empty show_formspec packets will only
	// close the formspec when the formname matches
	std::string m_formname;
	GUIFormSpecMenu *m_formspec = nullptr;

	bool handleEmptyFormspec(const std::string &formspec, const std::string &formname);

	void deleteFormspec();
};
