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

#include <vector>

#include "game.h"

using namespace irr;
using namespace irr::core;
using namespace irr::gui;


class TouchScreenGUI : public InputHandler {
public:
	TouchScreenGUI(IrrlichtDevice *device);
	~TouchScreenGUI();
	void init();
	void OnEvent(const SEvent &event);
	double getYaw() { return m_camera_yaw; }
	double getPitch() { return m_camera_pitch; }
	line3d<f32> getShootline() { return m_shootline; }

	bool isKeyDown(const KeyPress &keyCode);
	bool wasKeyDown(const KeyPress &keyCode);
	v2s32 getMousePos() { return m_down_to; }
	void setMousePos(s32 x, s32 y) {}
	bool getLeftState();
	bool getRightState() { return m_rightclick; }
	bool getLeftClicked() { return false; }
	bool getRightClicked() { return m_rightclick; }
	void resetLeftClicked() {}
	void resetRightClicked() { m_rightclick = false; }
	bool getLeftReleased() { return false; }
	bool getRightReleased() { return m_rightclick; }
	void resetLeftReleased() {}
	void resetRightReleased() { m_rightclick = false; }
	s32 getMouseWheel() { return 0; }
	void step(float dtime);
	void resetHud();
	void registerHudItem(int index, const rect<s32> &rect);
	bool hasPlayerItemChanged() { return m_player_item_changed; }
	u16 getPlayerItem();
	s32 getHotbarImageSize();
	void Toggle(bool visible);
	void Hide();
	void Show();

	bool isSingleClick();
	bool isDoubleClick();
	void resetClicks();
private:
	IrrlichtDevice *m_device;
	IGUIEnvironment *m_guienv;
	IGUIButton *m_button;
	rect<s32> m_control_pad_rect;
	double m_camera_yaw;
	double m_camera_pitch;
	line3d<f32> m_shootline;
	bool m_down;
	s32 m_down_pointer_id;
	u32 m_down_since;
	v2s32 m_down_from; // first known position
	v2s32 m_down_to; // last known position

	bool m_digging;
	bool m_rightclick;

	KeyList keyIsDown;
	KeyList keyWasDown;

	std::vector<rect<s32> > m_hud_rects;

	bool m_player_item_changed;
	u16 m_player_item;

	v2u32 m_screensize;

	u32 m_hud_start_y;
	bool m_visible; // is the gui visible

	bool m_double_click;
	int m_previous_click_time;
};
#endif
