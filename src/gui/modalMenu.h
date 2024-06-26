/*
modalMenu.h
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

#include "irrlichttypes_extrabloated.h"
#include "irr_ptr.h"
#include "util/string.h"

// fm:
#include "client/keycode.h"
#ifdef __ANDROID__
#include "porting_android.h"
#endif

class GUIModalMenu;

class IMenuManager
{
public:
	// A GUIModalMenu calls these when this class is passed as a parameter
	virtual void createdMenu(gui::IGUIElement *menu) = 0;
	virtual void deletingMenu(gui::IGUIElement *menu) = 0;
};

// Remember to drop() the menu after creating, so that it can
// remove itself when it wants to.

class GUIModalMenu : public gui::IGUIElement
{
public:
	GUIModalMenu(gui::IGUIEnvironment* env, gui::IGUIElement* parent, s32 id,
		IMenuManager *menumgr, bool remap_dbl_click = true);
	virtual ~GUIModalMenu();

	void allowFocusRemoval(bool allow);
	bool canTakeFocus(gui::IGUIElement *e);
	void draw();
	void quitMenu();

	virtual void regenerateGui(v2u32 screensize) = 0;
	virtual void drawMenu() = 0;
	virtual bool preprocessEvent(const SEvent& event);
	virtual bool OnEvent(const SEvent& event) {
		if(event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown) {
			KeyPress kp(event.KeyInput);
			if (kp == EscapeKey || kp == CancelKey) {
				quitMenu();
				return true;
			}
		}

		return false;
	};

	virtual bool pausesGame() { return false; } // Used for pause menu
#ifdef __ANDROID__
	virtual bool getAndroidUIInput() { return false; }
	bool hasAndroidUIInput();
#endif

protected:
	virtual std::wstring getLabelByID(s32 id) = 0;
	virtual std::string getNameByID(s32 id) = 0;

	/**
	 * check if event is part of a double click
	 * @param event event to evaluate
	 * @return true/false if a doubleclick was detected
	 */
	bool DoubleClickDetection(const SEvent &event);

	v2s32 m_pointer;
	v2s32 m_old_pointer;  // Mouse position after previous mouse event
	v2u32 m_screensize_old;
	float m_gui_scale;
#ifdef __ANDROID__
	std::string m_jni_field_name;
#endif
#ifdef HAVE_TOUCHSCREENGUI
	// This is set to true if the menu is currently processing a second-touch event.
	bool m_second_touch = false;
	bool m_touchscreen_visible = true;
#endif

private:
	struct clickpos
	{
		v2s32 pos;
		s64 time;
	};
	clickpos m_doubleclickdetect[2];

	IMenuManager *m_menumgr;
	/* If true, remap a double-click (or double-tap) action to ESC. This is so
	 * that, for example, Android users can double-tap to close a formspec.
	*
	 * This value can (currently) only be set by the class constructor
	 * and the default value for the setting is true.
	 */
	bool m_remap_dbl_click;
	// This might be necessary to expose to the implementation if it
	// wants to launch other menus
	bool m_allow_focus_removal = false;

#ifdef HAVE_TOUCHSCREENGUI
	irr_ptr<gui::IGUIElement> m_hovered;

	bool simulateMouseEvent(gui::IGUIElement *target, ETOUCH_INPUT_EVENT touch_event);
	void enter(gui::IGUIElement *element);
	void leave();
#endif
};
