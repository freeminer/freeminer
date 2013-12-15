/*
Copyright (C) 2013 xyz, Ilya Zhuravlev <whatever@xyz.is>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "touchscreengui.h"
#include "irrlichttypes.h"
#include "irr_v2d.h"
#include "log.h"
#include "keycode.h"
#include "settings.h"
#include "gettime.h"

#include <iostream>
#include <algorithm>

#include <ISceneCollisionManager.h>

using namespace irr::core;

extern Settings *g_settings;

enum {
	forward_id = 50, // change to random number when shit breaks
	backward_id,
	left_id,
	right_id,
	jump_id
};

TouchScreenGUI::TouchScreenGUI(IrrlichtDevice *device):
	m_device(device),
	m_guienv(device->getGUIEnvironment()),
	m_camera_yaw(0.0),
	m_camera_pitch(0.0),
	m_down(false),
	m_down_pointer_id(0),
	m_down_since(0),
	m_digging(false),
	m_rightclick(false),
	m_player_item_changed(false),
	m_player_item(0),
	m_hud_start_y(100000)
{
	m_screensize = m_device->getVideoDriver()->getScreenSize();
}

void TouchScreenGUI::init() {
	u32 control_pad_size = (2 * m_screensize.Y) / 3;
	u32 button_size = control_pad_size / 3;
	m_down = false;
	m_digging = false;

	m_control_pad_rect = rect<s32>(0, m_screensize.Y - 3 * button_size, 3 * button_size, m_screensize.Y);

	/*
	draw control pad
	0 1 2
	3 4 5
	6 7 8
	for now only 1, 3, 4, 5 and 7 are used
	*/
	int number = 0;
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x, ++number) {
			rect<s32> button_rect(
					x * button_size, m_screensize.Y - button_size * (3 - y),
					(x + 1) * button_size, m_screensize.Y - button_size * (2 - y)
			);
			u32 id = 0;
			std::wstring caption;
			switch (number) {
			case 1:
				id = forward_id;
				caption = L"^";
				break;
			case 3:
				id = left_id;
				caption = L"<";
				break;
			case 4:
				id = jump_id;
				caption = L"x";
				break;
			case 5:
				id = right_id;
				caption = L">";
				break;
			case 7:
				id = backward_id;
				caption = L"v";
				break;
			}
			if (id)
				m_guienv->addButton(button_rect, 0, id, caption.c_str());
		}
}

TouchScreenGUI::~TouchScreenGUI() {}

void TouchScreenGUI::OnEvent(const SEvent &event) {
	if (event.EventType == EET_MULTI_TOUCH_EVENT) {
		//leftclicked = false;
		//leftreleased = false;
		keyIsDown.unset(getKeySetting("keymap_forward"));
		keyIsDown.unset(getKeySetting("keymap_backward"));
		keyIsDown.unset(getKeySetting("keymap_left"));
		keyIsDown.unset(getKeySetting("keymap_right"));
		keyIsDown.unset(getKeySetting("keymap_jump"));

		bool main_pointer_still_here = false;

		for (int i = 0; i < event.MultiTouchInput.PointerCount; ++i) {
			s32 x = event.MultiTouchInput.X[i];
			s32 y = event.MultiTouchInput.Y[i];
			if (event.MultiTouchInput.ID[i] == m_down_pointer_id)
				m_down_to = v2s32(x, y);
			if (!event.MultiTouchInput.Touched[i])
				continue;
			if (event.MultiTouchInput.ID[i] == m_down_pointer_id)
				main_pointer_still_here = true;
			IGUIElement *element;
			if ((element = m_guienv->getRootGUIElement()->getElementFromPoint(v2s32(x, y)))) {
				std::string key = "";
				switch (element->getID()) {
				case forward_id:
					key = "forward";
					break;
				case left_id:
					key = "left";
					break;
				case right_id:
					key = "right";
					break;
				case backward_id:
					key = "backward";
					break;
				case jump_id:
					key = "jump";
					break;
				}

				if (key != "") {
					keyIsDown.set(getKeySetting(("keymap_" + key).c_str()));
					keyWasDown.set(getKeySetting(("keymap_" + key).c_str()));
				}
			}

			// perhaps this actually should track a pointer and store its MultiTouchInput.ID[i] somehow
			if (!m_control_pad_rect.isPointInside(v2s32(x, y))) {
				// update camera_yaw and camera_pitch
				s32 dx = x - event.MultiTouchInput.PrevX[i];
				s32 dy = y - event.MultiTouchInput.PrevY[i];

				float d = g_settings->getFloat("mouse_sensitivity");

				m_camera_yaw -= dx * d;
				m_camera_pitch += dy * d;

				// update shootline
				m_shootline = m_device->getSceneManager()->getSceneCollisionManager()->getRayFromScreenCoordinates(v2s32(x, y));

				if (!m_down) {
					m_down = true;
					m_down_pointer_id = event.MultiTouchInput.ID[i];
					m_down_since = getTimeMs();
					m_down_from = v2s32(x, y);
					m_down_to = m_down_from;
				}
			}

			// check if hud item is pressed
			for (int j = 0; j < m_hud_rects.size(); ++j)
				if (m_hud_rects[j].isPointInside(v2s32(x, y))) {
					m_player_item = j;
					m_player_item_changed = true;
					break;
				}
		}

		if (!main_pointer_still_here) {
			// TODO: tweak this
			// perhaps this should only right click when not digging?
			if (m_down_to.Y < m_hud_start_y && m_down && m_down_from.getDistanceFromSQ(m_down_to) < 400)
				m_rightclick = true;
			m_down = false;
			m_digging = false;
		}
	}
}

bool TouchScreenGUI::isKeyDown(const KeyPress &keyCode) {
	return keyIsDown[keyCode];
}

bool TouchScreenGUI::wasKeyDown(const KeyPress &keyCode) {
	bool b = keyWasDown[keyCode];
	if (b)
		keyWasDown.unset(keyCode);
	return b;
}

bool TouchScreenGUI::getLeftState() {
	return m_digging;
}

void TouchScreenGUI::step(float dtime) {
	if (m_down) {
		u32 dtime = getTimeMs() - m_down_since;
		if (dtime > 300)
			m_digging = true;
	}
}

void TouchScreenGUI::resetHud() {
	m_hud_rects.clear();
	m_hud_start_y = 100000;
}

void TouchScreenGUI::registerHudItem(int index, const rect<s32> &rect) {
	m_hud_start_y = std::min((int)m_hud_start_y, rect.UpperLeftCorner.Y);
	m_hud_rects.push_back(rect);
}

u16 TouchScreenGUI::getPlayerItem() {
	m_player_item_changed = false;
	return m_player_item;
}

s32 TouchScreenGUI::getHotbarImageSize() {
	return m_screensize.Y / 10;
}
