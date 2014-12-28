/*
game.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#include "game.h"
#include "irrlichttypes_extrabloated.h"
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIListBox.h>
#include <IGUIButton.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>
#include <IMaterialRendererServices.h>
#include "IMeshCache.h"
#include "client.h"
#include "server.h"
#include "guiPasswordChange.h"
#include "guiVolumeChange.h"
#include "guiKeyChangeMenu.h"
#include "guiFormSpecMenu.h"
#include "guiTextInputMenu.h"
#include "tool.h"
#include "guiChatConsole.h"
#include "config.h"
#include "version.h"
#include "clouds.h"
#include "particles.h"
#include "camera.h"
#include "mapblock.h"
#include "settings.h"
#include "profiler.h"
#include "mainmenumanager.h"
#include "gettext.h"
#include "log.h"
#include "filesys.h"
// Needed for determining pointing to nodes
#include "nodedef.h"
#include "nodemetadata.h"
#include "main.h" // For g_settings
#include "itemdef.h"
#include "tile.h" // For TextureSource
#include "shader.h" // For ShaderSource
#include "logoutputbuffer.h"
#include "subgame.h"
#include "quicktune_shortcutter.h"
#include "clientmap.h"
#include "hud.h"
#include "sky.h"
#include "sound.h"
#if USE_SOUND
#include "sound_openal.h"
#endif
#include "event_manager.h"
#include <iomanip>
#include <list>
#include "util/directiontables.h"
#include "util/pointedthing.h"
#include "FMStaticText.h"
#include "guiTable.h"
#include "util/string.h"
#include "drawscene.h"
#include "content_cao.h"
#include "fontengine.h"

#ifdef HAVE_TOUCHSCREENGUI
#include "touchscreengui.h"
#endif

#include "gsmapper.h"
#include <future>

/*
	Text input system
*/

struct TextDestNodeMetadata : public TextDest {
	TextDestNodeMetadata(v3s16 p, Client *client)
	{
		m_p = p;
		m_client = client;
	}
	// This is deprecated I guess? -celeron55
	void gotText(std::wstring text)
	{
		assert(0);
	}
	void gotText(std::map<std::string, std::string> fields)
	{
		m_client->sendNodemetaFields(m_p, "", fields);
	}

	v3s16 m_p;
	Client *m_client;
};

struct TextDestPlayerInventory : public TextDest {
	TextDestPlayerInventory(Client *client)
	{
		m_client = client;
		m_formname = "";
	}
	TextDestPlayerInventory(Client *client, std::string formname)
	{
		m_client = client;
		m_formname = formname;
	}
	void gotText(std::map<std::string, std::string> fields)
	{
		m_client->sendInventoryFields(m_formname, fields);
	}

	Client *m_client;
};

struct LocalFormspecHandler : public TextDest {
	LocalFormspecHandler();
	LocalFormspecHandler(std::string formname) :
		m_client(0)
	{
		m_formname = formname;
	}

	LocalFormspecHandler(std::string formname, Client *client) :
		m_client(client)
	{
		m_formname = formname;
	}

	void gotText(std::wstring message)
	{
		errorstream << "LocalFormspecHandler::gotText old style message received" << std::endl;
	}

	void gotText(std::map<std::string, std::string> fields)
	{
		if (m_formname == "MT_PAUSE_MENU") {
			if (fields.find("btn_sound") != fields.end()) {
				g_gamecallback->changeVolume();
				return;
			}

			if (fields.find("btn_key_config") != fields.end()) {
				g_gamecallback->keyConfig();
				return;
			}

			if (fields.find("btn_exit_menu") != fields.end()) {
				g_gamecallback->disconnect();
				return;
			}

			if (fields.find("btn_exit_os") != fields.end()) {
				g_gamecallback->exitToOS();
				return;
			}

			if (fields.find("btn_change_password") != fields.end()) {
				g_gamecallback->changePassword();
				return;
			}

			if (fields.find("quit") != fields.end()) {
				return;
			}

			if (fields.find("btn_continue") != fields.end()) {
				return;
			}
		}

		if (m_formname == "MT_CHAT_MENU") {
			assert(m_client != 0);

			if ((fields.find("btn_send") != fields.end()) ||
					(fields.find("quit") != fields.end())) {
				if (fields.find("f_text") != fields.end()) {
					m_client->typeChatMessage(narrow_to_wide(fields["f_text"]));
				}

				return;
			}
		}

		if (m_formname == "MT_DEATH_SCREEN") {
			assert(m_client != 0);

			if ((fields.find("btn_respawn") != fields.end())) {
				m_client->sendRespawn();
				return;
			}

			if (fields.find("quit") != fields.end()) {
				m_client->sendRespawn();
				return;
			}
		}

		// don't show error message for unhandled cursor keys
		if ((fields.find("key_up") != fields.end()) ||
				(fields.find("key_down") != fields.end()) ||
				(fields.find("key_left") != fields.end()) ||
				(fields.find("key_right") != fields.end())) {
			return;
		}

		errorstream << "LocalFormspecHandler::gotText unhandled >" << m_formname << "< event" << std::endl;
		int i = 0;

		for (std::map<std::string, std::string>::iterator iter = fields.begin();
				iter != fields.end(); iter++) {
			errorstream << "\t" << i << ": " << iter->first << "=" << iter->second << std::endl;
			i++;
		}
	}

	Client *m_client;
};

/* Form update callback */

class NodeMetadataFormSource: public IFormSource
{
public:
	NodeMetadataFormSource(ClientMap *map, v3s16 p):
		m_map(map),
		m_p(p)
	{
	}
	std::string getForm()
	{
		NodeMetadata *meta = m_map->getNodeMetadata(m_p);

		if (!meta)
			return "";

		return meta->getString("formspec");
	}
	std::string resolveText(std::string str)
	{
		NodeMetadata *meta = m_map->getNodeMetadata(m_p);

		if (!meta)
			return str;

		return meta->resolveString(str);
	}

	ClientMap *m_map;
	v3s16 m_p;
};

class PlayerInventoryFormSource: public IFormSource
{
public:
	PlayerInventoryFormSource(Client *client):
		m_client(client)
	{
	}
	std::string getForm()
	{
		LocalPlayer *player = m_client->getEnv().getLocalPlayer();
		return player->inventory_formspec;
	}

	Client *m_client;
};

/*
	Check if a node is pointable
*/
inline bool isPointableNode(const MapNode &n,
			    Client *client, bool liquids_pointable)
{
	const ContentFeatures &features = client->getNodeDefManager()->get(n);
	return features.pointable ||
	       (liquids_pointable && features.isLiquid());
}

/*
	Find what the player is pointing at
*/
PointedThing getPointedThing(Client *client, v3f player_position,
		v3f camera_direction, v3f camera_position, core::line3d<f32> shootline,
		f32 d, bool liquids_pointable, bool look_for_object, v3s16 camera_offset,
		std::vector<aabb3f> &hilightboxes, ClientActiveObject *&selected_object)
{
	PointedThing result;

	hilightboxes.clear();
	selected_object = NULL;

	INodeDefManager *nodedef = client->getNodeDefManager();
	ClientMap &map = client->getEnv().getClientMap();

	f32 mindistance = BS * 1001;

	// First try to find a pointed at active object
	if (look_for_object) {
		selected_object = client->getSelectedActiveObject(d * BS,
				  camera_position, shootline);

		if (selected_object != NULL) {
			if (selected_object->doShowSelectionBox()) {
				aabb3f *selection_box = selected_object->getSelectionBox();
				// Box should exist because object was
				// returned in the first place
				assert(selection_box);

				v3f pos = selected_object->getPosition();
				hilightboxes.push_back(aabb3f(
							       selection_box->MinEdge + pos - intToFloat(camera_offset, BS),
							       selection_box->MaxEdge + pos - intToFloat(camera_offset, BS)));
			}

			mindistance = (selected_object->getPosition() - camera_position).getLength();

			result.type = POINTEDTHING_OBJECT;
			result.object_id = selected_object->getId();
		}
	}

	// That didn't work, try to find a pointed at node


	v3s16 pos_i = floatToInt(player_position, BS);

	/*infostream<<"pos_i=("<<pos_i.X<<","<<pos_i.Y<<","<<pos_i.Z<<")"
			<<std::endl;*/

	s16 a = d;
	s16 ystart = pos_i.Y + 0 - (camera_direction.Y < 0 ? a : 1);
	s16 zstart = pos_i.Z - (camera_direction.Z < 0 ? a : 1);
	s16 xstart = pos_i.X - (camera_direction.X < 0 ? a : 1);
	s16 yend = pos_i.Y + 1 + (camera_direction.Y > 0 ? a : 1);
	s16 zend = pos_i.Z + (camera_direction.Z > 0 ? a : 1);
	s16 xend = pos_i.X + (camera_direction.X > 0 ? a : 1);

	// Prevent signed number overflow
	if (yend == 32767)
		yend = 32766;

	if (zend == 32767)
		zend = 32766;

	if (xend == 32767)
		xend = 32766;

	for (s16 y = ystart; y <= yend; y++)
		for (s16 z = zstart; z <= zend; z++)
			for (s16 x = xstart; x <= xend; x++) {
				MapNode n;
				bool is_valid_position;

				n = map.getNodeNoEx(v3s16(x, y, z), &is_valid_position);
				if (!is_valid_position)
					continue;

				if (!isPointableNode(n, client, liquids_pointable))
					continue;

				std::vector<aabb3f> boxes = n.getSelectionBoxes(nodedef);

				v3s16 np(x, y, z);
				v3f npf = intToFloat(np, BS);

				for (std::vector<aabb3f>::const_iterator
						i = boxes.begin();
						i != boxes.end(); i++) {
					aabb3f box = *i;
					box.MinEdge += npf;
					box.MaxEdge += npf;

					for (u16 j = 0; j < 6; j++) {
						v3s16 facedir = g_6dirs[j];
						aabb3f facebox = box;

						f32 d = 0.001 * BS;

						if (facedir.X > 0)
							facebox.MinEdge.X = facebox.MaxEdge.X - d;
						else if (facedir.X < 0)
							facebox.MaxEdge.X = facebox.MinEdge.X + d;
						else if (facedir.Y > 0)
							facebox.MinEdge.Y = facebox.MaxEdge.Y - d;
						else if (facedir.Y < 0)
							facebox.MaxEdge.Y = facebox.MinEdge.Y + d;
						else if (facedir.Z > 0)
							facebox.MinEdge.Z = facebox.MaxEdge.Z - d;
						else if (facedir.Z < 0)
							facebox.MaxEdge.Z = facebox.MinEdge.Z + d;

						v3f centerpoint = facebox.getCenter();
						f32 distance = (centerpoint - camera_position).getLength();

						if (distance >= mindistance)
							continue;

						if (!facebox.intersectsWithLine(shootline))
							continue;

						v3s16 np_above = np + facedir;

						result.type = POINTEDTHING_NODE;
						result.node_undersurface = np;
						result.node_abovesurface = np_above;
						mindistance = distance;

						hilightboxes.clear();

						if (!g_settings->getBool("enable_node_highlighting")) {
							for (std::vector<aabb3f>::const_iterator
									i2 = boxes.begin();
									i2 != boxes.end(); i2++) {
								aabb3f box = *i2;
								box.MinEdge += npf + v3f(-d, -d, -d) - intToFloat(camera_offset, BS);
								box.MaxEdge += npf + v3f(d, d, d) - intToFloat(camera_offset, BS);
								hilightboxes.push_back(box);
							}
						}
					}
				}
			} // for coords

	return result;
}

/* Profiler display */

void update_profiler_gui(gui::IGUIStaticText *guitext_profiler, FontEngine *fe,
		u32 show_profiler, u32 show_profiler_max, s32 screen_height)
{
	if (show_profiler == 0) {
		guitext_profiler->setVisible(false);
	} else {

		std::ostringstream os(std::ios_base::binary);
		g_profiler->printPage(os, show_profiler, show_profiler_max);
		std::wstring text = utf8_to_wide(os.str());
		guitext_profiler->setText(text.c_str());
		guitext_profiler->setVisible(true);

		s32 w = fe->getTextWidth(text.c_str());

		if (w < 400)
			w = 400;

		unsigned text_height = fe->getTextHeight();

		core::position2di upper_left, lower_right;

		upper_left.X  = 6;
		upper_left.Y  = (text_height + 5) * 2;
		lower_right.X = 12 + w;
		lower_right.Y = upper_left.Y + (text_height + 1) * MAX_PROFILER_TEXT_ROWS;

		if (lower_right.Y > screen_height * 2 / 3)
			lower_right.Y = screen_height * 2 / 3;

		core::rect<s32> rect(upper_left, lower_right);

		guitext_profiler->setRelativePosition(rect);
		guitext_profiler->setVisible(true);
	}
}

class ProfilerGraph
{
private:
	struct Piece {
		Profiler::GraphValues values;
	};
	struct Meta {
		float cur;
		float min;
		float max;
		video::SColor color;
		Meta(float initial = 0,
			video::SColor color = video::SColor(255, 255, 255, 255)):
			cur(initial),
			min(initial),
			max(initial),
			color(color)
		{}
	};
	std::list<Piece> m_log;
public:
	u32 m_log_max_size;

	ProfilerGraph():
		m_log_max_size(200)
	{}

	void put(const Profiler::GraphValues &values)
	{
		Piece piece;
		piece.values = values;
		m_log.push_back(piece);

		while (m_log.size() > m_log_max_size)
			m_log.erase(m_log.begin());
	}

	void draw(s32 x_left, s32 y_bottom, video::IVideoDriver *driver,
		  gui::IGUIFont *font) const
	{
		std::map<std::string, Meta> m_meta;

		for (std::list<Piece>::const_iterator k = m_log.begin();
				k != m_log.end(); k++) {
			const Piece &piece = *k;

			for (Profiler::GraphValues::const_iterator i = piece.values.begin();
					i != piece.values.end(); i++) {
				const std::string &id = i->first;
				const float &value = i->second;
				std::map<std::string, Meta>::iterator j =
					m_meta.find(id);

				if (j == m_meta.end()) {
					m_meta[id] = Meta(value);
					continue;
				}

				if (value < j->second.min)
					j->second.min = value;

				if (value > j->second.max)
					j->second.max = value;
				j->second.cur = value;
			}
		}

		// Assign colors
		static const video::SColor usable_colors[] = {
			video::SColor(255, 255, 100, 100),
			video::SColor(255, 90, 225, 90),
			video::SColor(255, 100, 100, 255),
			video::SColor(255, 255, 150, 50),
			video::SColor(255, 220, 220, 100)
		};
		static const u32 usable_colors_count =
			sizeof(usable_colors) / sizeof(*usable_colors);
		u32 next_color_i = 0;

		for (std::map<std::string, Meta>::iterator i = m_meta.begin();
				i != m_meta.end(); i++) {
			Meta &meta = i->second;
			video::SColor color(255, 200, 200, 200);

			if (next_color_i < usable_colors_count)
				color = usable_colors[next_color_i++];

			meta.color = color;
		}

		s32 graphh = 50;
		s32 textx = x_left + m_log_max_size + 15;
		s32 textx2 = textx + 200 - 15;

		// Draw background
		/*{
			u32 num_graphs = m_meta.size();
			core::rect<s32> rect(x_left, y_bottom - num_graphs*graphh,
					textx2, y_bottom);
			video::SColor bgcolor(120,0,0,0);
			driver->draw2DRectangle(bgcolor, rect, NULL);
		}*/

		s32 meta_i = 0;

		for (std::map<std::string, Meta>::const_iterator i = m_meta.begin();
				i != m_meta.end(); i++) {
			const std::string &id = i->first;
			const Meta &meta = i->second;
			s32 x = x_left;
			s32 y = y_bottom - meta_i * 50;
			float show_min = meta.min;
			float show_max = meta.max;

			if (show_min >= -0.0001 && show_max >= -0.0001) {
				if (show_min <= show_max * 0.5)
					show_min = 0;
			}

			s32 texth = 15;
			char buf[10];
			snprintf(buf, 10, "%.3g", show_max);
			font->draw(utf8_to_wide(buf).c_str(),
					core::rect<s32>(textx, y - graphh,
						   textx2, y - graphh + texth),
					meta.color);
			snprintf(buf, 10, "%.3g", show_min);
			font->draw(utf8_to_wide(buf).c_str(),
					core::rect<s32>(textx, y - texth,
						   textx2, y),
					meta.color);
			font->draw(utf8_to_wide(id + " " + ftos(meta.cur)).c_str(),
					core::rect<s32>(textx, y - graphh / 2 - texth / 2,
						   textx2, y - graphh / 2 + texth / 2),
					meta.color);
			s32 graph1y = y;
			s32 graph1h = graphh;
			bool relativegraph = (show_min != 0 && show_min != show_max);
			float lastscaledvalue = 0.0;
			bool lastscaledvalue_exists = false;

			for (std::list<Piece>::const_iterator j = m_log.begin();
					j != m_log.end(); j++) {
				const Piece &piece = *j;
				float value = 0;
				bool value_exists = false;
				Profiler::GraphValues::const_iterator k =
					piece.values.find(id);

				if (k != piece.values.end()) {
					value = k->second;
					value_exists = true;
				}

				if (!value_exists) {
					x++;
					lastscaledvalue_exists = false;
					continue;
				}

				float scaledvalue = 1.0;

				if (show_max != show_min)
					scaledvalue = (value - show_min) / (show_max - show_min);

				if (scaledvalue == 1.0 && value == 0) {
					x++;
					lastscaledvalue_exists = false;
					continue;
				}

				if (relativegraph) {
					if (lastscaledvalue_exists) {
						s32 ivalue1 = lastscaledvalue * graph1h;
						s32 ivalue2 = scaledvalue * graph1h;
						driver->draw2DLine(v2s32(x - 1, graph1y - ivalue1),
								   v2s32(x, graph1y - ivalue2), meta.color);
					}

					lastscaledvalue = scaledvalue;
					lastscaledvalue_exists = true;
				} else {
					s32 ivalue = scaledvalue * graph1h;
					driver->draw2DLine(v2s32(x, graph1y),
							   v2s32(x, graph1y - ivalue), meta.color);
				}

				x++;
			}

			meta_i++;
		}
	}
};

class NodeDugEvent: public MtEvent
{
public:
	v3s16 p;
	MapNode n;

	NodeDugEvent(v3s16 p, MapNode n):
		p(p),
		n(n)
	{}
	const char *getType() const
	{
		return "NodeDug";
	}
};

class SoundMaker
{
	ISoundManager *m_sound;
	INodeDefManager *m_ndef;
public:
	float m_player_step_timer;

	SimpleSoundSpec m_player_step_sound;
	SimpleSoundSpec m_player_leftpunch_sound;
	SimpleSoundSpec m_player_rightpunch_sound;

	SoundMaker(ISoundManager *sound, INodeDefManager *ndef):
		m_sound(sound),
		m_ndef(ndef),
		m_player_step_timer(0)
	{
	}

	void playPlayerStep()
	{
		if (m_player_step_timer <= 0 && m_player_step_sound.exists()) {
			m_player_step_timer = 0.03;
			m_sound->playSound(m_player_step_sound, false);
		}
	}

	static void viewBobbingStep(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerStep();
	}

	static void playerRegainGround(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerStep();
	}

	static void playerJump(MtEvent *e, void *data)
	{
		//SoundMaker *sm = (SoundMaker*)data;
	}

	static void cameraPunchLeft(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(sm->m_player_leftpunch_sound, false);
	}

	static void cameraPunchRight(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(sm->m_player_rightpunch_sound, false);
	}

	static void nodeDug(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		NodeDugEvent *nde = (NodeDugEvent *)e;
		sm->m_sound->playSound(sm->m_ndef->get(nde->n).sound_dug, false);
	}

	static void playerDamage(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(SimpleSoundSpec("player_damage", 0.5), false);
	}

	static void playerFallingDamage(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(SimpleSoundSpec("player_falling_damage", 0.5), false);
	}

	void registerReceiver(MtEventManager *mgr)
	{
		mgr->reg("ViewBobbingStep", SoundMaker::viewBobbingStep, this);
		mgr->reg("PlayerRegainGround", SoundMaker::playerRegainGround, this);
		mgr->reg("PlayerJump", SoundMaker::playerJump, this);
		mgr->reg("CameraPunchLeft", SoundMaker::cameraPunchLeft, this);
		mgr->reg("CameraPunchRight", SoundMaker::cameraPunchRight, this);
		mgr->reg("NodeDug", SoundMaker::nodeDug, this);
		mgr->reg("PlayerDamage", SoundMaker::playerDamage, this);
		mgr->reg("PlayerFallingDamage", SoundMaker::playerFallingDamage, this);
	}

	void step(float dtime)
	{
		m_player_step_timer -= dtime;
	}
};

// Locally stored sounds don't need to be preloaded because of this
class GameOnDemandSoundFetcher: public OnDemandSoundFetcher
{
	std::set<std::string> m_fetched;
public:
	void fetchSounds(const std::string &name,
			std::set<std::string> &dst_paths,
			std::set<std::string> &dst_datas)
	{
		if (m_fetched.count(name))
			return;

		m_fetched.insert(name);
		std::string base = porting::path_share + DIR_DELIM + "testsounds";
		dst_paths.insert(base + DIR_DELIM + name + ".ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".0.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".1.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".2.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".3.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".4.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".5.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".6.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".7.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".8.ogg");
		dst_paths.insert(base + DIR_DELIM + name + ".9.ogg");
	}
};

class GameGlobalShaderConstantSetter : public IShaderConstantSetter
{
	Sky *m_sky;
	bool *m_force_fog_off;
	f32 *m_fog_range;
	Client *m_client;
	Inventory *m_local_inventory;

public:
	GameGlobalShaderConstantSetter(Sky *sky, bool *force_fog_off,
			f32 *fog_range, Client *client, Inventory *local_inventory) :
		m_sky(sky),
		m_force_fog_off(force_fog_off),
		m_fog_range(fog_range),
		m_client(client),
		m_local_inventory(local_inventory)
	{}
	~GameGlobalShaderConstantSetter() {}

	virtual void onSetConstants(video::IMaterialRendererServices *services,
			bool is_highlevel)
	{
		if (!is_highlevel)
			return;

		// Background color
		video::SColor bgcolor = m_sky->getBgColor();
		video::SColorf bgcolorf(bgcolor);
		float bgcolorfa[4] = {
			bgcolorf.r,
			bgcolorf.g,
			bgcolorf.b,
			bgcolorf.a,
		};
		services->setPixelShaderConstant("skyBgColor", bgcolorfa, 4);

		// Fog distance
		float fog_distance = 10000 * BS;

		if (g_settings->getBool("enable_fog") && !*m_force_fog_off)
			fog_distance = *m_fog_range;

		services->setPixelShaderConstant("fogDistance", &fog_distance, 1);

		// Day-night ratio
		u32 daynight_ratio = m_client->getEnv().getDayNightRatio();
		float daynight_ratio_f = (float)daynight_ratio / 1000.0;
		services->setPixelShaderConstant("dayNightRatio", &daynight_ratio_f, 1);

		u32 animation_timer = porting::getTimeMs() % 100000;
		float animation_timer_f = (float)animation_timer / 100000.0;
		services->setPixelShaderConstant("animationTimer", &animation_timer_f, 1);
		services->setVertexShaderConstant("animationTimer", &animation_timer_f, 1);

		LocalPlayer *player = m_client->getEnv().getLocalPlayer();
		v3f eye_position = player->getEyePosition();
		services->setPixelShaderConstant("eyePosition", (irr::f32 *)&eye_position, 3);
		services->setVertexShaderConstant("eyePosition", (irr::f32 *)&eye_position, 3);

		v3f sun_moon_position;
		if (m_sky->sun_moon_light) {
			sun_moon_position = m_sky->sun_moon_light->getPosition();
		} else {
			sun_moon_position = v3f(0.0, eye_position.Y*BS+900.0, 0.0);
		}
		services->setPixelShaderConstant("sunPosition", (irr::f32 *)&sun_moon_position, 3);
		services->setVertexShaderConstant("sunPosition", (irr::f32 *)&sun_moon_position, 3);

		// Uniform sampler layers
		int layer0 = 0;
		int layer1 = 1;
		int layer2 = 2;
		// before 1.8 there isn't a "integer interface", only float
#if (IRRLICHT_VERSION_MAJOR == 1 && IRRLICHT_VERSION_MINOR < 8)
		services->setPixelShaderConstant("baseTexture" , (irr::f32 *)&layer0, 1);
		services->setPixelShaderConstant("normalTexture" , (irr::f32 *)&layer1, 1);
		services->setPixelShaderConstant("useNormalmap" , (irr::f32 *)&layer2, 1);
#else
		services->setPixelShaderConstant("baseTexture" , (irr::s32 *)&layer0, 1);
		services->setPixelShaderConstant("normalTexture" , (irr::s32 *)&layer1, 1);
		services->setPixelShaderConstant("useNormalmap" , (irr::s32 *)&layer2, 1);
#endif
		ItemStack playeritem;
		{
			InventoryList *mlist = m_local_inventory->getList("main");
			if(mlist != NULL)
			{
				playeritem = mlist->getItem(m_client->getPlayerItem());
			}
		}
		irr::f32 wieldLight = 0;
		if (g_settings->getBool("disable_wieldlight") == false)
			wieldLight = (irr::f32)((ItemGroupList)m_client->idef()->get(playeritem.name).groups)["wield_light"];
		services->setPixelShaderConstant("wieldLight", &wieldLight, 1);
	}
};

bool nodePlacementPrediction(Client &client,
		const ItemDefinition &playeritem_def, v3s16 nodepos, v3s16 neighbourpos)
{
	std::string prediction = playeritem_def.node_placement_prediction;
	INodeDefManager *nodedef = client.ndef();
	ClientMap &map = client.getEnv().getClientMap();
	MapNode node;
	bool is_valid_position;

	node = map.getNodeNoEx(nodepos, &is_valid_position);
	if (!is_valid_position)
		return false;

	if (prediction != "" && !nodedef->get(node).rightclickable) {
		verbosestream << "Node placement prediction for "
			      << playeritem_def.name << " is "
			      << prediction << std::endl;
		v3s16 p = neighbourpos;

		// Place inside node itself if buildable_to
		MapNode n_under = map.getNodeNoEx(nodepos, &is_valid_position);
		if (is_valid_position)
		{
			if (nodedef->get(n_under).buildable_to)
				p = nodepos;
			else {
				node = map.getNodeNoEx(p, &is_valid_position);
				if (is_valid_position &&!nodedef->get(node).buildable_to)
					return false;
			}
		}

		// Find id of predicted node
		content_t id;
		bool found = nodedef->getId(prediction, id);

		if (!found) {
			errorstream << "Node placement prediction failed for "
				    << playeritem_def.name << " (places "
				    << prediction
				    << ") - Name not known" << std::endl;
			return false;
		}

		// Predict param2 for facedir and wallmounted nodes
		u8 param2 = 0;

		if (nodedef->get(id).param_type_2 == CPT2_WALLMOUNTED) {
			v3s16 dir = nodepos - neighbourpos;

			if (abs(dir.Y) > MYMAX(abs(dir.X), abs(dir.Z))) {
				param2 = dir.Y < 0 ? 1 : 0;
			} else if (abs(dir.X) > abs(dir.Z)) {
				param2 = dir.X < 0 ? 3 : 2;
			} else {
				param2 = dir.Z < 0 ? 5 : 4;
			}
		}

		if (nodedef->get(id).param_type_2 == CPT2_FACEDIR) {
			v3s16 dir = nodepos - floatToInt(client.getEnv().getLocalPlayer()->getPosition(), BS);

			if (abs(dir.X) > abs(dir.Z)) {
				param2 = dir.X < 0 ? 3 : 1;
			} else {
				param2 = dir.Z < 0 ? 2 : 0;
			}
		}

		assert(param2 <= 5);

		//Check attachment if node is in group attached_node
		if (((ItemGroupList) nodedef->get(id).groups)["attached_node"] != 0) {
			static v3s16 wallmounted_dirs[8] = {
				v3s16(0, 1, 0),
				v3s16(0, -1, 0),
				v3s16(1, 0, 0),
				v3s16(-1, 0, 0),
				v3s16(0, 0, 1),
				v3s16(0, 0, -1),
			};
			v3s16 pp;

			if (nodedef->get(id).param_type_2 == CPT2_WALLMOUNTED)
				pp = p + wallmounted_dirs[param2];
			else
				pp = p + v3s16(0, -1, 0);

			if (!nodedef->get(map.getNodeNoEx(pp)).walkable)
				return false;
		}

		// Add node to client map
		MapNode n(id, 0, param2);

		try {
			LocalPlayer *player = client.getEnv().getLocalPlayer();

			// Dont place node when player would be inside new node
			// NOTE: This is to be eventually implemented by a mod as client-side Lua

			if(player->canPlaceNode(p, n)) {
				// This triggers the required mesh update too
				client.addNode(p, n, nodedef->get(id).light_source ? 3 : 1); // add without liquids
				return true;
			}
		} catch (InvalidPositionException &e) {
			errorstream << "Node placement prediction failed for "
				    << playeritem_def.name << " (places "
				    << prediction
				    << ") - Position not loaded" << std::endl;
		}
	}

	return false;
}

static inline void create_formspec_menu(GUIFormSpecMenu **cur_formspec,
		InventoryManager *invmgr, IGameDef *gamedef,
		IWritableTextureSource *tsrc, IrrlichtDevice *device,
		IFormSource *fs_src, TextDest *txt_dest, Client *client)
{

	if (*cur_formspec == 0) {
		*cur_formspec = new GUIFormSpecMenu(device, guiroot, -1, &g_menumgr,
						    invmgr, gamedef, tsrc, fs_src, txt_dest, client);
		(*cur_formspec)->doPause = false;

		/*
			Caution: do not call (*cur_formspec)->drop() here --
			the reference might outlive the menu, so we will
			periodically check if *cur_formspec is the only
			remaining reference (i.e. the menu was removed)
			and delete it in that case.
		*/

	} else {
		(*cur_formspec)->setFormSource(fs_src);
		(*cur_formspec)->setTextDest(txt_dest);
	}
}

#ifdef __ANDROID__
#define SIZE_TAG "size[11,5.5]"
#else
#define SIZE_TAG "size[11,5.5,true]"
#endif

#if 0
static void show_chat_menu(GUIFormSpecMenu **cur_formspec,
		InventoryManager *invmgr, IGameDef *gamedef,
		IWritableTextureSource *tsrc, IrrlichtDevice *device,
		Client *client, std::string text)
{
	std::string formspec =
		FORMSPEC_VERSION_STRING
		SIZE_TAG
		"field[3,2.35;6,0.5;f_text;;" + text + "]"
		"button_exit[4,3;3,0.5;btn_send;" + wide_to_narrow(wstrgettext("Proceed")) + "]"
		;

	/* Create menu */
	/* Note: FormspecFormSource and LocalFormspecHandler
	 * are deleted by guiFormSpecMenu                     */
	FormspecFormSource *fs_src = new FormspecFormSource(formspec);
	LocalFormspecHandler *txt_dst = new LocalFormspecHandler("MT_CHAT_MENU", client);

	create_formspec_menu(cur_formspec, invmgr, gamedef, tsrc, device, fs_src, txt_dst, NULL);
}
#endif

static void show_deathscreen(GUIFormSpecMenu **cur_formspec,
		InventoryManager *invmgr, IGameDef *gamedef,
		IWritableTextureSource *tsrc, IrrlichtDevice *device, Client *client)
{
	std::string formspec =
		std::string(FORMSPEC_VERSION_STRING) +
		SIZE_TAG
		"bgcolor[#320000b4;true]"
		"label[4.85,1.35;You died.]"
		"button_exit[4,3;3,0.5;btn_respawn;" + _("Respawn") + "]"
		;

	/* Create menu */
	/* Note: FormspecFormSource and LocalFormspecHandler
	 * are deleted by guiFormSpecMenu                     */
	FormspecFormSource *fs_src = new FormspecFormSource(formspec);
	LocalFormspecHandler *txt_dst = new LocalFormspecHandler("MT_DEATH_SCREEN", client);

	create_formspec_menu(cur_formspec, invmgr, gamedef, tsrc, device,  fs_src, txt_dst, NULL);
}

/******************************************************************************/
static void show_pause_menu(GUIFormSpecMenu **cur_formspec,
		InventoryManager *invmgr, IGameDef *gamedef,
		IWritableTextureSource *tsrc, IrrlichtDevice *device,
		bool singleplayermode)
{
#ifdef __ANDROID__
	std::string control_text = wide_to_narrow(wstrgettext("Default Controls:\n"
				   "No menu visible:\n"
				   "- single tap: button activate\n"
				   "- double tap: place/use\n"
				   "- slide finger: look around\n"
				   "Menu/Inventory visible:\n"
				   "- double tap (outside):\n"
				   " -->close\n"
				   "- touch stack, touch slot:\n"
				   " --> move stack\n"
				   "- touch&drag, tap 2nd finger\n"
				   " --> place single item to slot\n"
							     ));
#else
	std::string control_text = wide_to_narrow(wstrgettext("Default Controls:\n"
				   "- WASD: move\n"
				   "- Space: jump/climb\n"
				   "- Shift: sneak/go down\n"
				   "- Q: drop item\n"
				   "- I: inventory\n"
				   "- Mouse: turn/look\n"
				   "- Mouse left: dig/punch\n"
				   "- Mouse right: place/use\n"
				   "- Mouse wheel: select item\n"
				   "- T: chat\n"
							     ));
#endif

	float ypos = singleplayermode ? 0.5 : 0.1;
	std::ostringstream os;

	os << FORMSPEC_VERSION_STRING  << SIZE_TAG
	   << "button_exit[4," << (ypos++) << ";3,0.5;btn_continue;"
	   << wide_to_narrow(wstrgettext("Continue"))     << "]";

	if (!singleplayermode) {
		os << "button_exit[4," << (ypos++) << ";3,0.5;btn_change_password;"
		   << wide_to_narrow(wstrgettext("Change Password")) << "]";
	}

	os		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_sound;"
			<< wide_to_narrow(wstrgettext("Sound Volume")) << "]";
	os		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_key_config;"
			<< wide_to_narrow(wstrgettext("Change Keys"))  << "]";
	os		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_exit_menu;"
			<< wide_to_narrow(wstrgettext("Exit to Menu")) << "]";
	os		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_exit_os;"
			<< wide_to_narrow(wstrgettext("Exit to OS"))   << "]"
;
/*
			<< "textarea[7.5,0.25;3.9,6.25;;" << control_text << ";]"
			<< "textarea[0.4,0.25;3.5,6;;" << "Freeminer\n"
			<< minetest_build_info << "\n"
			<< "path_user = " << wrap_rows(porting::path_user, 20)
			<< "\n;]";
*/

	/* Create menu */
	/* Note: FormspecFormSource and LocalFormspecHandler  *
	 * are deleted by guiFormSpecMenu                     */
	FormspecFormSource *fs_src = new FormspecFormSource(os.str());
	LocalFormspecHandler *txt_dst = new LocalFormspecHandler("MT_PAUSE_MENU");

	create_formspec_menu(cur_formspec, invmgr, gamedef, tsrc, device,  fs_src, txt_dst, NULL);

	(*cur_formspec)->doPause = true;
}

/******************************************************************************/
static void updateChat(Client &client, f32 dtime, bool show_debug,
		const v2u32 &screensize, bool show_chat, u32 show_profiler,
		ChatBackend &chat_backend, gui::IGUIStaticText *guitext_chat)
{
	// Add chat log output for errors to be shown in chat
	static LogOutputBuffer chat_log_error_buf(LMT_ERROR);

	// Get new messages from error log buffer
	while (!chat_log_error_buf.empty()) {
		chat_backend.addMessage(L"", utf8_to_wide(chat_log_error_buf.get()));
	}

	// Get new messages from client
	std::string message;

	while (client.getChatMessage(message)) {
		chat_backend.addUnparsedMessage(utf8_to_wide(message));
	}

	// Remove old messages
	chat_backend.step(dtime);

	// Display all messages in a static text element
	unsigned int recent_chat_count = chat_backend.getRecentBuffer().getLineCount();
	std::wstring recent_chat       = chat_backend.getRecentChat();
	unsigned int line_height       = g_fontengine->getLineHeight();

	guitext_chat->setText(recent_chat.c_str());

	// Update gui element size and position
	s32 chat_y = 5 + line_height;

	if (show_debug)
		chat_y += line_height;

	// first pass to calculate height of text to be set
	s32 width = std::min(g_fontengine->getTextWidth(recent_chat) + 10,
			     porting::getWindowSize().X - 20);
	core::rect<s32> rect(10, chat_y, width, chat_y + porting::getWindowSize().Y);
	guitext_chat->setRelativePosition(rect);

	//now use real height of text and adjust rect according to this size
	rect = core::rect<s32>(10, chat_y, width,
			       chat_y + guitext_chat->getTextHeight());


	guitext_chat->setRelativePosition(rect);
	// Don't show chat if disabled or empty or profiler is enabled
	guitext_chat->setVisible(
		show_chat && recent_chat_count != 0 && !show_profiler);
}


/****************************************************************************
 Fast key cache for main game loop
 ****************************************************************************/

/* This is faster than using getKeySetting with the tradeoff that functions
 * using it must make sure that it's initialised before using it and there is
 * no error handling (for example bounds checking). This is really intended for
 * use only in the main running loop of the client (the_game()) where the faster
 * (up to 10x faster) key lookup is an asset. Other parts of the codebase
 * (e.g. formspecs) should continue using getKeySetting().
 */
struct KeyCache {

	KeyCache() { populate(); }

	enum {
		// Player movement
		KEYMAP_ID_FORWARD,
		KEYMAP_ID_BACKWARD,
		KEYMAP_ID_LEFT,
		KEYMAP_ID_RIGHT,
		KEYMAP_ID_JUMP,
		KEYMAP_ID_SPECIAL1,
		KEYMAP_ID_SNEAK,

		// Other
		KEYMAP_ID_DROP,
		KEYMAP_ID_INVENTORY,
		KEYMAP_ID_CHAT,
		KEYMAP_ID_CMD,
		KEYMAP_ID_CONSOLE,
		KEYMAP_ID_FREEMOVE,
		KEYMAP_ID_FASTMOVE,
		KEYMAP_ID_NOCLIP,
		KEYMAP_ID_SCREENSHOT,
		KEYMAP_ID_TOGGLE_HUD,
		KEYMAP_ID_TOGGLE_CHAT,
		KEYMAP_ID_TOGGLE_FORCE_FOG_OFF,
		KEYMAP_ID_TOGGLE_UPDATE_CAMERA,
		KEYMAP_ID_TOGGLE_DEBUG,
		KEYMAP_ID_TOGGLE_PROFILER,
		KEYMAP_ID_CAMERA_MODE,
		KEYMAP_ID_INCREASE_VIEWING_RANGE,
		KEYMAP_ID_DECREASE_VIEWING_RANGE,
		KEYMAP_ID_RANGESELECT,

		KEYMAP_ID_QUICKTUNE_NEXT,
		KEYMAP_ID_QUICKTUNE_PREV,
		KEYMAP_ID_QUICKTUNE_INC,
		KEYMAP_ID_QUICKTUNE_DEC,

		KEYMAP_ID_DEBUG_STACKS,

		//freeminer
		KEYMAP_ID_MSG,
		KEYMAP_ID_ZOOM,
		KEYMAP_ID_PLAYERLIST,

		// Fake keycode for array size and internal checks
		KEYMAP_INTERNAL_ENUM_COUNT

	};

	void populate();

	KeyPress key[KEYMAP_INTERNAL_ENUM_COUNT];
};

void KeyCache::populate()
{
	key[KEYMAP_ID_FORWARD]      = getKeySetting("keymap_forward");
	key[KEYMAP_ID_BACKWARD]     = getKeySetting("keymap_backward");
	key[KEYMAP_ID_LEFT]         = getKeySetting("keymap_left");
	key[KEYMAP_ID_RIGHT]        = getKeySetting("keymap_right");
	key[KEYMAP_ID_JUMP]         = getKeySetting("keymap_jump");
	key[KEYMAP_ID_SPECIAL1]     = getKeySetting("keymap_special1");
	key[KEYMAP_ID_SNEAK]        = getKeySetting("keymap_sneak");

	key[KEYMAP_ID_DROP]         = getKeySetting("keymap_drop");
	key[KEYMAP_ID_INVENTORY]    = getKeySetting("keymap_inventory");
	key[KEYMAP_ID_CHAT]         = getKeySetting("keymap_chat");
	key[KEYMAP_ID_CMD]          = getKeySetting("keymap_cmd");
	key[KEYMAP_ID_CONSOLE]      = getKeySetting("keymap_console");
	key[KEYMAP_ID_FREEMOVE]     = getKeySetting("keymap_freemove");
	key[KEYMAP_ID_FASTMOVE]     = getKeySetting("keymap_fastmove");
	key[KEYMAP_ID_NOCLIP]       = getKeySetting("keymap_noclip");
	key[KEYMAP_ID_SCREENSHOT]   = getKeySetting("keymap_screenshot");
	key[KEYMAP_ID_TOGGLE_HUD]   = getKeySetting("keymap_toggle_hud");
	key[KEYMAP_ID_TOGGLE_CHAT]  = getKeySetting("keymap_toggle_chat");
	key[KEYMAP_ID_TOGGLE_FORCE_FOG_OFF]
			= getKeySetting("keymap_toggle_force_fog_off");
/*
	key[KEYMAP_ID_TOGGLE_UPDATE_CAMERA]
			= getKeySetting("keymap_toggle_update_camera");
*/
	key[KEYMAP_ID_TOGGLE_DEBUG]
			= getKeySetting("keymap_toggle_debug");
	key[KEYMAP_ID_TOGGLE_PROFILER]
			= getKeySetting("keymap_toggle_profiler");
	key[KEYMAP_ID_CAMERA_MODE]
			= getKeySetting("keymap_camera_mode");
	key[KEYMAP_ID_INCREASE_VIEWING_RANGE]
			= getKeySetting("keymap_increase_viewing_range_min");
	key[KEYMAP_ID_DECREASE_VIEWING_RANGE]
			= getKeySetting("keymap_decrease_viewing_range_min");
	key[KEYMAP_ID_RANGESELECT]
			= getKeySetting("keymap_rangeselect");

	key[KEYMAP_ID_QUICKTUNE_NEXT] = getKeySetting("keymap_quicktune_next");
	key[KEYMAP_ID_QUICKTUNE_PREV] = getKeySetting("keymap_quicktune_prev");
	key[KEYMAP_ID_QUICKTUNE_INC]  = getKeySetting("keymap_quicktune_inc");
	key[KEYMAP_ID_QUICKTUNE_DEC]  = getKeySetting("keymap_quicktune_dec");

	key[KEYMAP_ID_DEBUG_STACKS]   = getKeySetting("keymap_print_debug_stacks");

	//freeminer:
	key[KEYMAP_ID_MSG]            = getKeySetting("keymap_msg");
	key[KEYMAP_ID_ZOOM]           = getKeySetting("keymap_zoom");
	key[KEYMAP_ID_PLAYERLIST]     = getKeySetting("keymap_playerlist");

}


/****************************************************************************

 ****************************************************************************/

const float object_hit_delay = 0.2;

struct FpsControl {
	u32 last_time, busy_time, sleep_time;
};


/* The reason the following structs are not anonymous structs within the
 * class is that they are not used by the majority of member functions and
 * many functions that do require objects of thse types do not modify them
 * (so they can be passed as a const qualified parameter)
 */

struct CameraOrientation {
	f32 camera_yaw;    // "right/left"
	f32 camera_pitch;  // "up/down"
};

struct GameRunData {
	u16 dig_index;
	u16 new_playeritem;
	PointedThing pointed_old;
	bool digging;
	bool ldown_for_dig;
	bool left_punch;
	bool update_wielded_item_trigger;
	bool reset_jump_timer;
	float nodig_delay_timer;
	float dig_time;
	float dig_time_complete;
	float repeat_rightclick_timer;
	float object_hit_delay_timer;
	float time_from_last_punch;
	ClientActiveObject *selected_object;

	float jump_timer;
	float damage_flash;
	float update_draw_list_timer;
	float statustext_time;

	f32 fog_range;

	v3f update_draw_list_last_cam_dir;

	u32 profiler_current_page;
	u32 profiler_max_page;     // Number of pages

	//freeminer:
	v3f update_draw_list_last_cam_pos;
	unsigned int autoexit;
	bool profiler_state;

	float time_of_day;
	float time_of_day_smooth;
};

struct Jitter {
	f32 max, min, avg, counter, max_sample, min_sample, max_fraction;
};

struct RunStats {
	u32 drawtime;
	u32 beginscenetime;
	u32 endscenetime;

	Jitter dtime_jitter, busy_time_jitter;
};

/* Flags that can, or may, change during main game loop
 */
struct VolatileRunFlags {
	bool invert_mouse;
	bool show_chat;
	bool show_hud;
	bool force_fog_off;
	bool show_debug;
	bool show_profiler_graph;
	bool disable_camera_update;
	bool first_loop_after_window_activation;
	bool camera_offset_changed;

	//freeminer:
	bool no_output;
	bool use_weather;
	float dedicated_server_step;
	int errors;
	bool show_block_boundaries;
	bool connected;
	bool reconnect;
};


/****************************************************************************
 THE GAME
 ****************************************************************************/

/* This is not intended to be a public class. If a public class becomes
 * desirable then it may be better to create another 'wrapper' class that
 * hides most of the stuff in this class (nothing in this class is required
 * by any other file) but exposes the public methods/data only.
 */
class Game
{
public:
	Game();
	~Game();

	bool startup(bool *kill,
			bool random_input,
			InputHandler *input,
			IrrlichtDevice *device,
			const std::string &map_dir,
			const std::string &playername,
			const std::string &password,
			// If address is "", local server is used and address is updated
			std::string *address,
			u16 port,
			std::string *error_message,
			ChatBackend *chat_backend,
			const SubgameSpec &gamespec,    // Used for local game
			bool simple_singleplayer_mode);

	void run();
	void shutdown();

protected:

	void extendedResourceCleanup();

	// Basic initialisation
	bool init(const std::string &map_dir, std::string *address,
			u16 port,
			const SubgameSpec &gamespec);
	bool initSound();
	bool createSingleplayerServer(const std::string map_dir,
			const SubgameSpec &gamespec, u16 port, std::string *address);

	// Client creation
	bool createClient(const std::string &playername,
			const std::string &password, std::string *address, u16 port,
			std::string *error_message);
	bool initGui(std::string *error_message);

	// Client connection
	bool connectToServer(const std::string &playername,
			const std::string &password, std::string *address, u16 port,
			bool *connect_ok, bool *aborted);
	bool getServerContent(bool *aborted);

	// Main loop

	void updateInteractTimers(GameRunData *args, f32 dtime);
	bool checkConnection();
	bool handleCallbacks();
	void processQueues();
	void updateProfilers(const GameRunData &run_data, const RunStats &stats,
			const FpsControl &draw_times, f32 dtime);
	void addProfilerGraphs(const RunStats &stats, const FpsControl &draw_times,
			f32 dtime);
	void updateStats(RunStats *stats, const FpsControl &draw_times, f32 dtime);

	void processUserInput(VolatileRunFlags *flags, GameRunData *interact_args,
			f32 dtime);
	void processKeyboardInput(VolatileRunFlags *flags,
			float *statustext_time,
			float *jump_timer,
			bool *reset_jump_timer,
			u32 *profiler_current_page,
			u32 profiler_max_page);
	void processItemSelection(u16 *new_playeritem);

	void dropSelectedItem();
	void openInventory();
	void openConsole(float height = 0.6, bool close_on_return = false, const std::wstring& input = L"");
	void toggleFreeMove(float *statustext_time);
	void toggleFreeMoveAlt(float *statustext_time, float *jump_timer);
	void toggleFast(float *statustext_time);
	void toggleNoClip(float *statustext_time);

	void toggleChat(float *statustext_time, bool *flag);
	void toggleHud(float *statustext_time, bool *flag);
	void toggleFog(float *statustext_time, bool *flag);
	void toggleDebug(float *statustext_time, bool *show_debug,
			bool *show_profiler_graph);
	void toggleUpdateCamera(float *statustext_time, bool *flag);
	void toggleBlockBoundaries(float *statustext_time, VolatileRunFlags *flags);
	void toggleProfiler(float *statustext_time, u32 *profiler_current_page,
			u32 profiler_max_page);

	void increaseViewRange(float *statustext_time);
	void decreaseViewRange(float *statustext_time);
	void toggleFullViewRange(float *statustext_time);

	void updateCameraDirection(CameraOrientation *cam, VolatileRunFlags *flags);
	void updatePlayerControl(const CameraOrientation &cam);
	void step(f32 *dtime);
	void processClientEvents(CameraOrientation *cam, float *damage_flash);
	void updateCamera(VolatileRunFlags *flags, u32 busy_time, f32 dtime,
			float time_from_last_punch);
	void updateSound(f32 dtime);
	void processPlayerInteraction(std::vector<aabb3f> &highlight_boxes,
			GameRunData *runData, f32 dtime, bool show_hud,
			bool show_debug);
	void handlePointingAtNode(GameRunData *runData,
			const PointedThing &pointed, const ItemDefinition &playeritem_def,
			const ToolCapabilities &playeritem_toolcap, f32 dtime);
	void handlePointingAtObject(GameRunData *runData,
			const PointedThing &pointed, const ItemStack &playeritem,
			const v3f &player_position, bool show_debug);
	void handleDigging(GameRunData *runData, const PointedThing &pointed,
			const v3s16 &nodepos, const ToolCapabilities &playeritem_toolcap,
			f32 dtime);
	void updateFrame(std::vector<aabb3f> &highlight_boxes, ProfilerGraph *graph,
			RunStats *stats, GameRunData *runData,
			f32 dtime, const VolatileRunFlags &flags, const CameraOrientation &cam);
	void updateGui(float *statustext_time, const RunStats &stats,
			const GameRunData& runData, f32 dtime, const VolatileRunFlags &flags,
			const CameraOrientation &cam);
	void updateProfilerGraphs(ProfilerGraph *graph);

	// Misc
	void limitFps(FpsControl *fps_timings, f32 *dtime);

	void showOverlayMessage(const char *msg, float dtime, int percent,
			bool draw_clouds = true);

private:
	InputHandler *input;

	Client *client;
	Server *server;

	IWritableTextureSource *texture_src;
	IWritableShaderSource *shader_src;

	// When created, these will be filled with data received from the server
	IWritableItemDefManager *itemdef_manager;
	IWritableNodeDefManager *nodedef_manager;

	GameOnDemandSoundFetcher soundfetcher; // useful when testing
	ISoundManager *sound;
	bool sound_is_dummy;
	SoundMaker *soundmaker;

	ChatBackend *chat_backend;

	GUIFormSpecMenu *current_formspec;

	EventManager *eventmgr;
	QuicktuneShortcutter *quicktune;

	GUIChatConsole *gui_chat_console; // Free using ->Drop()
	MapDrawControl *draw_control;
	Camera *camera;
	Clouds *clouds;	                  // Free using ->Drop()
	Sky *sky;                         // Free using ->Drop()
	Inventory *local_inventory;
	Hud *hud;

	/* 'cache'
	   This class does take ownership/responsibily for cleaning up etc of any of
	   these items (e.g. device)
	*/
	IrrlichtDevice *device;
	video::IVideoDriver *driver;
	scene::ISceneManager *smgr;
	bool *kill;
	std::string *error_message;
	IGameDef *gamedef;                     // Convenience (same as *client)
	scene::ISceneNode *skybox;

	bool random_input;
	bool simple_singleplayer_mode;
	/* End 'cache' */

	/* Pre-calculated values
	 */
	int crack_animation_length;

	/* GUI stuff
	 */
	gui::IGUIStaticText *guitext;          // First line of debug text
	gui::IGUIStaticText *guitext2;         // Second line of debug text
	gui::IGUIStaticText *guitext_info;     // At the middle of the screen
	gui::IGUIStaticText *guitext_status;
	gui::IGUIStaticText *guitext_chat;	   // Chat text
	gui::IGUIStaticText *guitext_profiler; // Profiler text

	std::wstring infotext;
	std::wstring statustext;

	//freeminer:
	GUITable *playerlist;
	video::SColor console_bg;
	gsMapper *mapper;
#if CMAKE_THREADS && CMAKE_HAVE_FUTURE
	std::future<void> updateDrawList_future;
#endif
public:
	VolatileRunFlags flags;
	GameRunData runData;
private:
	// minetest:

	KeyCache keycache;

	IntervalLimiter profiler_interval;

	/* TODO: Add a callback function so these can be updated when a setting
	 *       changes.  At this point in time it doesn't matter (e.g. /set
	 *       is documented to change server settings only)
	 *
	 * TODO: Local caching of settings is not optimal and should at some stage
	 *       be updated to use a global settings object for getting thse values
	 *       (as opposed to the this local caching). This can be addressed in
	 *       a later release.
	 */
	bool m_cache_doubletap_jump;
	bool m_cache_enable_node_highlighting;
	bool m_cache_enable_clouds;
	bool m_cache_enable_particles;
	bool m_cache_enable_fog;
	f32  m_cache_mouse_sensitivity;
	f32  m_repeat_right_click_time;
};

Game::Game() :
	client(NULL),
	server(NULL),
	texture_src(NULL),
	shader_src(NULL),
	itemdef_manager(NULL),
	nodedef_manager(NULL),
	sound(NULL),
	sound_is_dummy(false),
	soundmaker(NULL),
	chat_backend(NULL),
	current_formspec(NULL),
	eventmgr(NULL),
	quicktune(NULL),
	gui_chat_console(NULL),
	draw_control(NULL),
	camera(NULL),
	clouds(NULL),
	sky(NULL),
	local_inventory(NULL),
	hud(NULL)
	,
	playerlist(nullptr),
	mapper(nullptr)
{
	m_cache_doubletap_jump            = g_settings->getBool("doubletap_jump");
	m_cache_enable_node_highlighting  = g_settings->getBool("enable_node_highlighting");
	m_cache_enable_clouds             = g_settings->getBool("enable_clouds");
	m_cache_enable_particles          = g_settings->getBool("enable_particles");
	m_cache_enable_fog                = g_settings->getBool("enable_fog");
	m_cache_mouse_sensitivity         = g_settings->getFloat("mouse_sensitivity");
	m_repeat_right_click_time         = g_settings->getFloat("repeat_rightclick_time");
}


/****************************************************************************
 Game Public
 ****************************************************************************/

Game::~Game()
{
	delete client;
	delete soundmaker;
	if (!sound_is_dummy)
		delete sound;

	delete server; // deleted first to stop all server threads

	delete hud;
	delete local_inventory;
	delete camera;
	delete quicktune;
	delete eventmgr;
	delete texture_src;
	delete shader_src;
	delete nodedef_manager;
	delete itemdef_manager;
	delete draw_control;

	if (mapper)
		delete mapper;

	extendedResourceCleanup();
}

bool Game::startup(bool *kill,
		bool random_input,
		InputHandler *input,
		IrrlichtDevice *device,
		const std::string &map_dir,
		const std::string &playername,
		const std::string &password,
		std::string *address,     // can change if simple_singleplayer_mode
		u16 port,
		std::string *error_message,
		ChatBackend *chat_backend,
		const SubgameSpec &gamespec,
		bool simple_singleplayer_mode)
{
	// "cache"
	this->device        = device;
	this->kill          = kill;
	this->error_message = error_message;
	this->random_input  = random_input;
	this->input         = input;
	this->chat_backend  = chat_backend;
	this->simple_singleplayer_mode = simple_singleplayer_mode;

	driver              = device->getVideoDriver();
	smgr                = device->getSceneManager();

	smgr->getParameters()->setAttribute(scene::OBJ_LOADER_IGNORE_MATERIAL_FILES, true);

	if (!init(map_dir, address, port, gamespec))
		return false;

	if (!createClient(playername, password, address, port, error_message))
		return false;

	return true;
}


void Game::run()
{
	ProfilerGraph graph;
	RunStats stats              = { 0 };
	CameraOrientation cam_view  = { 0 };
	//runData         = { 0 };
	FpsControl draw_times       = { 0 };
	flags      = { 0 };
	f32 dtime; // in seconds

	runData.time_from_last_punch  = 10.0;
	runData.profiler_max_page = 2;
	runData.update_wielded_item_trigger = true;

	flags.show_chat = true;
	flags.show_hud = true;
	flags.show_debug = g_settings->getBool("show_debug");
	flags.invert_mouse = g_settings->getBool("invert_mouse");


	// freeminer:
	runData.update_draw_list_timer = 5;
	flags.dedicated_server_step = g_settings->getFloat("dedicated_server_step");
	flags.use_weather = g_settings->getBool("weather");
	flags.no_output = device->getVideoDriver()->getDriverType() == video::EDT_NULL;


	/* Clear the profiler */
	Profiler::GraphValues dummyvalues;
	g_profiler->graphGet(dummyvalues);

	draw_times.last_time = device->getTimer()->getTime();

	shader_src->addGlobalConstantSetter(new GameGlobalShaderConstantSetter(
			sky,
			&flags.force_fog_off,
			&runData.fog_range,
			client,
			local_inventory
			));

	std::vector<aabb3f> highlight_boxes;

	double run_time = 0;

	while (device->run() && !(*kill || g_gamecallback->shutdown_requested)) {

		/* Must be called immediately after a device->run() call because it
		 * uses device->getTimer()->getTime()
		 */
		limitFps(&draw_times, &dtime);
		run_time += dtime;
		if (runData.autoexit && run_time > runData.autoexit)
			g_gamecallback->shutdown_requested = 1;

		updateStats(&stats, draw_times, dtime);
		updateInteractTimers(&runData, dtime);

		if (!checkConnection())
			break;
		if (!handleCallbacks())
			break;

		processQueues();

		infotext = L"";
		hud->resizeHotbar();

		updateProfilers(runData, stats, draw_times, dtime);
		processUserInput(&flags, &runData, dtime);
		// Update camera before player movement to avoid camera lag of one frame
		updateCameraDirection(&cam_view, &flags);
		updatePlayerControl(cam_view);
		step(&dtime);
		processClientEvents(&cam_view, &runData.damage_flash);
		updateCamera(&flags, draw_times.busy_time, dtime,
				runData.time_from_last_punch);
		updateSound(dtime);
		processPlayerInteraction(highlight_boxes, &runData, dtime,
				flags.show_hud, flags.show_debug);
		updateFrame(highlight_boxes, &graph, &stats, &runData, dtime,
				flags, cam_view);
		updateProfilerGraphs(&graph);
	}
}


void Game::shutdown()
{

	if (runData.autoexit) {
		actionstream << "Profiler:" << std::fixed << std::setprecision(9) << std::endl;
		g_profiler->print(actionstream);
	}

	showOverlayMessage("Shutting down...", 0, 0, false);

	if (clouds)
		clouds->drop();

	if (gui_chat_console)
		gui_chat_console->drop();

	if (sky)
		sky->drop();

	clear_particles();

	/* cleanup menus */
	while (g_menumgr.menuCount() > 0) {
		g_menumgr.m_stack.front()->setVisible(false);
		g_menumgr.deletingMenu(g_menumgr.m_stack.front());
	}

	if (current_formspec) {
		current_formspec->drop();
		current_formspec = NULL;
	}

	chat_backend->addMessage(L"", L"# Disconnected.");
	chat_backend->addMessage(L"", L"");

	if (client) {
		client->Stop();
		if (texture_src)
			texture_src->processQueue();
		if (shader_src)
			shader_src->processQueue();
			sleep_ms(100);
	}

	guitext->remove();
	guitext2->remove();
	guitext_info->remove();
	guitext_status->remove();
	guitext_chat->remove();
	guitext_profiler->remove();

}



/****************************************************************************
 Startup
 ****************************************************************************/

bool Game::init(
		const std::string &map_dir,
		std::string *address,
		u16 port,
		const SubgameSpec &gamespec)
{
	showOverlayMessage("Loading...", 0, 0);

	texture_src = createTextureSource(device);
	shader_src = createShaderSource(device);

	itemdef_manager = createItemDefManager();
	nodedef_manager = createNodeDefManager();

	eventmgr = new EventManager();
	quicktune = new QuicktuneShortcutter();

	if (!(texture_src && shader_src && itemdef_manager && nodedef_manager
			&& eventmgr && quicktune))
		return false;

	if (!initSound())
		return false;

	// Create a server if not connecting to an existing one
	if (*address == "") {
		if (!createSingleplayerServer(map_dir, gamespec, port, address))
			return false;
	}

	return true;
}

bool Game::initSound()
{
#if USE_SOUND
	if (g_settings->getBool("enable_sound")) {
		infostream << "Attempting to use OpenAL audio" << std::endl;
		sound = createOpenALSoundManager(&soundfetcher);
		if (!sound)
			infostream << "Failed to initialize OpenAL audio" << std::endl;
	} else
		infostream << "Sound disabled." << std::endl;
#endif

	if (!sound) {
		infostream << "Using dummy audio." << std::endl;
		sound = &dummySoundManager;
		sound_is_dummy = true;
	}

	soundmaker = new SoundMaker(sound, nodedef_manager);
	if (!soundmaker)
		return false;

	soundmaker->registerReceiver(eventmgr);

	return true;
}

bool Game::createSingleplayerServer(const std::string map_dir,
		const SubgameSpec &gamespec, u16 port, std::string *address)
{
	showOverlayMessage("Creating server...", 0, 25);

	std::string bind_str = g_settings->get("bind_address");
	Address bind_addr(0, 0, 0, 0, port);

	if (g_settings->getBool("ipv6_server")) {
		bind_addr.setAddress((IPv6AddressBytes *) NULL);
	}

	try {
		bind_addr.Resolve(bind_str.c_str());
		*address = bind_str;
	} catch (ResolveError &e) {
		infostream << "Resolving bind address \"" << bind_str
			   << "\" failed: " << e.what()
			   << " -- Listening on all addresses." << std::endl;
	}

	if (bind_addr.isIPv6() && !g_settings->getBool("enable_ipv6")) {
		*error_message = "Unable to listen on " +
				bind_addr.serializeString() +
				" because IPv6 is disabled";
		errorstream << *error_message << std::endl;
		return false;
	}

	server = new Server(map_dir, gamespec, simple_singleplayer_mode,
			    bind_addr.isIPv6());

	server->start(bind_addr);

	return true;
}

bool Game::createClient(const std::string &playername,
		const std::string &password, std::string *address, u16 port,
		std::string *error_message)
{
	showOverlayMessage("Creating client...", 0, 50);

	device->setWindowCaption(L"Freeminer [Connecting]");

	draw_control = new MapDrawControl;
	if (!draw_control)
		return false;

	bool could_connect, connect_aborted;

	if (!connectToServer(playername, password, address, port,
			&could_connect, &connect_aborted))
		return false;

	if (!could_connect) {
		if (*error_message == "" && !connect_aborted) {
			// Should not happen if error messages are set properly
			*error_message = "Connection failed for unknown reason";
			errorstream << *error_message << std::endl;
		}
		return false;
	}

	if (!getServerContent(&connect_aborted)) {
		if (*error_message == "" && !connect_aborted) {
			// Should not happen if error messages are set properly
			*error_message = "Connection failed for unknown reason";
			errorstream << *error_message << std::endl;
		}
		return false;
	}

	// Update cached textures, meshes and materials
	client->afterContentReceived(device, g_fontengine->getFont());

	/* Camera
	 */
	camera = new Camera(smgr, *draw_control, gamedef);
	if (!camera || !camera->successfullyCreated(*error_message))
		return false;

	/* Clouds
	 */
	if (m_cache_enable_clouds) {
		clouds = new Clouds(smgr->getRootSceneNode(), smgr, -1, time(0));
		if (!clouds) {
			*error_message = "Memory allocation error";
			*error_message += " (clouds)";
			errorstream << *error_message << std::endl;
			return false;
		}
	}

	/* Skybox
	 */
	sky = new Sky(smgr->getRootSceneNode(), smgr, -1);
	skybox = NULL;	// This is used/set later on in the main run loop

	local_inventory = new Inventory(itemdef_manager);

	if (!(sky && local_inventory)) {
		*error_message = "Memory allocation error";
		*error_message += " (sky or local inventory)";
		errorstream << *error_message << std::endl;
		return false;
	}

	/* Pre-calculated values
	 */
	video::ITexture *t = texture_src->getTexture("crack_anylength.png");
	if (t) {
		v2u32 size = t->getOriginalSize();
		if (size.X)
		crack_animation_length = size.Y / size.X;
	} else {
		crack_animation_length = 0;
	}
	if (!crack_animation_length) {
		crack_animation_length = 0;
	}

	if (!initGui(error_message))
		return false;

	/* Set window caption
	 */
	core::stringw str = L"Freeminer [";
	str += driver->getName();
	str += "]";
	device->setWindowCaption(str.c_str());

	LocalPlayer *player = client->getEnv().getLocalPlayer();
	player->hurt_tilt_timer = 0;
	player->hurt_tilt_strength = 0;

	hud = new Hud(driver, smgr, guienv, gamedef, player, local_inventory);

	if (!hud) {
		*error_message = "Memory error: could not create HUD";
		errorstream << *error_message << std::endl;
		return false;
	}

	return true;
}

bool Game::initGui(std::string *error_message)
{
	// First line of debug text
	guitext = guienv->addStaticText(
			L"Freeminer",
			core::rect<s32>(0, 0, 0, 0),
			false, false, guiroot);

	// Second line of debug text
	guitext2 = guienv->addStaticText(
			L"",
			core::rect<s32>(0, 0, 0, 0),
			false, false, guiroot);

	// At the middle of the screen
	// Object infos are shown in this
	guitext_info = guienv->addStaticText(
			L"",
			core::rect<s32>(0, 0, 400, g_fontengine->getTextHeight() * 5 + 5) + v2s32(100, 200),
			false, true, guiroot);

	// Status text (displays info when showing and hiding GUI stuff, etc.)
	guitext_status = guienv->addStaticText(
			L"<Status>",
			core::rect<s32>(0, 0, 0, 0),
			false, false, guiroot);
	guitext_status->setVisible(false);

	// Chat text
	guitext_chat = nullptr;

	{
		guitext_chat = new gui::FMStaticText(L"", false, guienv, guienv->getRootGUIElement(), -1, core::rect<s32>(0, 0, 0, 0), false);
		guitext_chat->setWordWrap(true);
		guitext_chat->drop();
	}

	if (!guitext_chat) {
	guitext_chat = guienv->addStaticText(
			L"",
			core::rect<s32>(0, 0, 0, 0),
			//false, false); // Disable word wrap as of now
			false, true, guiroot);
	}

	// Remove stale "recent" chat messages from previous connections
	chat_backend->clearRecentChat();

	// Chat backend and console
	gui_chat_console = new GUIChatConsole(guienv, guienv->getRootGUIElement(),
			-1, chat_backend, client);
	if (!gui_chat_console) {
		*error_message = "Could not allocate memory for chat console";
		errorstream << *error_message << std::endl;
		return false;
	}

	// Profiler text (size is updated when text is updated)
	guitext_profiler = guienv->addStaticText(
			L"<Profiler>",
			core::rect<s32>(0, 0, 0, 0),
			false, false, guiroot);
	guitext_profiler->setBackgroundColor(video::SColor(120, 0, 0, 0));
	guitext_profiler->setVisible(false);
	guitext_profiler->setWordWrap(true);

#ifdef HAVE_TOUCHSCREENGUI

	if (g_touchscreengui)
		g_touchscreengui->init(texture_src, porting::getDisplayDensity());

#endif

	if(!g_settings->get("console_color").empty())
	{
		v3f console_color = g_settings->getV3F("console_color");
		console_bg = video::SColor(g_settings->getU16("console_alpha"), console_color.X, console_color.Y, console_color.Z);
	}

	v2u32 screensize = driver->getScreenSize();
	// create mapper
	mapper = new gsMapper(device, client);
	{
		// Update mapper elements
		u16 w = g_settings->getU16("hud_map_width");
		struct _gsm_color { u32 red; u32 green; u32 blue; } gsm_color;
		g_settings->getStruct("hud_map_back", "u32,u32,u32",
			&gsm_color, sizeof(gsm_color) );
		mapper->setMapVis(screensize.X-(w+10),10, w,
			g_settings->getU16("hud_map_height"),
			g_settings->getFloat("hud_map_scale"),
			g_settings->getU16("hud_map_alpha"),
			video::SColor(0, gsm_color.red, gsm_color.green, gsm_color.blue));
		mapper->setMapType(g_settings->getBool("hud_map_above"),
			g_settings->getU16("hud_map_scan"),
			g_settings->getS16("hud_map_surface"),
			g_settings->getBool("hud_map_tracking"),
			g_settings->getU16("hud_map_border"));
	}


	return true;
}

bool Game::connectToServer(const std::string &playername,
		const std::string &password, std::string *address, u16 port,
		bool *connect_ok, bool *aborted)
{
	showOverlayMessage("Resolving address...", 0, 75);

	Address connect_address(0, 0, 0, 0, port);

	try {
		connect_address.Resolve(address->c_str());

		if (connect_address.isZero()) { // i.e. INADDR_ANY, IN6ADDR_ANY
			//connect_address.Resolve("localhost");
			if (connect_address.isIPv6() || g_settings->getBool("ipv6_server")) {
				connect_address.setAddress(in6addr_loopback);
			} else {
				connect_address.setAddress(127, 0, 0, 1);
			}
		}
	} catch (ResolveError &e) {
		*error_message = std::string("Couldn't resolve address: ") + e.what();
		errorstream << *error_message << std::endl;
		return false;
	}

	if (connect_address.isIPv6() && !g_settings->getBool("enable_ipv6")) {
		*error_message = "Unable to connect to " +
				connect_address.serializeString() +
				" because IPv6 is disabled";
		errorstream << *error_message << std::endl;
		return false;
	}

	client = new Client(device,
			playername.c_str(), password, simple_singleplayer_mode,
			*draw_control, texture_src, shader_src,
			itemdef_manager, nodedef_manager, sound, eventmgr,
			connect_address.isIPv6());

	if (!client)
		return false;

	gamedef = client;	// Client acts as our GameDef


	infostream << "Connecting to server at ";
	connect_address.print(&infostream);
	infostream << std::endl;

	client->connect(connect_address);

	/*
		Wait for server to accept connection
	*/

	try {
		input->clear();

		FpsControl fps_control = { 0 };
		f32 dtime; // in seconds

		auto end_ms = porting::getTimeMs() + u32(CONNECTION_TIMEOUT * 1000);
		while (device->run()) {

			limitFps(&fps_control, &dtime);

			// Update client and server
			client->step(dtime);

			if (server != NULL)
				server->step(dtime);

			// End condition
			if (client->getState() == LC_Init) {
				*connect_ok = true;
				break;
			}

			// Break conditions
			if (client->accessDenied()) {
				*error_message = "Access denied. Reason: "
						+ client->accessDeniedReason();
				errorstream << *error_message << std::endl;
				break;
			}

			if (input->wasKeyDown(EscapeKey) || input->wasKeyDown(CancelKey)) {
				*aborted = true;
				infostream << "Connect aborted [Escape]" << std::endl;
				break;
			}

			// Update status
			showOverlayMessage("Connecting to server...", dtime, 100);

			if (porting::getTimeMs() > end_ms) {
				flags.reconnect = true;
				return false;
			}
		}
	} catch (con::PeerNotFoundException &e) {
		// TODO: Should something be done here? At least an info/error
		// message?
		return false;
	}

	return true;
}

bool Game::getServerContent(bool *aborted)
{
	input->clear();

	FpsControl fps_control = { 0 };
	f32 dtime; // in seconds

	int progress_old = 0;

	limitFps(&fps_control, &dtime);
	float time_counter = 0;
	auto dtime_start = dtime;

	while (device->run()) {

		limitFps(&fps_control, &dtime);

		// Update client and server
		client->step(dtime);

		if (server != NULL)
			server->step(dtime);

		// End condition
		if (client->mediaReceived() && client->itemdefReceived() &&
				client->nodedefReceived()) {
			break;
		}

		// Error conditions
		if (client->accessDenied()) {
			*error_message = "Access denied. Reason: "
					+ client->accessDeniedReason();
			errorstream << *error_message << std::endl;
			return false;
		}

		if (client->getState() < LC_Init) {
			*error_message = "Client disconnected";
			errorstream << *error_message << std::endl;
			return false;
		}

		if (input->wasKeyDown(EscapeKey) || input->wasKeyDown(CancelKey)) {
			*aborted = true;
			infostream << "Connect aborted [Escape]" << std::endl;
			return false;
		}

		// Display status
		int progress = 0;

		if (!client->itemdefReceived()) {
			wchar_t *text = wgettext("Item definitions...");
			progress = 0;
			draw_load_screen(text, device, guienv, dtime, progress);
			delete[] text;
		} else if (!client->nodedefReceived()) {
			wchar_t *text = wgettext("Node definitions...");
			progress = 25;
			draw_load_screen(text, device, guienv, dtime, progress);
			delete[] text;
		} else {
			std::stringstream message;
			message.precision(3);
			message << _("Media...");

			if ((USE_CURL == 0) ||
					(!g_settings->getBool("enable_remote_media_server"))) {
				float cur = client->getCurRate();
				std::string cur_unit = _(" KB/s");

				if (cur > 900) {
					cur /= 1024.0;
					cur_unit = _(" MB/s");
				}

				message << " ( " << cur << cur_unit << " )";
			}

			progress = 50 + client->mediaReceiveProgress() * 50 + 0.5;
			draw_load_screen(narrow_to_wide(message.str().c_str()), device,
					guienv, dtime, progress);
		}

		if (progress_old != progress) {
			progress_old = progress;
			time_counter = 0;
		}
		time_counter += dtime < dtime_start ? dtime : dtime - dtime_start;
		if (time_counter > CONNECTION_TIMEOUT) {
			flags.reconnect = 1;
			*aborted = true;
			return false;
		}

	}

	return true;
}



/****************************************************************************
 Run
 ****************************************************************************/

inline void Game::updateInteractTimers(GameRunData *args, f32 dtime)
{
	if (args->nodig_delay_timer >= 0)
		args->nodig_delay_timer -= dtime;

	if (args->object_hit_delay_timer >= 0)
		args->object_hit_delay_timer -= dtime;

	args->time_from_last_punch += dtime;
}


/* returns false if game should exit, otherwise true
 */
inline bool Game::checkConnection()
{
	if (client->accessDenied()) {
		*error_message = "Access denied. Reason: "
				+ client->accessDeniedReason();
		errorstream << *error_message << std::endl;
		return false;
	}

	if (client->m_con.Connected()) {
		flags.connected = 1;
	} else if (flags.connected) {
		flags.reconnect = 1;
		return false;
	}

	return true;
}


/* returns false if game should exit, otherwise true
 */
inline bool Game::handleCallbacks()
{
	if (g_gamecallback->disconnect_requested) {
		g_gamecallback->disconnect_requested = false;
		return false;
	}

	if (g_gamecallback->changepassword_requested) {
		(new GUIPasswordChange(guienv, guiroot, -1,
				       &g_menumgr, client))->drop();
		g_gamecallback->changepassword_requested = false;
	}

	if (g_gamecallback->changevolume_requested) {
		(new GUIVolumeChange(guienv, guiroot, -1,
				     &g_menumgr, client))->drop();
		g_gamecallback->changevolume_requested = false;
	}

	if (g_gamecallback->keyconfig_requested) {
		(new GUIKeyChangeMenu(guienv, guiroot, -1,
				      &g_menumgr))->drop();
		g_gamecallback->keyconfig_requested = false;
	}

	if (g_gamecallback->keyconfig_changed) {
		keycache.populate(); // update the cache with new settings
		g_gamecallback->keyconfig_changed = false;
	}

	return true;
}


void Game::processQueues()
{
	if (!flags.no_output)
	texture_src->processQueue();
	itemdef_manager->processQueue(gamedef);
	if (!flags.no_output)
	shader_src->processQueue();
}


void Game::updateProfilers(const GameRunData &run_data, const RunStats &stats,
		const FpsControl &draw_times, f32 dtime)
{
	float profiler_print_interval =
			g_settings->getFloat("profiler_print_interval");
	bool print_to_log = true;

	if (profiler_print_interval == 0) {
		print_to_log = false;
		profiler_print_interval = 5;
	}

	if (!run_data.autoexit)
	if (profiler_interval.step(dtime, profiler_print_interval)) {
		if (print_to_log) {
			infostream << "Profiler:" << std::endl;
			g_profiler->print(infostream);
		}

		update_profiler_gui(guitext_profiler, g_fontengine,
				run_data.profiler_current_page, run_data.profiler_max_page,
				driver->getScreenSize().Height);

		g_profiler->clear();
	}

	addProfilerGraphs(stats, draw_times, dtime);
}


void Game::addProfilerGraphs(const RunStats &stats,
		const FpsControl &draw_times, f32 dtime)
{
	g_profiler->graphAdd("mainloop_other",
			draw_times.busy_time / 1000.0f - stats.drawtime / 1000.0f);

	if (draw_times.sleep_time != 0)
		g_profiler->graphAdd("mainloop_sleep", draw_times.sleep_time / 1000.0f);
	g_profiler->graphAdd("mainloop_dtime", dtime);

	g_profiler->add("Elapsed time", dtime);
	g_profiler->avg("FPS", 1. / dtime);
}


void Game::updateStats(RunStats *stats, const FpsControl &draw_times,
		f32 dtime)
{

	f32 jitter;
	Jitter *jp;

	/* Time average and jitter calculation
	 */
	jp = &stats->dtime_jitter;
	jp->avg = jp->avg * 0.96 + dtime * 0.04;

	jitter = dtime - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;

	jp->counter += dtime;

	if (jp->counter > 0.0) {
		jp->counter -= 3.0;
		jp->max_sample = jp->max;
		jp->max_fraction = jp->max_sample / (jp->avg + 0.001);
		jp->max = 0.0;
	}

	/* Busytime average and jitter calculation
	 */
	jp = &stats->busy_time_jitter;
	jp->avg = jp->avg + draw_times.busy_time * 0.02;

	jitter = draw_times.busy_time - jp->avg;

	if (jitter > jp->max)
		jp->max = jitter;
	if (jitter < jp->min)
		jp->min = jitter;

	jp->counter += dtime;

	if (jp->counter > 0.0) {
		jp->counter -= 3.0;
		jp->max_sample = jp->max;
		jp->min_sample = jp->min;
		jp->max = 0.0;
		jp->min = 0.0;
	}

}



/****************************************************************************
 Input handling
 ****************************************************************************/

void Game::processUserInput(VolatileRunFlags *flags,
		GameRunData *interact_args, f32 dtime)
{
	// Reset input if window not active or some menu is active
	if (device->isWindowActive() == false
			|| noMenuActive() == false
			|| guienv->hasFocus(gui_chat_console)) {
		input->clear();
	}

	if (!guienv->hasFocus(gui_chat_console) && gui_chat_console->isOpen()) {
		gui_chat_console->closeConsoleAtOnce();
	}

	// Input handler step() (used by the random input generator)
	input->step(dtime);

#ifdef HAVE_TOUCHSCREENGUI

	if (g_touchscreengui) {
		g_touchscreengui->step(dtime);
	}

#endif
#ifdef __ANDROID__

	if (current_formspec != 0)
		current_formspec->getAndroidUIInput();

#endif

	// Increase timer for double tap of "keymap_jump"
	if (m_cache_doubletap_jump && interact_args->jump_timer <= 0.2)
		interact_args->jump_timer += dtime;

	processKeyboardInput(
			flags,
			&interact_args->statustext_time,
			&interact_args->jump_timer,
			&interact_args->reset_jump_timer,
			&interact_args->profiler_current_page,
			interact_args->profiler_max_page);

	processItemSelection(&interact_args->new_playeritem);
}


void Game::processKeyboardInput(VolatileRunFlags *flags,
		float *statustext_time,
		float *jump_timer,
		bool *reset_jump_timer,
		u32 *profiler_current_page,
		u32 profiler_max_page)
{

	//TimeTaker tt("process kybd input", NULL, PRECISION_NANO);

	if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_DROP])) {
		dropSelectedItem();
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_INVENTORY])) {
		openInventory();
	} else if (input->wasKeyDown(EscapeKey) || input->wasKeyDown(CancelKey)) {
		show_pause_menu(&current_formspec, client, gamedef, texture_src, device,
				simple_singleplayer_mode);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_CHAT])) {
		openConsole(0.1, true);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_CMD])) {
		openConsole(0.1, true, L"/");
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_MSG])) {
		openConsole(0.1, true, L"/msg ");
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_CONSOLE])) {
		openConsole();
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_FREEMOVE])) {
		toggleFreeMove(statustext_time);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_JUMP])) {
		toggleFreeMoveAlt(statustext_time, jump_timer);
		*reset_jump_timer = true;
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_FASTMOVE])) {
		toggleFast(statustext_time);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_NOCLIP])) {
		toggleNoClip(statustext_time);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_SCREENSHOT])) {
		client->makeScreenshot(device);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_TOGGLE_HUD])) {
		toggleHud(statustext_time, &flags->show_hud);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_TOGGLE_CHAT])) {
		toggleChat(statustext_time, &flags->show_chat);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_TOGGLE_FORCE_FOG_OFF])) {
		toggleFog(statustext_time, &flags->force_fog_off);
/*
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_TOGGLE_UPDATE_CAMERA])) {
		toggleUpdateCamera(statustext_time, &flags->disable_camera_update);
*/
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_TOGGLE_DEBUG])) {
		toggleDebug(statustext_time, &flags->show_debug, &flags->show_profiler_graph);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_TOGGLE_PROFILER])) {
		toggleProfiler(statustext_time, profiler_current_page, profiler_max_page);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_INCREASE_VIEWING_RANGE])) {
		increaseViewRange(statustext_time);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_DECREASE_VIEWING_RANGE])) {
		decreaseViewRange(statustext_time);
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_RANGESELECT])) {
		toggleFullViewRange(statustext_time);
		client->sendDrawControl();
	} else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_QUICKTUNE_NEXT]))
		quicktune->next();
	else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_QUICKTUNE_PREV]))
		quicktune->prev();
	else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_QUICKTUNE_INC]))
		quicktune->inc();
	else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_QUICKTUNE_DEC]))
		quicktune->dec();
	else if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_DEBUG_STACKS])) {
		// Print debug stacks
		dstream << "-----------------------------------------"
		        << std::endl;
		dstream << DTIME << "Printing debug stacks:" << std::endl;
		dstream << "-----------------------------------------"
		        << std::endl;
		debug_stacks_print();
	}

	//freeminer
#if !defined(NDEBUG)
	if (input->wasKeyDown(getKeySetting("keymap_toggle_block_boundaries"))) {
		toggleBlockBoundaries(statustext_time, flags);
	}
#endif

		if (playerlist)
			playerlist->setSelected(-1);
		if(!input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_PLAYERLIST]) && playerlist != NULL)
		{
			playerlist->remove();
			playerlist = NULL;
		}
		if(input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_PLAYERLIST]) && playerlist == NULL)
		{
			v2u32 screensize = driver->getScreenSize();
			std::list<std::string> players_list = client->getEnv().getPlayerNames();
			std::vector<std::string> players;
			players.reserve(players_list.size());
			std::copy(players_list.begin(), players_list.end(), std::back_inserter(players));
			std::sort(players.begin(), players.end(), string_icompare);

			u32 max_height = screensize.Y * 0.7;

			u32 row_height = g_fontengine->getTextHeight() + 4;
			u32 rows = max_height / row_height;
			u32 columns = players.size() / rows;
			if (players.size() % rows > 0)
				++columns;
			u32 actual_height = row_height * rows;
			if (rows > players.size())
				actual_height = row_height * players.size();
			u32 max_width = 0;
			for (size_t i = 0; i < players.size(); ++i)
				max_width = std::max(max_width, g_fontengine->getTextWidth(utf8_to_wide(players[i]).c_str()));
			max_width += 15;
			u32 actual_width = columns * max_width;

			if (columns != 0) {
				u32 x = (screensize.X - actual_width) / 2;
				u32 y = (screensize.Y - actual_height) / 2;
				playerlist = new GUITable(guienv, guienv->getRootGUIElement(), -1, core::rect<s32>(x, y, x + actual_width, y + actual_height), texture_src);
				playerlist->drop();
				playerlist->setScrollBarEnabled(false);
				GUITable::TableOptions table_options;
				GUITable::TableColumns table_columns;
				for (size_t i = 0; i < columns; ++i) {
					GUITable::TableColumn col;
					col.type = "text";
					table_columns.push_back(col);
				}
				std::vector<std::string> players_ordered;
				players_ordered.reserve(columns * rows);
				for (size_t i = 0; i < rows; ++i)
					for (size_t j = 0; j < columns; ++j) {
						size_t index = j * rows + i;
						if (index >= players.size())
							players_ordered.push_back("");
						else
							players_ordered.push_back(players[index]);
					}
				playerlist->setTable(table_options, table_columns, players_ordered);
			}
		}

	if (!input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_JUMP]) && *reset_jump_timer) {
		*reset_jump_timer = false;
		*jump_timer = 0.0;
	}

	//tt.stop();

	if (quicktune->hasMessage()) {
		std::string msg = quicktune->getMessage();
		statustext = narrow_to_wide(msg);
		*statustext_time = 0;
	}
}


void Game::processItemSelection(u16 *new_playeritem)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	/* Item selection using mouse wheel
	 */
	*new_playeritem = client->getPlayerItem();

	s32 wheel = input->getMouseWheel();
	u16 max_item = MYMIN(PLAYER_INVENTORY_SIZE - 1,
		                 player->hud_hotbar_itemcount - 1);

	if (wheel < 0)
		*new_playeritem = *new_playeritem < max_item ? *new_playeritem + 1 : 0;
	else if (wheel > 0)
		*new_playeritem = *new_playeritem > 0 ? *new_playeritem - 1 : max_item;
	// else wheel == 0


	/* Item selection using keyboard
	 */
	for (u16 i = 0; i < 10; i++) {
		static const KeyPress *item_keys[10] = {
			NumberKey + 1, NumberKey + 2, NumberKey + 3, NumberKey + 4,
			NumberKey + 5, NumberKey + 6, NumberKey + 7, NumberKey + 8,
			NumberKey + 9, NumberKey + 0,
		};

		if (input->wasKeyDown(*item_keys[i])) {
			if (i < PLAYER_INVENTORY_SIZE && i < player->hud_hotbar_itemcount) {
				if (*new_playeritem == i && g_settings->getBool("hotbar_cycling"))
					*new_playeritem = client->getPreviousPlayerItem();
				else
					*new_playeritem = i;
				infostream << "Selected item: " << new_playeritem << std::endl;
			}
			break;
		}
	}
}


void Game::dropSelectedItem()
{
	IDropAction *a = new IDropAction();
	a->count = 0;
	a->from_inv.setCurrentPlayer();
	a->from_list = "main";
	a->from_i = client->getPlayerItem();
	client->inventoryAction(a);
}


void Game::openInventory()
{
	infostream << "the_game: " << "Launching inventory" << std::endl;

	PlayerInventoryFormSource *fs_src = new PlayerInventoryFormSource(client);
	TextDest *txt_dst = new TextDestPlayerInventory(client);

	create_formspec_menu(&current_formspec, client, gamedef, texture_src,
			device, fs_src, txt_dst, client);

	InventoryLocation inventoryloc;
	inventoryloc.setCurrentPlayer();
	current_formspec->setFormSpec(fs_src->getForm(), inventoryloc);
}


void Game::openConsole(float height, bool close_on_return, const std::wstring& input)
{
	if (!gui_chat_console->isOpenInhibited()) {
		// Set initial console prompt
		if (!input.empty()) {
			gui_chat_console->setPrompt(input);
		}
		gui_chat_console->openConsole(height, close_on_return);
		guienv->setFocus(gui_chat_console);
	}
}


void Game::toggleFreeMove(float *statustext_time)
{
	static const wchar_t *msg[] = { L"free_move disabled", L"free_move enabled" };

	bool free_move = !g_settings->getBool("free_move");
	g_settings->set("free_move", bool_to_cstr(free_move));

	*statustext_time = 0;
	statustext = msg[free_move];
	if (free_move && !client->checkPrivilege("fly"))
		statustext += L" (note: no 'fly' privilege)";
}


void Game::toggleFreeMoveAlt(float *statustext_time, float *jump_timer)
{
	if (m_cache_doubletap_jump && *jump_timer < 0.2f)
		toggleFreeMove(statustext_time);
}


void Game::toggleFast(float *statustext_time)
{
	static const wchar_t *msg[] = { L"fast_move disabled", L"fast_move enabled" };
	bool fast_move = !g_settings->getBool("fast_move");
	g_settings->set("fast_move", bool_to_cstr(fast_move));

	*statustext_time = 0;
	statustext = msg[fast_move];

	if (fast_move && !client->checkPrivilege("fast"))
		statustext += L" (note: no 'fast' privilege)";
}


void Game::toggleNoClip(float *statustext_time)
{
	static const wchar_t *msg[] = { L"noclip disabled", L"noclip enabled" };
	bool noclip = !g_settings->getBool("noclip");
	g_settings->set("noclip", bool_to_cstr(noclip));

	*statustext_time = 0;
	statustext = msg[noclip];

	if (noclip && !client->checkPrivilege("noclip"))
		statustext += L" (note: no 'noclip' privilege)";
}


void Game::toggleChat(float *statustext_time, bool *flag)
{
	static const wchar_t *msg[] = { L"Chat hidden", L"Chat shown" };

	*flag = !*flag;
	*statustext_time = 0;
	statustext = msg[*flag];
}


void Game::toggleHud(float *statustext_time, bool *flag)
{
	static const wchar_t *msg[] = { L"HUD hidden", L"HUD shown" };

	*flag = !*flag;
	*statustext_time = 0;
	statustext = msg[*flag];
	if (g_settings->getBool("enable_node_highlighting"))
		client->setHighlighted(client->getHighlighted(), *flag);
}


void Game::toggleFog(float *statustext_time, bool *flag)
{
	static const wchar_t *msg[] = { L"Fog enabled", L"Fog disabled" };

	*flag = !*flag;
	*statustext_time = 0;
	statustext = msg[*flag];
}


void Game::toggleDebug(float *statustext_time, bool *show_debug,
		bool *show_profiler_graph)
{
	// Initial / 3x toggle: Chat only
	// 1x toggle: Debug text with chat
	// 2x toggle: Debug text with profiler graph
	if (!*show_debug) {
		*show_debug = true;
		*show_profiler_graph = false;
		statustext = L"Debug info shown";
	} else if (*show_profiler_graph) {
		*show_debug = false;
		*show_profiler_graph = false;
		statustext = L"Debug info and profiler graph hidden";
	} else {
		*show_profiler_graph = true;
		statustext = L"Profiler graph shown";
	}
	*statustext_time = 0;
}


void Game::toggleUpdateCamera(float *statustext_time, bool *flag)
{
	static const wchar_t *msg[] = {
		L"Camera update enabled",
		L"Camera update disabled"
	};

	*flag = !*flag;
	*statustext_time = 0;
	statustext = msg[*flag];
}


void Game::toggleBlockBoundaries(float *statustext_time, VolatileRunFlags *flags) {
	static const wchar_t *msg[] = {
		L"Block boundaries shown",
		L"Block boundaries hidden"
	};
	flags->show_block_boundaries = !flags->show_block_boundaries;
	*statustext_time = 0;
	statustext = msg[flags->show_block_boundaries];
}


void Game::toggleProfiler(float *statustext_time, u32 *profiler_current_page,
		u32 profiler_max_page)
{
	*profiler_current_page = (*profiler_current_page + 1) % (profiler_max_page + 1);

	// FIXME: This updates the profiler with incomplete values
	update_profiler_gui(guitext_profiler, g_fontengine, *profiler_current_page,
			profiler_max_page, driver->getScreenSize().Height);

	if (*profiler_current_page != 0) {
		std::wstringstream sstr;
		sstr << "Profiler shown (page " << *profiler_current_page
		     << " of " << profiler_max_page << ")";
		statustext = sstr.str();
		if (*profiler_current_page == 1)
			runData.profiler_state = g_profiler_enabled;
		g_profiler_enabled = true;
	} else {
		statustext = L"Profiler hidden";
		g_profiler_enabled = runData.profiler_state;
	}

	*statustext_time = 0;
}


void Game::increaseViewRange(float *statustext_time)
{
	s16 range = g_settings->getS16("viewing_range_nodes_min");
	s16 range_new = range + 10;
	g_settings->set("viewing_range_nodes_min", itos(range_new));
	statustext = narrow_to_wide("Minimum viewing range changed to "
			+ itos(range_new));
	*statustext_time = 0;
}


void Game::decreaseViewRange(float *statustext_time)
{
	s16 range = g_settings->getS16("viewing_range_nodes_min");
	s16 range_new = range - 10;

	if (range_new < 0)
		range_new = range;

	g_settings->set("viewing_range_nodes_min", itos(range_new));
	statustext = narrow_to_wide("Minimum viewing range changed to "
			+ itos(range_new));
	*statustext_time = 0;
}


void Game::toggleFullViewRange(float *statustext_time)
{
	static const wchar_t *msg[] = {
		L"Disabled full viewing range",
		L"Enabled full viewing range"
	};

	draw_control->range_all = !draw_control->range_all;
	infostream << msg[draw_control->range_all] << std::endl;
	statustext = msg[draw_control->range_all];
	*statustext_time = 0;
}


void Game::updateCameraDirection(CameraOrientation *cam,
		VolatileRunFlags *flags)
{
	// float turn_amount = 0;	// Deprecated?

	if (!(device->isWindowActive() && noMenuActive()) || random_input) {

	// FIXME: Clean this up

#ifndef ANDROID
		// Mac OSX gets upset if this is set every frame
		if (device->getCursorControl()->isVisible() == false)
			device->getCursorControl()->setVisible(true);
#endif

		//infostream<<"window inactive"<<std::endl;
		flags->first_loop_after_window_activation = true;
		return;
	}

#ifndef __ANDROID__
	if (!random_input) {
		// Mac OSX gets upset if this is set every frame
		if (device->getCursorControl()->isVisible())
			device->getCursorControl()->setVisible(false);
	}
#endif

	if (flags->first_loop_after_window_activation) {
		//infostream<<"window active, first loop"<<std::endl;
		flags->first_loop_after_window_activation = false;
	} else {

#ifdef HAVE_TOUCHSCREENGUI

		if (g_touchscreengui) {
			cam->camera_yaw   = g_touchscreengui->getYaw();
			cam->camera_pitch = g_touchscreengui->getPitch();
		} else {
#endif
			s32 dx = input->getMousePos().X - (driver->getScreenSize().Width / 2);
			s32 dy = input->getMousePos().Y - (driver->getScreenSize().Height / 2);

			if (flags->invert_mouse
					|| (camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT)) {
				dy = -dy;
			}

			//infostream<<"window active, pos difference "<<dx<<","<<dy<<std::endl;

			float d = m_cache_mouse_sensitivity;
			d = rangelim(d, 0.01, 100.0);
			cam->camera_yaw -= dx * d;
			cam->camera_pitch += dy * d;
			// turn_amount = v2f(dx, dy).getLength() * d; // deprecated?

#ifdef HAVE_TOUCHSCREENGUI
			}
#endif

		if (cam->camera_pitch < -89.5)
			cam->camera_pitch = -89.5;
		else if (cam->camera_pitch > 89.5)
			cam->camera_pitch = 89.5;
	}

	input->setMousePos(driver->getScreenSize().Width / 2,
			driver->getScreenSize().Height / 2);

	// Deprecated? Not used anywhere else
	// recent_turn_speed = recent_turn_speed * 0.9 + turn_amount * 0.1;
	// std::cerr<<"recent_turn_speed = "<<recent_turn_speed<<std::endl;
}


void Game::updatePlayerControl(const CameraOrientation &cam)
{
	//TimeTaker tt("update player control", NULL, PRECISION_NANO);

	PlayerControl control(
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_FORWARD]),
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_BACKWARD]),
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_LEFT]),
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_RIGHT]),
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_JUMP]),
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_SPECIAL1]),
		input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_SNEAK]),
		input->getLeftState(),
		input->getRightState(),
		cam.camera_pitch,
		cam.camera_yaw
	);
	client->setPlayerControl(control);
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	player->keyPressed =
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_FORWARD])  & 0x1) << 0) |
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_BACKWARD]) & 0x1) << 1) |
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_LEFT])     & 0x1) << 2) |
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_RIGHT])    & 0x1) << 3) |
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_JUMP])     & 0x1) << 4) |
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_SPECIAL1]) & 0x1) << 5) |
		( (u32)(input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_SNEAK])    & 0x1) << 6) |
		( (u32)(input->getLeftState()                                        & 0x1) << 7) |
		( (u32)(input->getRightState()                                       & 0x1) << 8
	);

	auto & draw_control = client->getEnv().getClientMap().getControl();
	if (input->isKeyDown(keycache.key[KeyCache::KEYMAP_ID_ZOOM])) {
		bool changed = player->zoom == false;
		player->zoom = true;
		if (changed) {
			draw_control.fov = g_settings->getFloat("zoom_fov");
			client->sendDrawControl();
		}
	} else {
		bool changed = player->zoom == true;
		player->zoom = false;
		if (changed) {
			draw_control.fov = g_settings->getFloat("fov");
			client->sendDrawControl();
		}
	}

	//tt.stop();
}


inline void Game::step(f32 *dtime)
{
	bool can_be_and_is_paused =
			(simple_singleplayer_mode && g_menumgr.pausesGame());

	if (can_be_and_is_paused) {	// This is for a singleplayer server
		*dtime = 0;             // No time passes
	} else {
		if (server != NULL) {
			//TimeTaker timer("server->step(dtime)");
			try {
			server->step(*dtime);
			} catch(std::exception &e) {
				if (!flags.errors++ || !(flags.errors % (int)(60/flags.dedicated_server_step)))
					errorstream << "Fatal error n=" << flags.errors << " : " << e.what() << std::endl;
			}
		}

		//TimeTaker timer("client.step(dtime)");
		client->step(*dtime);
	}
}


void Game::processClientEvents(CameraOrientation *cam, float *damage_flash)
{
	ClientEvent event = client->getClientEvent();

	LocalPlayer *player = client->getEnv().getLocalPlayer();

	for ( ; event.type != CE_NONE; event = client->getClientEvent()) {

		if (event.type == CE_PLAYER_DAMAGE &&
				client->getHP() != 0) {
			//u16 damage = event.player_damage.amount;
			//infostream<<"Player damage: "<<damage<<std::endl;

			*damage_flash += 100.0;
			*damage_flash += 8.0 * event.player_damage.amount;

			player->hurt_tilt_timer = 1.5;
			player->hurt_tilt_strength = event.player_damage.amount / 4;
			player->hurt_tilt_strength = rangelim(player->hurt_tilt_strength, 1.0, 4.0);

			MtEvent *e = new SimpleTriggerEvent("PlayerDamage");
			gamedef->event()->put(e);
		} else if (event.type == CE_PLAYER_FORCE_MOVE) {
			cam->camera_yaw = event.player_force_move.yaw;
			cam->camera_pitch = event.player_force_move.pitch;
		} else if (event.type == CE_DEATHSCREEN) {
			if (g_settings->getBool("respawn_auto")) {
				client->sendRespawn();
			} else {

			show_deathscreen(&current_formspec, client, gamedef, texture_src,
					 device, client);
			}

			/* Handle visualization */
			*damage_flash = 0;
			player->hurt_tilt_timer = 0;
			player->hurt_tilt_strength = 0;

		} else if (event.type == CE_SHOW_FORMSPEC) {
			FormspecFormSource *fs_src =
				new FormspecFormSource(*(event.show_formspec.formspec));
			TextDestPlayerInventory *txt_dst =
				new TextDestPlayerInventory(client, *(event.show_formspec.formname));

			create_formspec_menu(&current_formspec, client, gamedef,
					     texture_src, device, fs_src, txt_dst, client);

			delete(event.show_formspec.formspec);
			delete(event.show_formspec.formname);
		} else if (event.type == CE_SPAWN_PARTICLE) {
			video::ITexture *texture =
				gamedef->tsrc()->getTexture(*(event.spawn_particle.texture));

			new Particle(gamedef, smgr, player, client->getEnv(),
					*event.spawn_particle.pos,
					*event.spawn_particle.vel,
					*event.spawn_particle.acc,
					event.spawn_particle.expirationtime,
					event.spawn_particle.size,
					event.spawn_particle.collisiondetection,
					event.spawn_particle.vertical,
					texture,
					v2f(0.0, 0.0),
					v2f(1.0, 1.0));
		} else if (event.type == CE_ADD_PARTICLESPAWNER) {
			video::ITexture *texture =
				gamedef->tsrc()->getTexture(*(event.add_particlespawner.texture));

			new ParticleSpawner(gamedef, smgr, player,
					event.add_particlespawner.amount,
					event.add_particlespawner.spawntime,
					*event.add_particlespawner.minpos,
					*event.add_particlespawner.maxpos,
					*event.add_particlespawner.minvel,
					*event.add_particlespawner.maxvel,
					*event.add_particlespawner.minacc,
					*event.add_particlespawner.maxacc,
					event.add_particlespawner.minexptime,
					event.add_particlespawner.maxexptime,
					event.add_particlespawner.minsize,
					event.add_particlespawner.maxsize,
					event.add_particlespawner.collisiondetection,
					event.add_particlespawner.vertical,
					texture,
					event.add_particlespawner.id);
		} else if (event.type == CE_DELETE_PARTICLESPAWNER) {
			delete_particlespawner(event.delete_particlespawner.id);
		} else if (event.type == CE_HUDADD) {
			u32 id = event.hudadd.id;

			LocalPlayer *player = client->getEnv().getLocalPlayer();
			HudElement *e = player->getHud(id);

			if (e != NULL) {
				delete event.hudadd.pos;
				delete event.hudadd.name;
				delete event.hudadd.scale;
				delete event.hudadd.text;
				delete event.hudadd.align;
				delete event.hudadd.offset;
				delete event.hudadd.world_pos;
				delete event.hudadd.size;
				continue;
			}

			e = new HudElement;
			e->type   = (HudElementType)event.hudadd.type;
			e->pos    = *event.hudadd.pos;
			e->name   = *event.hudadd.name;
			e->scale  = *event.hudadd.scale;
			e->text   = *event.hudadd.text;
			e->number = event.hudadd.number;
			e->item   = event.hudadd.item;
			e->dir    = event.hudadd.dir;
			e->align  = *event.hudadd.align;
			e->offset = *event.hudadd.offset;
			e->world_pos = *event.hudadd.world_pos;
			e->size = *event.hudadd.size;

			player->addHud(e);
/*
			//if this isn't true our huds aren't consistent
			assert(new_id == id);
*/

			delete event.hudadd.pos;
			delete event.hudadd.name;
			delete event.hudadd.scale;
			delete event.hudadd.text;
			delete event.hudadd.align;
			delete event.hudadd.offset;
			delete event.hudadd.world_pos;
			delete event.hudadd.size;
		} else if (event.type == CE_HUDRM) {
			HudElement *e = player->removeHud(event.hudrm.id);

			if (e != NULL)
				delete(e);
		} else if (event.type == CE_HUDCHANGE) {
			u32 id = event.hudchange.id;
			HudElement *e = player->getHud(id);

			if (e == NULL) {
				delete event.hudchange.v3fdata;
				delete event.hudchange.v2fdata;
				delete event.hudchange.sdata;
				delete event.hudchange.v2s32data;
				continue;
			}

			switch (event.hudchange.stat) {
			case HUD_STAT_POS:
				e->pos = *event.hudchange.v2fdata;
				break;

			case HUD_STAT_NAME:
				e->name = *event.hudchange.sdata;
				break;

			case HUD_STAT_SCALE:
				e->scale = *event.hudchange.v2fdata;
				break;

			case HUD_STAT_TEXT:
				e->text = *event.hudchange.sdata;
				break;

			case HUD_STAT_NUMBER:
				e->number = event.hudchange.data;
				break;

			case HUD_STAT_ITEM:
				e->item = event.hudchange.data;
				break;

			case HUD_STAT_DIR:
				e->dir = event.hudchange.data;
				break;

			case HUD_STAT_ALIGN:
				e->align = *event.hudchange.v2fdata;
				break;

			case HUD_STAT_OFFSET:
				e->offset = *event.hudchange.v2fdata;
				break;

			case HUD_STAT_WORLD_POS:
				e->world_pos = *event.hudchange.v3fdata;
				break;

			case HUD_STAT_SIZE:
				e->size = *event.hudchange.v2s32data;
				break;
			}

			delete event.hudchange.v3fdata;
			delete event.hudchange.v2fdata;
			delete event.hudchange.sdata;
			delete event.hudchange.v2s32data;
		} else if (event.type == CE_SET_SKY) {
			sky->setVisible(false);

			if (skybox) {
				skybox->remove();
				skybox = NULL;
			}

			// Handle according to type
			if (*event.set_sky.type == "regular") {
				sky->setVisible(true);
			} else if (*event.set_sky.type == "skybox" &&
					event.set_sky.params->size() == 6) {
				sky->setFallbackBgColor(*event.set_sky.bgcolor);
				skybox = smgr->addSkyBoxSceneNode(
						 texture_src->getTexture((*event.set_sky.params)[0]),
						 texture_src->getTexture((*event.set_sky.params)[1]),
						 texture_src->getTexture((*event.set_sky.params)[2]),
						 texture_src->getTexture((*event.set_sky.params)[3]),
						 texture_src->getTexture((*event.set_sky.params)[4]),
						 texture_src->getTexture((*event.set_sky.params)[5]));
			}
			// Handle everything else as plain color
			else {
				if (*event.set_sky.type != "plain")
					infostream << "Unknown sky type: "
						   << (*event.set_sky.type) << std::endl;

				sky->setFallbackBgColor(*event.set_sky.bgcolor);
			}

			delete event.set_sky.bgcolor;
			delete event.set_sky.type;
			delete event.set_sky.params;
		} else if (event.type == CE_OVERRIDE_DAY_NIGHT_RATIO) {
			bool enable = event.override_day_night_ratio.do_override;
			u32 value = event.override_day_night_ratio.ratio_f * 1000;
			client->getEnv().setDayNightRatioOverride(enable, value);
		}
	}
}


void Game::updateCamera(VolatileRunFlags *flags, u32 busy_time,
		f32 dtime, float time_from_last_punch)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	/*
		For interaction purposes, get info about the held item
		- What item is it?
		- Is it a usable item?
		- Can it point to liquids?
	*/
	ItemStack playeritem;
	{
		InventoryList *mlist = local_inventory->getList("main");

		if (mlist && client->getPlayerItem() < mlist->getSize())
			playeritem = mlist->getItem(client->getPlayerItem());
	}

	ToolCapabilities playeritem_toolcap =
		playeritem.getToolCapabilities(itemdef_manager);

	v3s16 old_camera_offset = camera->getOffset();

	if (input->wasKeyDown(keycache.key[KeyCache::KEYMAP_ID_CAMERA_MODE])) {
		camera->toggleCameraMode();
		GenericCAO *playercao = player->getCAO();

		assert(playercao != NULL);

		playercao->setVisible(camera->getCameraMode() > CAMERA_MODE_FIRST);
	}

	float full_punch_interval = playeritem_toolcap.full_punch_interval;
	float tool_reload_ratio = time_from_last_punch / full_punch_interval;

	tool_reload_ratio = MYMIN(tool_reload_ratio, 1.0);
	camera->update(player, dtime, busy_time / 1000.0f, tool_reload_ratio,
		      client->getEnv());
	camera->step(dtime);

	v3f camera_position = camera->getPosition();
	v3f camera_direction = camera->getDirection();
	f32 camera_fov = camera->getFovMax();
	v3s16 camera_offset = camera->getOffset();

	flags->camera_offset_changed = (camera_offset != old_camera_offset);

	if (!flags->disable_camera_update) {
		client->getEnv().getClientMap().updateCamera(camera_position,
				camera_direction, camera_fov, camera_offset);

		if (flags->camera_offset_changed) {
			client->updateCameraOffset(camera_offset);
			client->getEnv().updateCameraOffset(camera_offset);

			if (clouds)
				clouds->updateCameraOffset(camera_offset);
			if (sky)
				sky->camera_offset = camera_offset;
		}
	}
}


void Game::updateSound(f32 dtime)
{
	// Update sound listener
	v3s16 camera_offset = camera->getOffset();
	sound->updateListener(camera->getCameraNode()->getPosition() + intToFloat(camera_offset, BS),
			      v3f(0, 0, 0), // velocity
			      camera->getDirection(),
			      camera->getCameraNode()->getUpVector());
	sound->setListenerGain(g_settings->getFloat("sound_volume"));


	//	Update sound maker
	soundmaker->step(dtime);

	LocalPlayer *player = client->getEnv().getLocalPlayer();

	ClientMap &map = client->getEnv().getClientMap();
	MapNode n = map.getNodeNoEx(player->getStandingNodePos());
	soundmaker->m_player_step_sound = nodedef_manager->get(n).sound_footstep;
}


void Game::processPlayerInteraction(std::vector<aabb3f> &highlight_boxes,
		GameRunData *runData, f32 dtime, bool show_hud, bool show_debug)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	ItemStack playeritem;
	{
		InventoryList *mlist = local_inventory->getList("main");

		if (mlist && client->getPlayerItem() < mlist->getSize())
			playeritem = mlist->getItem(client->getPlayerItem());
	}

	const ItemDefinition &playeritem_def =
			playeritem.getDefinition(itemdef_manager);

	v3f player_position  = player->getPosition();
	v3f camera_position  = camera->getPosition();
	v3f camera_direction = camera->getDirection();
	v3s16 camera_offset  = camera->getOffset();


	/*
		Calculate what block is the crosshair pointing to
	*/

	f32 d = playeritem_def.range; // max. distance
	f32 d_hand = itemdef_manager->get("").range;

	if (d < 0 && d_hand >= 0)
		d = d_hand;
	else if (d < 0)
		d = 4.0;

	core::line3d<f32> shootline;

	if (camera->getCameraMode() != CAMERA_MODE_THIRD_FRONT) {

		shootline = core::line3d<f32>(camera_position,
						camera_position + camera_direction * BS * (d + 1));

	} else {
	    // prevent player pointing anything in front-view
		if (camera->getCameraMode() == CAMERA_MODE_THIRD_FRONT)
			shootline = core::line3d<f32>(0, 0, 0, 0, 0, 0);
	}

#ifdef HAVE_TOUCHSCREENGUI

	if ((g_settings->getBool("touchtarget")) && (g_touchscreengui)) {
		shootline = g_touchscreengui->getShootline();
		shootline.start += intToFloat(camera_offset, BS);
		shootline.end += intToFloat(camera_offset, BS);
	}

#endif

	PointedThing pointed = getPointedThing(
			// input
			client, player_position, camera_direction,
			camera_position, shootline, d,
			playeritem_def.liquids_pointable,
			!runData->ldown_for_dig,
			camera_offset,
			// output
			highlight_boxes,
			runData->selected_object);

	if (pointed != runData->pointed_old) {
		infostream << "Pointing at " << pointed.dump() << std::endl;
/* node debug
			MapNode nu = client->getEnv().getClientMap().getNodeNoEx(pointed.node_undersurface);
			MapNode na = client->getEnv().getClientMap().getNodeNoEx(pointed.node_abovesurface);
			infostream	<< "|| nu0="<<(int)nu.param0<<" nu1"<<(int)nu.param1<<" nu2"<<(int)nu.param1<<"; nam="<<client->getNodeDefManager()->get(nu.getContent()).name
						<< "|| na0="<<(int)na.param0<<" na1"<<(int)na.param1<<" na2"<<(int)na.param1<<"; nam="<<client->getNodeDefManager()->get(na.getContent()).name
						<<std::endl;
*/

		if (m_cache_enable_node_highlighting) {
			if (pointed.type == POINTEDTHING_NODE) {
				client->setHighlighted(pointed.node_undersurface, show_hud);
			} else {
				client->setHighlighted(pointed.node_undersurface, false);
			}
		}
	}

	/*
		Stop digging when
		- releasing left mouse button
		- pointing away from node
	*/
	if (runData->digging) {
		if (input->getLeftReleased()) {
			infostream << "Left button released"
			           << " (stopped digging)" << std::endl;
			runData->digging = false;
		} else if (pointed != runData->pointed_old) {
			if (pointed.type == POINTEDTHING_NODE
					&& runData->pointed_old.type == POINTEDTHING_NODE
					&& pointed.node_undersurface
							== runData->pointed_old.node_undersurface) {
				// Still pointing to the same node, but a different face.
				// Don't reset.
			} else {
				infostream << "Pointing away from node"
				           << " (stopped digging)" << std::endl;
				runData->digging = false;
			}
		}

		if (!runData->digging) {
			client->interact(1, runData->pointed_old);
			client->setCrack(-1, v3s16(0, 0, 0));
			runData->dig_time = 0.0;
		}
	}

	if (!runData->digging && runData->ldown_for_dig && !input->getLeftState()) {
		runData->ldown_for_dig = false;
	}

	runData->left_punch = false;

	soundmaker->m_player_leftpunch_sound.name = "";

	if (input->getRightState())
		runData->repeat_rightclick_timer += dtime;
	else
		runData->repeat_rightclick_timer = 0;

	if (playeritem_def.usable && input->getLeftState()) {
		if (input->getLeftClicked())
			client->interact(4, pointed);
	} else if (pointed.type == POINTEDTHING_NODE) {
		ToolCapabilities playeritem_toolcap =
				playeritem.getToolCapabilities(itemdef_manager);
		handlePointingAtNode(runData, pointed, playeritem_def,
				playeritem_toolcap, dtime);
	} else if (pointed.type == POINTEDTHING_OBJECT) {
		handlePointingAtObject(runData, pointed, playeritem,
				player_position, show_debug);
	} else if (input->getLeftState()) {
		// When button is held down in air, show continuous animation
		runData->left_punch = true;
	}

	runData->pointed_old = pointed;

	if (runData->left_punch || input->getLeftClicked())
		camera->setDigging(0); // left click animation

	input->resetLeftClicked();
	input->resetRightClicked();

	input->resetLeftReleased();
	input->resetRightReleased();
}


void Game::handlePointingAtNode(GameRunData *runData,
		const PointedThing &pointed, const ItemDefinition &playeritem_def,
		const ToolCapabilities &playeritem_toolcap, f32 dtime)
{
	v3s16 nodepos = pointed.node_undersurface;
	v3s16 neighbourpos = pointed.node_abovesurface;

	/*
		Check information text of node
	*/

	ClientMap &map = client->getEnv().getClientMap();
	NodeMetadata *meta = map.getNodeMetadata(nodepos);

	if (meta) {
		infotext = narrow_to_wide(meta->getString("infotext"));
	} else {
		MapNode n = map.getNodeNoEx(nodepos);

		if (nodedef_manager->get(n).tiledef[0].name == "unknown_node.png") {
			infotext = L"Unknown node: ";
			infotext += narrow_to_wide(nodedef_manager->get(n).name);
		}
	}

	if (runData->nodig_delay_timer <= 0.0 && input->getLeftState()
			&& client->checkPrivilege("interact")) {
		handleDigging(runData, pointed, nodepos, playeritem_toolcap, dtime);
	}

	if ((input->getRightClicked() ||
			runData->repeat_rightclick_timer >= m_repeat_right_click_time) &&
			client->checkPrivilege("interact")) {
		runData->repeat_rightclick_timer = 0;
		infostream << "Ground right-clicked" << std::endl;

/*
				// Sign special case, at least until formspec is properly implemented.
				// Deprecated?
				if(meta && meta->getString("formspec") == "hack:sign_text_input"
						&& !random_input
						&& !input->isKeyDown(getKeySetting("keymap_sneak")))
				{
					infostream<<"Launching metadata text input"<<std::endl;

					// Get a new text for it

					TextDest *dest = new TextDestNodeMetadata(nodepos, client);

					std::wstring wtext = narrow_to_wide(meta->getString("text"));

					(new GUITextInputMenu(guienv, guiroot, -1,
							&g_menumgr, dest,
							wtext))->drop();
				}
				// If metadata provides an inventory view, activate it
				else
*/
				if(meta && meta->getString("formspec") != "" && !random_input
				&& !input->isKeyDown(getKeySetting("keymap_sneak"))) {
			infostream << "Launching custom inventory view" << std::endl;

			InventoryLocation inventoryloc;
			inventoryloc.setNodeMeta(nodepos);

			NodeMetadataFormSource *fs_src = new NodeMetadataFormSource(
				&client->getEnv().getClientMap(), nodepos);
			TextDest *txt_dst = new TextDestNodeMetadata(nodepos, client);

			create_formspec_menu(&current_formspec, client, gamedef,
					     texture_src, device, fs_src, txt_dst, client);

			current_formspec->setFormSpec(meta->getString("formspec"), inventoryloc);
		} else {
			// Report right click to server

			camera->setDigging(1);  // right click animation (always shown for feedback)

			// If the wielded item has node placement prediction,
			// make that happen
			bool placed = nodePlacementPrediction(*client,
					playeritem_def,
					nodepos, neighbourpos);

			if (placed) {
				// Report to server
				client->interact(3, pointed);
				// Read the sound
				soundmaker->m_player_rightpunch_sound =
						playeritem_def.sound_place;
			} else {
				soundmaker->m_player_rightpunch_sound =
						SimpleSoundSpec();
			}

			if (playeritem_def.node_placement_prediction == "" ||
					nodedef_manager->get(map.getNodeNoEx(nodepos)).rightclickable)
				client->interact(3, pointed); // Report to server
		}
	}
}


void Game::handlePointingAtObject(GameRunData *runData,
		const PointedThing &pointed,
		const ItemStack &playeritem,
		const v3f &player_position,
		bool show_debug)
{
	infotext = narrow_to_wide(runData->selected_object->infoText());

	if (infotext == L"" && show_debug) {
		infotext = narrow_to_wide(runData->selected_object->debugInfoText());
	}

	if (input->getLeftState()) {
		bool do_punch = false;
		bool do_punch_damage = false;

		if (runData->object_hit_delay_timer <= 0.0) {
			do_punch = true;
			do_punch_damage = true;
			runData->object_hit_delay_timer = object_hit_delay;
		}

		if (input->getLeftClicked())
			do_punch = true;

		if (do_punch) {
			infostream << "Left-clicked object" << std::endl;
			runData->left_punch = true;
		}

		if (do_punch_damage) {
			// Report direct punch
			v3f objpos = runData->selected_object->getPosition();
			v3f dir = (objpos - player_position).normalize();

			bool disable_send = runData->selected_object->directReportPunch(
					dir, &playeritem, runData->time_from_last_punch);
			runData->time_from_last_punch = 0;

			if (!disable_send)
				client->interact(0, pointed);
		}
	} else if (input->getRightClicked()) {
		infostream << "Right-clicked object" << std::endl;
		client->interact(3, pointed);  // place
	}
}


void Game::handleDigging(GameRunData *runData,
		const PointedThing &pointed, const v3s16 &nodepos,
		const ToolCapabilities &playeritem_toolcap, f32 dtime)
{
	if (!runData->digging) {
		infostream << "Started digging" << std::endl;
		client->interact(0, pointed);
		runData->digging = true;
		runData->ldown_for_dig = true;
	}

	LocalPlayer *player = client->getEnv().getLocalPlayer();
	ClientMap &map = client->getEnv().getClientMap();
	MapNode n = client->getEnv().getClientMap().getNodeNoEx(nodepos);
	const ContentFeatures &features = client->getNodeDefManager()->get(n);

	// NOTE: Similar piece of code exists on the server side for
	// cheat detection.
	// Get digging parameters
	DigParams params = getDigParams(nodedef_manager->get(n).groups,
			&playeritem_toolcap);

	// If can't dig, try hand
	if (!params.diggable) {
		const ItemDefinition &hand = itemdef_manager->get("");
		const ToolCapabilities *tp = hand.tool_capabilities;

		if (tp)
			params = getDigParams(nodedef_manager->get(n).groups, tp);
	}

	if (params.diggable == false) {
		// I guess nobody will wait for this long
		runData->dig_time_complete = 10000000.0;
	} else {
		runData->dig_time_complete = params.time;

		if (m_cache_enable_particles) {
			addPunchingParticles(gamedef, smgr, player,
					client->getEnv(), nodepos, features.tiles);
		}
	}

	if (runData->dig_time_complete >= 0.001) {
		runData->dig_index = (float)crack_animation_length
				* runData->dig_time
				/ runData->dig_time_complete;
	} else {
		// This is for torches
		runData->dig_index = crack_animation_length;
	}

	SimpleSoundSpec sound_dig = nodedef_manager->get(n).sound_dig;

	if (sound_dig.exists() && params.diggable) {
		if (sound_dig.name == "__group") {
			if (params.main_group != "") {
				soundmaker->m_player_leftpunch_sound.gain = 0.5;
				soundmaker->m_player_leftpunch_sound.name =
						std::string("default_dig_") +
						params.main_group;
			}
		} else {
			soundmaker->m_player_leftpunch_sound = sound_dig;
		}
	}

	// Don't show cracks if not diggable
	if (runData->dig_time_complete >= 100000.0) {
	} else if (runData->dig_index < crack_animation_length) {
		//TimeTaker timer("client.setTempMod");
		//infostream<<"dig_index="<<dig_index<<std::endl;
		client->setCrack(runData->dig_index, nodepos);
	} else {
		infostream << "Digging completed" << std::endl;
		client->interact(2, pointed);
		client->setCrack(-1, v3s16(0, 0, 0));
		bool is_valid_position;
		MapNode wasnode = map.getNodeNoEx(nodepos, &is_valid_position);
		if (is_valid_position)
			client->removeNode(nodepos, 2);

		if (m_cache_enable_particles) {
			const ContentFeatures &features =
				client->getNodeDefManager()->get(wasnode);
			addDiggingParticles
			(gamedef, smgr, player, client->getEnv(),
			 nodepos, features.tiles);
		}

		runData->dig_time = 0;
		runData->digging = false;

		runData->nodig_delay_timer =
				runData->dig_time_complete / (float)crack_animation_length;

		// We don't want a corresponding delay to
		// very time consuming nodes
		if (runData->nodig_delay_timer > 0.3)
			runData->nodig_delay_timer = 0.3;

		// We want a slight delay to very little
		// time consuming nodes
		const float mindelay = 0.15;

		if (runData->nodig_delay_timer < mindelay)
			runData->nodig_delay_timer = mindelay;

		// Send event to trigger sound
		MtEvent *e = new NodeDugEvent(nodepos, wasnode);
		gamedef->event()->put(e);
	}

	if (runData->dig_time_complete < 100000.0) {
		runData->dig_time += dtime;
	} else {
		runData->dig_time = 0;
		client->setCrack(-1, nodepos);
	}

	camera->setDigging(0);  // left click animation
}


void Game::updateFrame(std::vector<aabb3f> &highlight_boxes,
		ProfilerGraph *graph, RunStats *stats, GameRunData *runData,
		f32 dtime, const VolatileRunFlags &flags, const CameraOrientation &cam)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();

	/*
		Fog range
	*/

	auto player_position = player->getPosition();
	auto pos_i = floatToInt(player_position, BS);
	if (!flags.no_output) {

	auto fog_was = runData->fog_range;

	if (draw_control->range_all) {
		runData->fog_range = 100000 * BS;
	} else if (!flags.no_output){
		runData->fog_range = draw_control->wanted_range * BS
				+ 0.0 * MAP_BLOCKSIZE * BS;

		if (flags.use_weather) {
			auto humidity = client->getEnv().getClientMap().getHumidity(pos_i, 1);
			runData->fog_range *= (1.55 - 1.4*(float)humidity/100);
		}

		runData->fog_range = MYMIN(
				runData->fog_range,
				(draw_control->farthest_drawn + 20) * BS);
		runData->fog_range *= 0.9;

		runData->fog_range = fog_was + (runData->fog_range-fog_was)/50;
	}

	/*
		Calculate general brightness
	*/
	u32 daynight_ratio = client->getEnv().getDayNightRatio();
	float time_brightness = decode_light_f((float)daynight_ratio / 1000.0);
	float direct_brightness = time_brightness;
	bool sunlight_seen = false;

	if (g_settings->getBool("free_move")) {
		//direct_brightness = time_brightness;
		sunlight_seen = true;
	} else if (!flags.no_output) {
		//ScopeProfiler sp(g_profiler, "Detecting background light", SPT_AVG);
		float old_brightness = sky->getBrightness();
		direct_brightness = client->getEnv().getClientMap()
				.getBackgroundBrightness(MYMIN(runData->fog_range * 1.2, 60 * BS),
					daynight_ratio, (int)(old_brightness * 255.5), &sunlight_seen)
				    / 255.0;
	}

	float time_of_day = runData->time_of_day;
	float time_of_day_smooth = runData->time_of_day_smooth;

	time_of_day = client->getEnv().getTimeOfDayF();

	const float maxsm = 0.05;
	const float todsm = 0.05;

	if (fabs(time_of_day - time_of_day_smooth) > maxsm &&
			fabs(time_of_day - time_of_day_smooth + 1.0) > maxsm &&
			fabs(time_of_day - time_of_day_smooth - 1.0) > maxsm)
		time_of_day_smooth = time_of_day;

	if (time_of_day_smooth > 0.8 && time_of_day < 0.2)
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm)
				+ (time_of_day + 1.0) * todsm;
	else
		time_of_day_smooth = time_of_day_smooth * (1.0 - todsm)
				+ time_of_day * todsm;

	runData->time_of_day = time_of_day;
	runData->time_of_day_smooth = time_of_day_smooth;

	if (!flags.no_output)
	sky->update(time_of_day_smooth, time_brightness, direct_brightness,
			sunlight_seen, camera->getCameraMode(), player->getYaw(),
			player->getPitch());

	/*
		Update clouds
	*/
	if (clouds) {
		if (sky->getCloudsVisible()) {
			clouds->setVisible(true);
			clouds->step(dtime);
			clouds->update(v2f(player_position.X, player_position.Z),
				       sky->getCloudColor());
		} else {
			clouds->setVisible(false);
		}
	}

	/*
		Update particles
	*/

	allparticles_step(dtime);
	allparticlespawners_step(dtime, client->getEnv());

	/*
		Fog
	*/

	if (m_cache_enable_fog && !flags.force_fog_off) {
		driver->setFog(
				sky->getBgColor(),
				video::EFT_FOG_LINEAR,
				runData->fog_range * 0.4,
				runData->fog_range * 1.0,
				0.01,
				false, // pixel fog
				false // range fog
		);
	} else {
		driver->setFog(
				sky->getBgColor(),
				video::EFT_FOG_LINEAR,
				100000 * BS,
				110000 * BS,
				0.01,
				false, // pixel fog
				false // range fog
		);
	}

	} // no_output

	/*
		Get chat messages from client
	*/

	v2u32 screensize = driver->getScreenSize();

	updateChat(*client, dtime, flags.show_debug, screensize,
			flags.show_chat, runData->profiler_current_page,
			*chat_backend, guitext_chat);

	/*
		Inventory
	*/

	if (client->getPlayerItem() != runData->new_playeritem)
		client->selectPlayerItem(runData->new_playeritem);

	// Update local inventory if it has changed
	if (client->getLocalInventoryUpdated()) {
		//infostream<<"Updating local inventory"<<std::endl;
		client->getLocalInventory(*local_inventory);
		runData->update_wielded_item_trigger = true;
	}

	if (runData->update_wielded_item_trigger) {
		// Update wielded tool
		InventoryList *mlist = local_inventory->getList("main");

		if (mlist && (client->getPlayerItem() < mlist->getSize())) {
			ItemStack item = mlist->getItem(client->getPlayerItem());
			camera->wield(item);
		}
		runData->update_wielded_item_trigger = false;
	}

	/*
		Update block draw list every 200ms or when camera direction has
		changed much
	*/
	runData->update_draw_list_timer += dtime;

	//auto camera_direction = camera->getDirection();
	auto camera_position = camera->getPosition();

		if (!flags.no_output)
		if (client->getEnv().getClientMap().m_drawlist_last || runData->update_draw_list_timer >= 0.5 ||
				runData->update_draw_list_last_cam_pos.getDistanceFrom(camera_position) > MAP_BLOCKSIZE*BS*2 ||
				flags.camera_offset_changed){
			runData->update_draw_list_timer = 0;
			bool allow = true;
#if CMAKE_THREADS && CMAKE_HAVE_FUTURE
			if (g_settings->getBool("more_threads")) {
				bool allow = true;
				if (updateDrawList_future.valid()) {
					auto res = updateDrawList_future.wait_for(std::chrono::milliseconds(0));
					if (res == std::future_status::timeout)
						allow = false;
				}
				if (allow) {
					updateDrawList_future = std::async(std::launch::async, [](Client * client, video::IVideoDriver* driver, float dtime){ client->getEnv().getClientMap().updateDrawList(driver, dtime, 1000); }, client, driver, dtime);
				}
			}
			else
#endif
				client->getEnv().getClientMap().updateDrawList(driver, dtime);
			if (allow)
				runData->update_draw_list_last_cam_pos = camera->getPosition();
		}

	updateGui(&runData->statustext_time, *stats, *runData, dtime, flags, cam);

	/*
	   make sure menu is on top
	   1. Delete formspec menu reference if menu was removed
	   2. Else, make sure formspec menu is on top
	*/
	if (current_formspec) {
		if (current_formspec->getReferenceCount() == 1) {
			current_formspec->drop();
			current_formspec = NULL;
		} else if (!noMenuActive()) {
			guiroot->bringToFront(current_formspec);
		}
	}

	/*
		Drawing begins
	*/

	video::SColor skycolor = sky->getSkyColor();

	TimeTaker tt_draw("mainloop: draw");
	if (!flags.no_output)
	{
		TimeTaker timer("beginScene");
		driver->beginScene(true, true, skycolor);
		stats->beginscenetime = timer.stop(true);
	}

	if (!flags.no_output)
	draw_scene(driver, smgr, *camera, *client, player, *hud, guienv,
			highlight_boxes, screensize, skycolor, flags.show_hud);

	/*
		Draw map
	*/
	if ((g_settings->getBool("hud_map")) && flags.show_hud)
	{
		mapper->drawMap( floatToInt(player->getPosition(), BS) );
	}

	/*
		Profiler graph
	*/
	if (flags.show_profiler_graph)
		graph->draw(10, screensize.Y - 10, driver, g_fontengine->getFont());

	/*
		Damage flash
	*/
	if (runData->damage_flash > 0.0) {
		video::SColor color(std::min(runData->damage_flash, 180.0f),
				180,
				0,
				0);
		driver->draw2DRectangle(color,
					core::rect<s32>(0, 0, screensize.X, screensize.Y),
					NULL);

		runData->damage_flash -= 100.0 * dtime;
	}

	/*
		Damage camera tilt
	*/
	if (player->hurt_tilt_timer > 0.0) {
		player->hurt_tilt_timer -= dtime * 5;

		if (player->hurt_tilt_timer < 0)
			player->hurt_tilt_strength = 0;
	}

		/*
			Draw background for player list
		*/
		if (playerlist != NULL)
		{
			driver->draw2DRectangle(console_bg, playerlist->getAbsolutePosition());
			driver->draw2DRectangleOutline(playerlist->getAbsolutePosition(), video::SColor(255,128,128,128));
		}

		/*
			Movement FOV (for superspeed and flying)
		*/

		float max_fov = 0;
		if(player->free_move)
			max_fov += 5;
		if(player->superspeed)
			max_fov += 8;

		if((player->free_move || player->superspeed) && player->movement_fov < max_fov)
			player->movement_fov += dtime*50;
		if(player->movement_fov > max_fov)
			player->movement_fov -= dtime*50;

	/*
		End scene
	*/
	if (!flags.no_output)
	{
		TimeTaker timer("endScene");
		driver->endScene();
		stats->endscenetime = timer.stop(true);
	}

	stats->drawtime = tt_draw.stop(true);
	g_profiler->graphAdd("mainloop_draw", stats->drawtime / 1000.0f);
}


void Game::updateGui(float *statustext_time, const RunStats &stats,
		const GameRunData& runData, f32 dtime, const VolatileRunFlags &flags,
		const CameraOrientation &cam)
{
	v2u32 screensize = driver->getScreenSize();
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	v3f player_position = player->getPosition();

	draw_control->drawtime_avg = draw_control->drawtime_avg * 0.95 + (float)stats.drawtime*0.05;
	draw_control->fps_avg = 1000/draw_control->drawtime_avg;
	draw_control->fps = (1.0/stats.dtime_jitter.avg);

	if (flags.show_debug) {
/*
		static float drawtime_avg = 0;
		drawtime_avg = drawtime_avg * 0.95 + stats.drawtime * 0.05;

		u16 fps = 1.0 / stats.dtime_jitter.avg;
*/
		//s32 fps = driver->getFPS();

		std::ostringstream os(std::ios_base::binary);
		os << std::fixed
		   << "Freeminer " << minetest_version_hash
		   << std::setprecision(0)
		   << " FPS = " << draw_control->fps
/*
		   << " (R: range_all=" << draw_control->range_all << ")"
*/
		   << std::setprecision(0)
		   << " drawtime = " << draw_control->drawtime_avg
/*
		   << std::setprecision(1)
		   << ", dtime_jitter = "
		   << (stats.dtime_jitter.max_fraction * 100.0) << " %"
*/
		   << std::setprecision(1)
		   << ", v_range = " << draw_control->wanted_range
		   << ", farmesh = "<<draw_control->farmesh<<":"<<draw_control->farmesh_step
		   << std::setprecision(3)
		   << ", RTT = " << client->getRTT();
		guitext->setText(narrow_to_wide(os.str()).c_str());
		guitext->setVisible(true);
	} else if (flags.show_hud || flags.show_chat) {
		std::ostringstream os(std::ios_base::binary);
		os << "Freeminer " << minetest_version_hash;
		guitext->setText(narrow_to_wide(os.str()).c_str());
		guitext->setVisible(true);
	} else {
		guitext->setVisible(false);
	}

	if (guitext->isVisible()) {
		core::rect<s32> rect(
				5,              5,
				screensize.X,   5 + g_fontengine->getTextHeight()
		);
		guitext->setRelativePosition(rect);
	}

	if (flags.show_debug) {
		auto pos_i = floatToInt(player_position, BS);

		std::ostringstream os(std::ios_base::binary);
		os << std::setprecision(1) << std::fixed
		   << "(" << (player_position.X / BS)
		   << ", " << (player_position.Y / BS)
		   << ", " << (player_position.Z / BS)
		   << ") (spd=" << (int)player->getSpeed().getLength()/BS
		   << ") (yaw=" << (wrapDegrees_0_360(cam.camera_yaw))
		   << ") (t=" << client->getEnv().getClientMap().getHeat(pos_i, 1)
		   << "C, h=" << client->getEnv().getClientMap().getHumidity(pos_i, 1)
/*
		   << "%) (seed = " << ((u64)client->getMapSeed())
*/
		   << "%"
		   << ")";

		if (runData.pointed_old.type == POINTEDTHING_NODE) {
			ClientMap &map = client->getEnv().getClientMap();
			const INodeDefManager *nodedef = client->getNodeDefManager();
			MapNode n = map.getNodeNoEx(runData.pointed_old.node_undersurface);
			const ContentFeatures &features = nodedef->get(n);
			if (n.getContent() != CONTENT_IGNORE && features.name != "unknown") {
				os << " (pointing_at = " << features.name
#if !defined(NDEBUG)
					<< " - " << features.tiledef[0].name.c_str()
					<< " - " << features.drawtype
					<< " - " << features.param_type
					<< " - " << features.param_type_2
#endif
				   << ")";
			}
		}

		guitext2->setText(narrow_to_wide(os.str()).c_str());
		guitext2->setVisible(true);

		core::rect<s32> rect(
				5,             5 + g_fontengine->getTextHeight(),
				screensize.X,  5 + g_fontengine->getTextHeight() * 2
		);
		guitext2->setRelativePosition(rect);
	} else {
		guitext2->setVisible(false);
	}

	guitext_info->setText(infotext.c_str());
	guitext_info->setVisible(flags.show_hud && g_menumgr.menuCount() == 0);

	float statustext_time_max = 1.5;

	if (!statustext.empty()) {
		*statustext_time += dtime;

		if (*statustext_time >= statustext_time_max) {
			statustext = L"";
			*statustext_time = 0;
		}
	}

	guitext_status->setText(statustext.c_str());
	guitext_status->setVisible(!statustext.empty());

	if (!statustext.empty()) {
		s32 status_y = screensize.Y - 130;
		core::rect<s32> rect(
				10, status_y - guitext_status->getTextHeight(),
				10 + guitext_status->getTextWidth(), status_y
		);
		guitext_status->setRelativePosition(rect);

		// Fade out
		video::SColor initial_color(255, 0, 0, 0);

		if (guienv->getSkin())
			initial_color = guienv->getSkin()->getColor(gui::EGDC_BUTTON_TEXT);

		video::SColor final_color = initial_color;
		final_color.setAlpha(0);
		video::SColor fade_color = initial_color.getInterpolated_quadratic(
				initial_color, final_color,
				pow(*statustext_time / statustext_time_max, 2.0f));
		guitext_status->setOverrideColor(fade_color);
		guitext_status->enableOverrideColor(true);
	}
}


/* Log times and stuff for visualization */
inline void Game::updateProfilerGraphs(ProfilerGraph *graph)
{
	Profiler::GraphValues values;
	g_profiler->graphGet(values);
	graph->put(values);
}



/****************************************************************************
 Misc
 ****************************************************************************/

/* On some computers framerate doesn't seem to be automatically limited
 */
inline void Game::limitFps(FpsControl *fps_timings, f32 *dtime)
{
	// not using getRealTime is necessary for wine
	device->getTimer()->tick(); // Maker sure device time is up-to-date
	u32 time = device->getTimer()->getTime();

	u32 last_time = fps_timings->last_time;

	if (time > last_time)  // Make sure time hasn't overflowed
		fps_timings->busy_time = time - last_time;
	else
		fps_timings->busy_time = 0;

	u32 frametime_min = 1000 / (g_menumgr.pausesGame()
			? g_settings->getFloat("pause_fps_max")
			: g_settings->getFloat("fps_max"));

	if (fps_timings->busy_time < frametime_min) {
		fps_timings->sleep_time = frametime_min - fps_timings->busy_time;
		device->sleep(fps_timings->sleep_time);
	} else {
		fps_timings->sleep_time = 0;
	}

	/* Get the new value of the device timer. Note that device->sleep() may
	 * not sleep for the entire requested time as sleep may be interrupted and
	 * therefore it is arguably more accurate to get the new time from the
	 * device rather than calculating it by adding sleep_time to time.
	 */

	device->getTimer()->tick(); // Update device timer
	time = device->getTimer()->getTime();

	if (time > last_time)  // Make sure last_time hasn't overflowed
		*dtime = (time - last_time) / 1000.0;
	else
		*dtime = 0;

	fps_timings->last_time = time;
}


void Game::showOverlayMessage(const char *msg, float dtime,
		int percent, bool draw_clouds)
{
	wchar_t *text = wgettext(msg);
	draw_load_screen(text, device, guienv, dtime, percent, draw_clouds);
	delete[] text;
}


/****************************************************************************
 Shutdown / cleanup
 ****************************************************************************/

void Game::extendedResourceCleanup()
{
	// Extended resource accounting
	infostream << "Irrlicht resources after cleanup:" << std::endl;
	infostream << "\tRemaining meshes   : "
	           << device->getSceneManager()->getMeshCache()->getMeshCount() << std::endl;
	infostream << "\tRemaining textures : "
	           << driver->getTextureCount() << std::endl;

	for (unsigned int i = 0; i < driver->getTextureCount(); i++) {
		irr::video::ITexture *texture = driver->getTextureByIndex(i);
		infostream << "\t\t" << i << ":" << texture->getName().getPath().c_str()
		           << std::endl;
	}

	clearTextureNameCache();
	infostream << "\tRemaining materials: "
               << driver-> getMaterialRendererCount()
		       << " (note: irrlicht doesn't support removing renderers)" << std::endl;
}



/****************************************************************************
 extern function for launching the game
 ****************************************************************************/

bool the_game(bool *kill,
		bool random_input,
		InputHandler *input,
		IrrlichtDevice *device,

		const std::string &map_dir,
		const std::string &playername,
		const std::string &password,
		const std::string &address,         // If empty local server is created
		u16 port,

		std::string &error_message,
		ChatBackend &chat_backend,
		const SubgameSpec &gamespec,        // Used for local game
		bool simple_singleplayer_mode,
		unsigned int autoexit
	)
{
	Game game;

	/* Make a copy of the server address because if a local singleplayer server
	 * is created then this is updated and we don't want to change the value
	 * passed to us by the calling function
	 */
	std::string server_address = address;

	bool started = false;
	try {

		bool started = false;
		game.runData  = { 0 };
		if (game.startup(kill, random_input, input, device, map_dir,
					playername, password, &server_address, port,
					&error_message, &chat_backend, gamespec,
					simple_singleplayer_mode)) {
			started = true;
			game.runData.autoexit = autoexit;

			game.run();
			game.shutdown();
		}

#ifdef NDEBUG
	} catch (SerializationError &e) {
		error_message = std::string("A serialization error occurred:\n")
				+ e.what() + "\n\nThe server is probably "
				" running a different version of Freeminer.";
		errorstream << (error_message) << std::endl;
	} catch (ServerError &e) {
		error_message = e.what();
		errorstream << "ServerError: " << e.what() << std::endl;
	} catch (ModError &e) {
		errorstream << "ModError: " << e.what() << std::endl;
		error_message = std::string() + e.what() + _("\nCheck debug.txt for details.");
#else
	} catch (int) { //nothing
#endif
	}

	return !started && game.flags.reconnect;
}

