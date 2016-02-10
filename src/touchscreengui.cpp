/*
Copyright (C) 2014 sapier

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
#include "util/numeric.h"
#include "porting.h"
#include "guiscalingfilter.h"

#include <iostream>
#include <algorithm>

#include <ISceneCollisionManager.h>

// Very slow button repeat frequency (in seconds)
#define SLOW_BUTTON_REPEAT	(1.0f)

using namespace irr::core;

extern Settings *g_settings;

const char* touchgui_button_imagenames[] = {
	"up_arrow.png",
	"down_arrow.png",
	"left_arrow.png",
	"right_arrow.png",
	"jump_btn.png",
	"down.png"
};

static irr::EKEY_CODE id2keycode(touch_gui_button_id id)
{
	std::string key = "";
	switch (id) {
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
		case inventory_id:
			key = "inventory";
			break;
		case drop_id:
			key = "drop";
			break;
		case jump_id:
			key = "jump";
			break;
		case crunch_id:
			key = "sneak";
			break;
		case fly_id:
			key = "freemove";
			break;
		case noclip_id:
			key = "noclip";
			break;
		case fast_id:
			key = "fastmove";
			break;
		case debug_id:
			key = "toggle_debug";
			break;
		case chat_id:
			key = "chat";
			break;
		case camera_id:
			key = "camera_mode";
			break;
		case range_id:
			key = "rangeselect";
			break;
	}
	if(!key.size())
		return irr::EKEY_CODE();
	return keyname_to_keycode(g_settings->get("keymap_" + key).c_str());
}

TouchScreenGUI *g_touchscreengui = nullptr;

static void load_button_texture(button_info* btn, const char* path,
		rect<s32> button_rect, ISimpleTextureSource* tsrc, video::IVideoDriver *driver)
{
	unsigned int tid;
	video::ITexture *texture = guiScalingImageButton(driver,
			tsrc->getTexture(path, &tid), button_rect.getWidth(),
			button_rect.getHeight());
	if (texture) {
		btn->guibutton->setUseAlphaChannel(true);
		if (g_settings->getBool("gui_scaling_filter")) {
			rect<s32> txr_rect = rect<s32>(0, 0, button_rect.getWidth(), button_rect.getHeight());
			btn->guibutton->setImage(texture, txr_rect);
			btn->guibutton->setPressedImage(texture, txr_rect);
			btn->guibutton->setScaleImage(false);
		} else {
			btn->guibutton->setImage(texture);
			btn->guibutton->setPressedImage(texture);
			btn->guibutton->setScaleImage(true);
		}
		btn->guibutton->setDrawBorder(false);
		btn->guibutton->setText(L"");
		}
}

AutoHideButtonBar::AutoHideButtonBar(IrrlichtDevice *device,
		IEventReceiver* receiver) :
			m_texturesource(NULL),
			m_driver(device->getVideoDriver()),
			m_guienv(device->getGUIEnvironment()),
			m_receiver(receiver),
			m_active(false),
			m_visible(true),
			m_timeout(0),
			m_timeout_value(3),
			m_initialized(false),
			m_dir(AHBB_Dir_Right_Left)
{
	m_screensize = device->getVideoDriver()->getScreenSize();

}

void AutoHideButtonBar::init(ISimpleTextureSource* tsrc,
		const char* starter_img, int button_id, v2s32 UpperLeft,
		v2s32 LowerRight, autohide_button_bar_dir dir, float timeout)
{
	m_texturesource = tsrc;

	m_upper_left = UpperLeft;
	m_lower_right = LowerRight;

	/* init settings bar */
	clear();

	irr::core::rect<int> current_button = rect<s32>(UpperLeft.X, UpperLeft.Y,
			LowerRight.X, LowerRight.Y);

	m_starter.guibutton         = m_guienv->addButton(current_button, 0, button_id, L"", 0);
	m_starter.guibutton->grab();
	m_starter.repeatcounter     = -1;
	m_starter.keycode           = KEY_OEM_8; // use invalid keycode as it's not relevant
	m_starter.immediate_release = true;
	m_starter.ids.clear();

	load_button_texture(&m_starter, starter_img, current_button,
			m_texturesource, m_driver);

	m_dir = dir;
	m_timeout_value = timeout;

	m_initialized = true;
}

void AutoHideButtonBar::clear()
{
	if (m_starter.guibutton) {
	m_starter.guibutton->setVisible(false);
	m_starter.guibutton->drop();
	}

	for (auto i : m_buttons) {
		i->guibutton->drop();
		delete i;
	}
	m_buttons.clear();

}

AutoHideButtonBar::~AutoHideButtonBar()
{
	clear();
}

void AutoHideButtonBar::addButton(touch_gui_button_id button_id,
		const wchar_t* caption, const char* btn_image)
{

	if (!m_initialized) {
		errorstream << "AutoHideButtonBar::addButton not yet initialized!"
				<< std::endl;
		return;
	}
	int button_size = 0;

	if ((m_dir == AHBB_Dir_Top_Bottom) || (m_dir == AHBB_Dir_Bottom_Top)) {
		button_size = m_lower_right.X - m_upper_left.X;
	} else {
		button_size = m_lower_right.Y - m_upper_left.Y;
	}

	irr::core::rect<int> current_button;

	if ((m_dir == AHBB_Dir_Right_Left) || (m_dir == AHBB_Dir_Left_Right)) {

		int x_start = 0;
		int x_end = 0;

		if (m_dir == AHBB_Dir_Left_Right) {
			x_start = m_lower_right.X + (button_size * 1.25 * m_buttons.size())
					+ (button_size * 0.25);
			x_end = x_start + button_size;
		} else {
			x_end = m_upper_left.X - (button_size * 1.25 * m_buttons.size())
					- (button_size * 0.25);
			x_start = x_end - button_size;
		}

		current_button = rect<s32>(x_start, m_upper_left.Y, x_end,
				m_lower_right.Y);
	} else {
		int y_start = 0;
		int y_end = 0;

		if (m_dir == AHBB_Dir_Top_Bottom) {
			y_start = m_lower_right.X + (button_size * 1.25 * m_buttons.size())
					+ (button_size * 0.25);
			y_end = y_start + button_size;
		} else {
			y_end = m_upper_left.X - (button_size * 1.25 * m_buttons.size())
					- (button_size * 0.25);
			y_start = y_end - button_size;
		}

		current_button = rect<s32>(m_upper_left.X, y_start, m_lower_right.Y,
				y_end);
	}

	button_info* btn       = new button_info();
	btn->guibutton         = m_guienv->addButton(current_button, 0, button_id, caption, 0);
	btn->guibutton->grab();
	btn->guibutton->setVisible(false);
	btn->guibutton->setEnabled(false);
	btn->repeatcounter     = -1;
	btn->keycode           = id2keycode(button_id);
	btn->immediate_release = true;
	btn->ids.clear();

	load_button_texture(btn, btn_image, current_button, m_texturesource,
			m_driver);

	m_buttons.push_back(btn);
}

bool AutoHideButtonBar::isButton(const SEvent &event)
{
	IGUIElement* rootguielement = m_guienv->getRootGUIElement();

	if (rootguielement == NULL) {
		return false;
	}

	gui::IGUIElement *element = rootguielement->getElementFromPoint(
			core::position2d<s32>(event.TouchInput.X, event.TouchInput.Y));

	if (element == NULL) {
		return false;
	}

	if (m_active) {
		/* check for all buttons in vector */

		std::vector<button_info*>::iterator iter = m_buttons.begin();

		while (iter != m_buttons.end()) {
			if ((*iter)->guibutton == element) {

				SEvent* translated = new SEvent();
				memset(translated, 0, sizeof(SEvent));
				translated->EventType            = irr::EET_KEY_INPUT_EVENT;
				translated->KeyInput.Key         = (*iter)->keycode;
				translated->KeyInput.Control     = false;
				translated->KeyInput.Shift       = false;
				translated->KeyInput.Char        = 0;

				/* add this event */
				translated->KeyInput.PressedDown = true;
				m_receiver->OnEvent(*translated);

				/* remove this event */
				translated->KeyInput.PressedDown = false;
				m_receiver->OnEvent(*translated);

				delete translated;

				(*iter)->ids.push_back(event.TouchInput.ID);

				m_timeout = 0;

				return true;
			}
			++iter;
		}
	} else {
		/* check for starter button only */
		if (element == m_starter.guibutton) {
			m_starter.ids.push_back(event.TouchInput.ID);
			m_starter.guibutton->setVisible(false);
			m_starter.guibutton->setEnabled(false);
			m_active = true;
			m_timeout = 0;

			std::vector<button_info*>::iterator iter = m_buttons.begin();

			while (iter != m_buttons.end()) {
				(*iter)->guibutton->setVisible(true);
				(*iter)->guibutton->setEnabled(true);
				++iter;
			}

			return true;
		}
	}
	return false;
}

bool AutoHideButtonBar::isReleaseButton(int eventID)
{
	std::vector<int>::iterator id = std::find(m_starter.ids.begin(),
			m_starter.ids.end(), eventID);

	if (id != m_starter.ids.end()) {
		m_starter.ids.erase(id);
		return true;
	}

	std::vector<button_info*>::iterator iter = m_buttons.begin();

	while (iter != m_buttons.end()) {
		std::vector<int>::iterator id = std::find((*iter)->ids.begin(),
				(*iter)->ids.end(), eventID);

		if (id != (*iter)->ids.end()) {
			(*iter)->ids.erase(id);
			// TODO handle settings button release
			return true;
		}
		++iter;
	}

	return false;
}

void AutoHideButtonBar::step(float dtime)
{
	if (m_active) {
		m_timeout += dtime;

		if (m_timeout > m_timeout_value) {
			deactivate();
		}
	}
}

void AutoHideButtonBar::deactivate()
{
	if (m_visible == true) {
		m_starter.guibutton->setVisible(true);
		m_starter.guibutton->setEnabled(true);
	}
	m_active = false;

	std::vector<button_info*>::iterator iter = m_buttons.begin();

	while (iter != m_buttons.end()) {
			(*iter)->guibutton->setVisible(false);
			(*iter)->guibutton->setEnabled(false);
		++iter;
	}
}

void AutoHideButtonBar::hide()
{
	m_visible = false;
	m_starter.guibutton->setVisible(false);
	m_starter.guibutton->setEnabled(false);

	std::vector<button_info*>::iterator iter = m_buttons.begin();

	while (iter != m_buttons.end()) {
		(*iter)->guibutton->setVisible(false);
		(*iter)->guibutton->setEnabled(false);
		++iter;
	}
}

void AutoHideButtonBar::show()
{
	m_visible = true;

	if (m_active) {
		std::vector<button_info*>::iterator iter = m_buttons.begin();

		while (iter != m_buttons.end()) {
			(*iter)->guibutton->setVisible(true);
			(*iter)->guibutton->setEnabled(true);
			++iter;
		}
	} else {
		m_starter.guibutton->setVisible(true);
		m_starter.guibutton->setEnabled(true);
	}
}

TouchScreenGUI::TouchScreenGUI(IrrlichtDevice *device, IEventReceiver* receiver):
	m_device(device),
	m_guienv(device->getGUIEnvironment()),
	m_camera_yaw(0.0),
	m_camera_pitch(0.0),
	m_visible(false),
	m_move_id(-1),
	m_receiver(receiver),
	m_move_has_really_moved(false),
	m_move_downtime(0),
	m_move_sent_as_mouse_event(false),
	// use some downlocation way off screen as init value  to avoid invalid behaviour
	m_move_downlocation(v2s32(-10000, -10000)),
	m_settingsbar(device, receiver),
	m_rarecontrolsbar(device, receiver)
{
	for (unsigned int i=0; i < after_last_element_id; i++) {
		m_buttons[i].guibutton     =  0;
		m_buttons[i].repeatcounter = -1;
		m_buttons[i].repeatdelay   = BUTTON_REPEAT_DELAY;
	}

	m_screensize = m_device->getVideoDriver()->getScreenSize();
}

void TouchScreenGUI::initButton(touch_gui_button_id id, rect<s32> button_rect,
		std::wstring caption, bool immediate_release, float repeat_delay)
{

	button_info* btn       = &m_buttons[id];
	btn->guibutton         = m_guienv->addButton(button_rect, 0, id, caption.c_str());
	btn->guibutton->grab();
	btn->repeatcounter     = -1;
	btn->repeatdelay       = repeat_delay;
	btn->keycode           = id2keycode(id);
	btn->immediate_release = immediate_release;
	btn->ids.clear();

	load_button_texture(btn,touchgui_button_imagenames[id],button_rect,
			m_texturesource, m_device->getVideoDriver());
}

static int getMaxControlPadSize(float density) {
	return 200 * density * g_settings->getFloat("hud_scaling");
}

int TouchScreenGUI::getGuiButtonSize()
{
	u32 control_pad_size = MYMIN((2 * m_screensize.Y) / 3,
			getMaxControlPadSize(porting::getDisplayDensity()));

	return control_pad_size / 3;
}

void TouchScreenGUI::init(ISimpleTextureSource* tsrc)
{
	if (!tsrc)
		return;

	u32 button_size      = getGuiButtonSize();
	m_visible            = true;
	m_texturesource      = tsrc;
	m_control_pad_rect   = rect<s32>(0, m_screensize.Y - 3 * button_size,
			3 * button_size, m_screensize.Y);
	/*
	draw control pad
	0 1 2
	3 4 5
	for now only 0, 1, 2, and 4 are used
	*/
	int number = 0;
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 3; ++x, ++number) {
			rect<s32> button_rect(
					x * button_size, m_screensize.Y - button_size * (2 - y),
					(x + 1) * button_size, m_screensize.Y - button_size * (1 - y)
			);
			touch_gui_button_id id = after_last_element_id;
			std::wstring caption;
			switch (number) {
			case 0:
				id = left_id;
				caption = L"<";
				break;
			case 1:
				id = forward_id;
				caption = L"^";
				break;
			case 2:
				id = right_id;
				caption = L">";
				break;
			case 4:
				id = backward_id;
				caption = L"v";
				break;
			}
			if (id != after_last_element_id) {
				initButton(id, button_rect, caption, false);
				}
		}

	/* init jump button */
	initButton(jump_id,
			rect<s32>(m_screensize.X-(1.75*button_size),
					m_screensize.Y - (0.5*button_size),
					m_screensize.X-(0.25*button_size),
					m_screensize.Y),
			L"x",false);

	/* init crunch button */
	initButton(crunch_id,
			rect<s32>(m_screensize.X-(3.25*button_size),
					m_screensize.Y - (0.5*button_size),
					m_screensize.X-(1.75*button_size),
					m_screensize.Y),
			L"H",false);

	m_settingsbar.init(m_texturesource, "gear_icon.png", settings_starter_id,
			v2s32(m_screensize.X - (button_size / 2),
					m_screensize.Y - ((SETTINGS_BAR_Y_OFFSET + 1) * button_size)
							+ (button_size * 0.5)),
			v2s32(m_screensize.X,
					m_screensize.Y - (SETTINGS_BAR_Y_OFFSET * button_size)
							+ (button_size * 0.5)), AHBB_Dir_Right_Left,
			3.0);

	m_settingsbar.addButton(fly_id,    L"fly",       "fly_btn.png");
	m_settingsbar.addButton(noclip_id, L"noclip",    "noclip_btn.png");
	m_settingsbar.addButton(fast_id,   L"fast",      "fast_btn.png");
	m_settingsbar.addButton(debug_id,  L"debug",     "debug_btn.png");
	m_settingsbar.addButton(camera_id, L"camera",    "camera_btn.png");
	m_settingsbar.addButton(range_id,  L"rangeview", "rangeview_btn.png");

	m_rarecontrolsbar.init(m_texturesource, "rare_controls.png",
			rare_controls_starter_id,
			v2s32(0,
					m_screensize.Y
							- ((RARE_CONTROLS_BAR_Y_OFFSET + 1) * button_size)
							+ (button_size * 0.5)),
			v2s32(button_size / 2,
					m_screensize.Y - (RARE_CONTROLS_BAR_Y_OFFSET * button_size)
							+ (button_size * 0.5)), AHBB_Dir_Left_Right,
			2);

	m_rarecontrolsbar.addButton(chat_id,      L"Chat", "chat_btn.png");
	m_rarecontrolsbar.addButton(inventory_id, L"inv",  "inventory_btn.png");
	m_rarecontrolsbar.addButton(drop_id,      L"drop", "drop_btn.png");

}

touch_gui_button_id TouchScreenGUI::getButtonID(s32 x, s32 y)
{
	IGUIElement* rootguielement = m_guienv->getRootGUIElement();

	if (rootguielement != NULL) {
		gui::IGUIElement *element =
				rootguielement->getElementFromPoint(core::position2d<s32>(x,y));

		if (element) {
			for (unsigned int i=0; i < after_last_element_id; i++) {
				if (element == m_buttons[i].guibutton) {
					return (touch_gui_button_id) i;
				}
			}
		}
	}
	return after_last_element_id;
}

touch_gui_button_id TouchScreenGUI::getButtonID(int eventID)
{
	for (unsigned int i=0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];

		std::vector<int>::iterator id =
				std::find(btn->ids.begin(),btn->ids.end(), eventID);

		if (id != btn->ids.end())
			return (touch_gui_button_id) i;
	}

	return after_last_element_id;
}

bool TouchScreenGUI::isHUDButton(const SEvent &event)
{
	// check if hud item is pressed
	for (std::map<int,rect<s32> >::iterator iter = m_hud_rects.begin();
			iter != m_hud_rects.end(); ++iter) {
		if (iter->second.isPointInside(
				v2s32(event.TouchInput.X,
						event.TouchInput.Y)
			)) {
			if ( iter->first < 8) {
				SEvent* translated = new SEvent();
				memset(translated,0,sizeof(SEvent));
				translated->EventType = irr::EET_KEY_INPUT_EVENT;
				translated->KeyInput.Key         = (irr::EKEY_CODE) (KEY_KEY_1 + iter->first);
				translated->KeyInput.Control     = false;
				translated->KeyInput.Shift       = false;
				translated->KeyInput.PressedDown = true;
				m_receiver->OnEvent(*translated);
				m_hud_ids[event.TouchInput.ID]   = translated->KeyInput.Key;
				delete translated;
				return true;
			}
		}
	}
	return false;
}

bool TouchScreenGUI::isReleaseHUDButton(int eventID)
{
	std::map<int,irr::EKEY_CODE>::iterator iter = m_hud_ids.find(eventID);

	if (iter != m_hud_ids.end()) {
		SEvent* translated = new SEvent();
		memset(translated,0,sizeof(SEvent));
		translated->EventType            = irr::EET_KEY_INPUT_EVENT;
		translated->KeyInput.Key         = iter->second;
		translated->KeyInput.PressedDown = false;
		translated->KeyInput.Control     = false;
		translated->KeyInput.Shift       = false;
		m_receiver->OnEvent(*translated);
		m_hud_ids.erase(iter);
		delete translated;
		return true;
	}
	return false;
}

void TouchScreenGUI::handleButtonEvent(touch_gui_button_id button,
		int eventID, bool action)
{
	button_info* btn = &m_buttons[button];
	SEvent* translated = new SEvent();
	memset(translated,0,sizeof(SEvent));
	translated->EventType            = irr::EET_KEY_INPUT_EVENT;
	translated->KeyInput.Key         = btn->keycode;
	translated->KeyInput.Control     = false;
	translated->KeyInput.Shift       = false;
	translated->KeyInput.Char        = 0;

	/* add this event */
	if (action) {
		if(!(std::find(btn->ids.begin(),btn->ids.end(), eventID) == btn->ids.end()))
			return;

		btn->ids.push_back(eventID);

		if (btn->ids.size() > 1) return;

		btn->repeatcounter = 0;
		translated->KeyInput.PressedDown = true;
		translated->KeyInput.Key = btn->keycode;
		m_receiver->OnEvent(*translated);
	}
	/* remove event */
	if ((!action) || (btn->immediate_release)) {

		std::vector<int>::iterator pos =
				std::find(btn->ids.begin(),btn->ids.end(), eventID);
		/* has to be in touch list */
		if(!(pos != btn->ids.end()))
			return;
		btn->ids.erase(pos);

		if (btn->ids.size() > 0)  { return; }

		translated->KeyInput.PressedDown = false;
		btn->repeatcounter               = -1;
		m_receiver->OnEvent(*translated);
	}
	delete translated;
}


void TouchScreenGUI::handleReleaseEvent(int evt_id)
{
	touch_gui_button_id button = getButtonID(evt_id);

	/* handle button events */
	if (button != after_last_element_id) {
		handleButtonEvent(button, evt_id, false);
	}
	/* handle hud button events */
	else if (isReleaseHUDButton(evt_id)) {
		/* nothing to do here */
	} else if (m_settingsbar.isReleaseButton(evt_id)) {
		/* nothing to do here */
	} else if (m_rarecontrolsbar.isReleaseButton(evt_id)) {
		/* nothing to do here */
	}
	/* handle the point used for moving view */
	else if (evt_id == m_move_id) {
		m_move_id = -1;

		/* if this pointer issued a mouse event issue symmetric release here */
		if (m_move_sent_as_mouse_event) {
			SEvent* translated = new SEvent;
			memset(translated,0,sizeof(SEvent));
			translated->EventType               = EET_MOUSE_INPUT_EVENT;
			translated->MouseInput.X            = m_move_downlocation.X;
			translated->MouseInput.Y            = m_move_downlocation.Y;
			translated->MouseInput.Shift        = false;
			translated->MouseInput.Control      = false;
			translated->MouseInput.ButtonStates = 0;
			translated->MouseInput.Event        = EMIE_LMOUSE_LEFT_UP;
			m_receiver->OnEvent(*translated);
			delete translated;
		}
		else {
			/* do double tap detection */
			doubleTapDetection();
		}
	}
	else {
		infostream
			<< "TouchScreenGUI::translateEvent released unknown button: "
			<< evt_id << std::endl;
	}

	for (std::vector<id_status>::iterator iter = m_known_ids.begin();
			iter != m_known_ids.end(); ++iter) {
		if (iter->id == evt_id) {
			m_known_ids.erase(iter);
			break;
		}
	}
}

void TouchScreenGUI::translateEvent(const SEvent &event)
{
	if (!m_visible) {
		infostream << "TouchScreenGUI::translateEvent got event but not visible?!" << std::endl;
		return;
	}

	if (event.EventType != EET_TOUCH_INPUT_EVENT) {
		return;
	}

	if (event.TouchInput.Event == ETIE_PRESSED_DOWN) {

		/* add to own copy of eventlist ...
		 * android would provide this information but irrlicht guys don't
		 * wanna design a efficient interface
		 */
		id_status toadd;
		toadd.id = event.TouchInput.ID;
		toadd.X  = event.TouchInput.X;
		toadd.Y  = event.TouchInput.Y;
		m_known_ids.push_back(toadd);

		int eventID = event.TouchInput.ID;

		touch_gui_button_id button =
				getButtonID(event.TouchInput.X, event.TouchInput.Y);

		/* handle button events */
		if (button != after_last_element_id) {
			handleButtonEvent(button, eventID, true);
			m_settingsbar.deactivate();
			m_rarecontrolsbar.deactivate();
		} else if (isHUDButton(event)) {
			m_settingsbar.deactivate();
			m_rarecontrolsbar.deactivate();
			/* already handled in isHUDButton() */
		} else if (m_settingsbar.isButton(event)) {
			m_rarecontrolsbar.deactivate();
			/* already handled in isSettingsBarButton() */
		} else if (m_rarecontrolsbar.isButton(event)) {
			m_settingsbar.deactivate();
			/* already handled in isSettingsBarButton() */
		}
		/* handle non button events */
		else {
			m_settingsbar.deactivate();
			m_rarecontrolsbar.deactivate();
			/* if we don't already have a moving point make this the moving one */
			if (m_move_id == -1) {
				m_move_id                  = event.TouchInput.ID;
				m_move_has_really_moved    = false;
				m_move_downtime            = getTimeMs();
				m_move_downlocation        = v2s32(event.TouchInput.X, event.TouchInput.Y);
				m_move_sent_as_mouse_event = false;
			}
		}

		m_pointerpos[event.TouchInput.ID] = v2s32(event.TouchInput.X, event.TouchInput.Y);
	}
	else if (event.TouchInput.Event == ETIE_LEFT_UP) {
		verbosestream << "Up event for pointerid: " << event.TouchInput.ID << std::endl;
		handleReleaseEvent(event.TouchInput.ID);
	}
	else {
		if(!(event.TouchInput.Event == ETIE_MOVED))
			return;
		int move_idx = event.TouchInput.ID;

		if (m_pointerpos[event.TouchInput.ID] ==
				v2s32(event.TouchInput.X, event.TouchInput.Y)) {
			return;
		}

		if (m_move_id != -1) {
			if ((event.TouchInput.ID == m_move_id) &&
				(!m_move_sent_as_mouse_event)) {

				double distance = sqrt(
						(m_pointerpos[event.TouchInput.ID].X - event.TouchInput.X) *
						(m_pointerpos[event.TouchInput.ID].X - event.TouchInput.X) +
						(m_pointerpos[event.TouchInput.ID].Y - event.TouchInput.Y) *
						(m_pointerpos[event.TouchInput.ID].Y - event.TouchInput.Y));

				if ((distance > g_settings->getU16("touchscreen_threshold")) ||
						(m_move_has_really_moved)) {
					m_move_has_really_moved = true;
					s32 X = event.TouchInput.X;
					s32 Y = event.TouchInput.Y;

					// update camera_yaw and camera_pitch
					s32 dx = X - m_pointerpos[event.TouchInput.ID].X;
					s32 dy = Y - m_pointerpos[event.TouchInput.ID].Y;

					/* adapt to similar behaviour as pc screen */
					double d         = g_settings->getFloat("mouse_sensitivity") *4;
					double old_yaw   = m_camera_yaw;
					double old_pitch = m_camera_pitch;

					m_camera_yaw   -= dx * d;
					m_camera_pitch  = MYMIN(MYMAX( m_camera_pitch + (dy * d),-180),180);

					while (m_camera_yaw < 0)
						m_camera_yaw += 360;

					while (m_camera_yaw > 360)
						m_camera_yaw -= 360;

					// update shootline
					m_shootline = m_device
							->getSceneManager()
							->getSceneCollisionManager()
							->getRayFromScreenCoordinates(v2s32(X, Y));
					m_pointerpos[event.TouchInput.ID] = v2s32(X, Y);
				}
			}
			else if ((event.TouchInput.ID == m_move_id) &&
					(m_move_sent_as_mouse_event)) {
				m_shootline = m_device
						->getSceneManager()
						->getSceneCollisionManager()
						->getRayFromScreenCoordinates(
								v2s32(event.TouchInput.X,event.TouchInput.Y));
			}
		} else {
			handleChangedButton(event);
		}
	}
}

void TouchScreenGUI::handleChangedButton(const SEvent &event)
{
	for (unsigned int i = 0; i < after_last_element_id; i++) {

		if (m_buttons[i].ids.empty()) {
			continue;
		}
		for (std::vector<int>::iterator iter = m_buttons[i].ids.begin();
				iter != m_buttons[i].ids.end(); ++iter) {

			if (event.TouchInput.ID == *iter) {

				int current_button_id =
						getButtonID(event.TouchInput.X, event.TouchInput.Y);

				if (current_button_id == i) {
					continue;
				}

				/* remove old button */
				handleButtonEvent((touch_gui_button_id) i,*iter,false);

				if (current_button_id == after_last_element_id) {
					return;
				}
				handleButtonEvent((touch_gui_button_id) current_button_id,*iter,true);
				return;

			}
		}
	}

	int current_button_id = getButtonID(event.TouchInput.X, event.TouchInput.Y);

	if (current_button_id == after_last_element_id) {
		return;
	}

	button_info* btn = &m_buttons[current_button_id];
	if (std::find(btn->ids.begin(),btn->ids.end(), event.TouchInput.ID)
			== btn->ids.end())
	{
		handleButtonEvent((touch_gui_button_id) current_button_id,
				event.TouchInput.ID, true);
	}

}

bool TouchScreenGUI::doubleTapDetection()
{
	m_key_events[0].down_time = m_key_events[1].down_time;
	m_key_events[0].x         = m_key_events[1].x;
	m_key_events[0].y         = m_key_events[1].y;
	m_key_events[1].down_time = m_move_downtime;
	m_key_events[1].x         = m_move_downlocation.X;
	m_key_events[1].y         = m_move_downlocation.Y;

	u32 delta = porting::getDeltaMs(m_key_events[0].down_time, getTimeMs());
	if (delta > 400)
		return false;

	double distance = sqrt(
			(m_key_events[0].x - m_key_events[1].x) * (m_key_events[0].x - m_key_events[1].x) +
			(m_key_events[0].y - m_key_events[1].y) * (m_key_events[0].y - m_key_events[1].y));


	if (distance > (20 + g_settings->getU16("touchscreen_threshold")))
		return false;

	SEvent* translated = new SEvent();
	memset(translated, 0, sizeof(SEvent));
	translated->EventType               = EET_MOUSE_INPUT_EVENT;
	translated->MouseInput.X            = m_key_events[0].x;
	translated->MouseInput.Y            = m_key_events[0].y;
	translated->MouseInput.Shift        = false;
	translated->MouseInput.Control      = false;
	translated->MouseInput.ButtonStates = EMBSM_RIGHT;

	// update shootline
	m_shootline = m_device
			->getSceneManager()
			->getSceneCollisionManager()
			->getRayFromScreenCoordinates(v2s32(m_key_events[0].x, m_key_events[0].y));

	translated->MouseInput.Event = EMIE_RMOUSE_PRESSED_DOWN;
	verbosestream << "TouchScreenGUI::translateEvent right click press" << std::endl;
	m_receiver->OnEvent(*translated);

	translated->MouseInput.ButtonStates = 0;
	translated->MouseInput.Event        = EMIE_RMOUSE_LEFT_UP;
	verbosestream << "TouchScreenGUI::translateEvent right click release" << std::endl;
	m_receiver->OnEvent(*translated);
	delete translated;
	return true;

}

TouchScreenGUI::~TouchScreenGUI()
{
	for (unsigned int i = 0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];
		if (btn->guibutton != 0) {
			btn->guibutton->drop();
			btn->guibutton = NULL;
		}
	}
}

void TouchScreenGUI::step(float dtime)
{
	/* simulate keyboard repeats */
	for (unsigned int i = 0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];

		if (btn->ids.size() > 0) {
			btn->repeatcounter += dtime;

			/* in case we're moving around digging does not happen */
			if (m_move_id != -1)
				m_move_has_really_moved = true;

			if (btn->repeatcounter < btn->repeatdelay) continue;

			btn->repeatcounter              = 0;
			SEvent translated;
			memset(&translated, 0, sizeof(SEvent));
			translated.EventType            = irr::EET_KEY_INPUT_EVENT;
			translated.KeyInput.Key         = btn->keycode;
			translated.KeyInput.PressedDown = false;
			m_receiver->OnEvent(translated);

			translated.KeyInput.PressedDown = true;
			m_receiver->OnEvent(translated);
		}
	}

	/* if a new placed pointer isn't moved for some time start digging */
	if ((m_move_id != -1) &&
			(!m_move_has_really_moved) &&
			(!m_move_sent_as_mouse_event)) {

		u32 delta = porting::getDeltaMs(m_move_downtime,getTimeMs());

		if (delta > MIN_DIG_TIME_MS) {
			m_shootline = m_device
					->getSceneManager()
					->getSceneCollisionManager()
					->getRayFromScreenCoordinates(
							v2s32(m_move_downlocation.X,m_move_downlocation.Y));

			SEvent translated;
			memset(&translated, 0, sizeof(SEvent));
			translated.EventType               = EET_MOUSE_INPUT_EVENT;
			translated.MouseInput.X            = m_move_downlocation.X;
			translated.MouseInput.Y            = m_move_downlocation.Y;
			translated.MouseInput.Shift        = false;
			translated.MouseInput.Control      = false;
			translated.MouseInput.ButtonStates = EMBSM_LEFT;
			translated.MouseInput.Event        = EMIE_LMOUSE_PRESSED_DOWN;
			verbosestream << "TouchScreenGUI::step left click press" << std::endl;
			m_receiver->OnEvent(translated);
			m_move_sent_as_mouse_event         = true;
		}
	}

	m_settingsbar.step(dtime);
	m_rarecontrolsbar.step(dtime);
}

void TouchScreenGUI::resetHud()
{
	m_hud_rects.clear();
}

void TouchScreenGUI::registerHudItem(int index, const rect<s32> &rect)
{
	m_hud_rects[index] = rect;
}

void TouchScreenGUI::Toggle(bool visible)
{
	m_visible = visible;
	for (unsigned int i = 0; i < after_last_element_id; i++) {
		button_info* btn = &m_buttons[i];
		if (btn->guibutton != 0) {
			btn->guibutton->setVisible(visible);
		}
	}

	/* clear all active buttons */
	if (!visible) {
		while (m_known_ids.size() > 0) {
			handleReleaseEvent(m_known_ids.begin()->id);
		}

		m_settingsbar.hide();
		m_rarecontrolsbar.hide();
	} else {
		m_settingsbar.show();
		m_rarecontrolsbar.show();
	}
}

void TouchScreenGUI::hide()
{
	if (!m_visible)
		return;

	Toggle(false);
}

void TouchScreenGUI::show()
{
	if (m_visible)
		return;

	Toggle(true);
}
