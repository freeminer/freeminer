/*
mainmenumanager.h
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

/*
	All kinds of stuff that needs to be exposed from main.cpp
*/
#include "modalMenu.h"
#include <cassert>
#include <list>

class IGameCallback
{
public:
	virtual void exitToOS() = 0;
	virtual void keyConfig() = 0;
	virtual void disconnect() = 0;
	virtual void changePassword() = 0;
	virtual void changeVolume() = 0;

	virtual void signalKeyConfigChange() = 0;
};

extern gui::IGUIEnvironment *guienv;
extern gui::IGUIStaticText *guiroot;

// Handler for the modal menus

class MainMenuManager : public IMenuManager
{
public:
	virtual void createdMenu(gui::IGUIElement *menu)
	{
#ifndef NDEBUG
		for (gui::IGUIElement *i : m_stack) {
			assert(i != menu);
		}
#endif

		if(!m_stack.empty())
			m_stack.back()->setVisible(false);
		m_stack.push_back(menu);
		guienv->setFocus(m_stack.back());
	}

	virtual void deletingMenu(gui::IGUIElement *menu)
	{
		// Remove all entries if there are duplicates
		m_stack.remove(menu);

		if(!m_stack.empty()) {
			m_stack.back()->setVisible(true);
			guienv->setFocus(m_stack.back());
		}
	}

	// Returns true to prevent further processing
	virtual bool preprocessEvent(const SEvent& event)
	{
		if (m_stack.empty())
			return false;
		GUIModalMenu *mm = dynamic_cast<GUIModalMenu*>(m_stack.back());
		return mm && mm->preprocessEvent(event);
	}

	u32 menuCount()
	{
		return m_stack.size();
	}

	bool pausesGame()
	{
		for (gui::IGUIElement *i : m_stack) {
			GUIModalMenu *mm = dynamic_cast<GUIModalMenu*>(i);
			if (mm && mm->pausesGame())
				return true;
		}
		return false;
	}

	std::list<gui::IGUIElement*> m_stack;
};

extern MainMenuManager g_menumgr;

extern bool isMenuActive();

class MainGameCallback : public IGameCallback
{
public:
	MainGameCallback() = default;
	virtual ~MainGameCallback() = default;

	virtual void exitToOS()
	{
		shutdown_requested = true;
	}

	virtual void disconnect()
	{
		disconnect_requested = true;
	}

	virtual void changePassword()
	{
		changepassword_requested = true;
	}

	virtual void changeVolume()
	{
		changevolume_requested = true;
	}

	virtual void keyConfig()
	{
		keyconfig_requested = true;
	}

	virtual void signalKeyConfigChange()
	{
		keyconfig_changed = true;
	}


	bool disconnect_requested = false;
	bool changepassword_requested = false;
	bool changevolume_requested = false;
	bool keyconfig_requested = false;
	bool shutdown_requested = false;

	bool keyconfig_changed = false;
};

extern MainGameCallback *g_gamecallback;
