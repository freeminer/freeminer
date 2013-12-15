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

#ifndef TOUCHSCREENGUI_HEADER
#define TOUCHSCREENGUI_HEADER

#include <IGUIEnvironment.h>
#include <IGUIButton.h>
#include <IEventReceiver.h>

#include "game.h"

using namespace irr;
using namespace irr::core;
using namespace irr::gui;

class TouchScreenGUI {
public:
	TouchScreenGUI(IrrlichtDevice *device);
	~TouchScreenGUI();
	void OnEvent(const SEvent &event, KeyList *keyIsDown, KeyList *keyWasDown);
	double getYaw() { return m_camera_yaw; }
	double getPitch() { return m_camera_pitch; }
	line3d<f32> getShootline() { return m_shootline; }
private:
	IrrlichtDevice *m_device;
	IGUIEnvironment *m_guienv;
	IGUIButton *m_button;
	rect<s32> m_control_pad_rect;
	double m_camera_yaw;
	double m_camera_pitch;
	line3d<f32> m_shootline;
};
#endif
