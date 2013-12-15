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

#include <iostream>

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
	m_camera_pitch(0.0)
{
	v2u32 screensize = m_device->getVideoDriver()->getScreenSize();
	u32 control_pad_size = (2 * screensize.Y) / 3;
	u32 button_size = control_pad_size / 3;

	m_control_pad_rect = rect<s32>(0, screensize.Y - 3 * button_size, 3 * button_size, screensize.Y);

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
					x * button_size, screensize.Y - button_size * (3 - y),
					(x + 1) * button_size, screensize.Y - button_size * (2 - y)
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

void TouchScreenGUI::OnEvent(const SEvent &event, KeyList *keyIsDown, KeyList *keyWasDown) {
	if (event.EventType == EET_MULTI_TOUCH_EVENT) {
		keyIsDown->unset(getKeySetting("keymap_forward"));
		keyIsDown->unset(getKeySetting("keymap_backward"));
		keyIsDown->unset(getKeySetting("keymap_left"));
		keyIsDown->unset(getKeySetting("keymap_right"));
		keyIsDown->unset(getKeySetting("keymap_jump"));

		for (int i = 0; i < NUMBER_OF_MULTI_TOUCHES; ++i) {
			if (!event.MultiTouchInput.Touched[i])
				continue;
			s32 x = event.MultiTouchInput.X[i];
			s32 y = event.MultiTouchInput.Y[i];
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
					keyIsDown->set(getKeySetting(("keymap_" + key).c_str()));
					keyWasDown->set(getKeySetting(("keymap_" + key).c_str()));
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
			}
		}
	}
}
