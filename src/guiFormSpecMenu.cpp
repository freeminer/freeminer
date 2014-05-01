/*
guiFormSpecMenu.cpp
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


#include <cstdlib>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <limits>
#include "guiFormSpecMenu.h"
#include "guiTable.h"
#include "constants.h"
#include "gamedef.h"
#include "keycode.h"
#include "strfnd.h"
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIButton.h>
#include <IGUIStaticText.h>
#include <IGUIFont.h>
#include <IGUITabControl.h>
#include <IGUIComboBox.h>
#include "log.h"
#include "tile.h" // ITextureSource
#include "hud.h" // drawItemStack
#include "hex.h"
#include "util/string.h"
#include "util/numeric.h"
#include "filesys.h"
#include "gettime.h"
#include "gettext.h"
#if USE_FREETYPE
#include "settings.h"
#include "main.h"  // for g_settings
#include "intlGUIEditBox.h"
#endif

#include "scripting_game.h"

#define MY_CHECKPOS(a,b)													\
	if (v_pos.size() != 2) {												\
		errorstream<< "Invalid pos for element " << a << "specified: \""	\
			<< parts[b] << "\"" << std::endl;								\
			return;															\
	}

#define MY_CHECKGEOM(a,b)													\
	if (v_geom.size() != 2) {												\
		errorstream<< "Invalid pos for element " << a << "specified: \""	\
			<< parts[b] << "\"" << std::endl;								\
			return;															\
	}


/*
	GUIFormSpecMenu
*/

GUIFormSpecMenu::GUIFormSpecMenu(irr::IrrlichtDevice* dev,
		gui::IGUIElement* parent, s32 id, IMenuManager *menumgr,
		InventoryManager *invmgr, IGameDef *gamedef,
		ISimpleTextureSource *tsrc, IFormSource* fsrc, TextDest* tdst,
		GUIFormSpecMenu** ext_ptr) :
	GUIModalMenu(dev->getGUIEnvironment(), parent, id, menumgr),
	m_device(dev),
	m_invmgr(invmgr),
	m_gamedef(gamedef),
	m_tsrc(tsrc),
	m_selected_item(NULL),
	m_selected_amount(0),
	m_selected_dragging(false),
	m_tooltip_element(NULL),
	m_allowclose(true),
	m_lock(false),
	m_form_src(fsrc),
	m_text_dst(tdst),
	m_ext_ptr(ext_ptr)
{
	current_keys_pending.key_down = false;
	current_keys_pending.key_up = false;
	current_keys_pending.key_enter = false;
	current_keys_pending.key_escape = false;

}

GUIFormSpecMenu::~GUIFormSpecMenu()
{
	removeChildren();

	delete m_selected_item;

	if (m_form_src != NULL) {
		delete m_form_src;
	}
	if (m_text_dst != NULL) {
		delete m_text_dst;
	}

	if (m_ext_ptr != NULL) {
		assert(*m_ext_ptr == this);
		*m_ext_ptr = NULL;
	}
}

void GUIFormSpecMenu::removeChildren()
{
	const core::list<gui::IGUIElement*> &children = getChildren();
	core::list<gui::IGUIElement*> children_copy;
	for(core::list<gui::IGUIElement*>::ConstIterator
			i = children.begin(); i != children.end(); i++)
	{
		children_copy.push_back(*i);
	}
	for(core::list<gui::IGUIElement*>::Iterator
			i = children_copy.begin();
			i != children_copy.end(); i++)
	{
		(*i)->remove();
	}
	/*{
		gui::IGUIElement *e = getElementFromId(256);
		if(e != NULL)
			e->remove();
	}*/

	if(m_tooltip_element)
	{
		m_tooltip_element->remove();
		m_tooltip_element->drop();
		m_tooltip_element = NULL;
	}

}

void GUIFormSpecMenu::setInitialFocus()
{
	// Set initial focus according to following order of precedence:
	// 1. first empty editbox
	// 2. first editbox
	// 3. first table
	// 4. last button
	// 5. first focusable (not statictext, not tabheader)
	// 6. first child element

	core::list<gui::IGUIElement*> children = getChildren();

	// in case "children" contains any NULL elements, remove them
	for (core::list<gui::IGUIElement*>::Iterator it = children.begin();
			it != children.end();) {
		if (*it)
			++it;
		else
			it = children.erase(it);
	}

	// 1. first empty editbox
	for (core::list<gui::IGUIElement*>::Iterator it = children.begin();
			it != children.end(); ++it) {
		if ((*it)->getType() == gui::EGUIET_EDIT_BOX
				&& (*it)->getText()[0] == 0) {
			Environment->setFocus(*it);
			return;
		}
	}

	// 2. first editbox
	for (core::list<gui::IGUIElement*>::Iterator it = children.begin();
			it != children.end(); ++it) {
		if ((*it)->getType() == gui::EGUIET_EDIT_BOX) {
			Environment->setFocus(*it);
			return;
		}
	}

	// 3. first table
	for (core::list<gui::IGUIElement*>::Iterator it = children.begin();
			it != children.end(); ++it) {
		if ((*it)->getTypeName() == std::string("GUITable")) {
			Environment->setFocus(*it);
			return;
		}
	}

	// 4. last button
	for (core::list<gui::IGUIElement*>::Iterator it = children.getLast();
			it != children.end(); --it) {
		if ((*it)->getType() == gui::EGUIET_BUTTON) {
			Environment->setFocus(*it);
			return;
		}
	}

	// 5. first focusable (not statictext, not tabheader)
	for (core::list<gui::IGUIElement*>::Iterator it = children.begin();
			it != children.end(); ++it) {
		if ((*it)->getType() != gui::EGUIET_STATIC_TEXT &&
				(*it)->getType() != gui::EGUIET_TAB_CONTROL) {
			Environment->setFocus(*it);
			return;
		}
	}

	// 6. first child element
	if (children.empty())
		Environment->setFocus(this);
	else
		Environment->setFocus(*(children.begin()));
}

GUITable* GUIFormSpecMenu::getTable(std::wstring tablename)
{
	for (u32 i = 0; i < m_tables.size(); ++i) {
		if (tablename == m_tables[i].first.fname)
			return m_tables[i].second;
	}
	return 0;
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> tokens;

	std::string current = "";
	bool last_was_escape = false;
	for(unsigned int i=0; i < s.size(); i++) {
		if (last_was_escape) {
			current += '\\';
			current += s.c_str()[i];
			last_was_escape = false;
		}
		else {
			if (s.c_str()[i] == delim) {
				tokens.push_back(current);
				current = "";
				last_was_escape = false;
			}
			else if (s.c_str()[i] == '\\'){
				last_was_escape = true;
			}
			else {
				current += s.c_str()[i];
				last_was_escape = false;
			}
		}
	}
	//push last element
	tokens.push_back(current);

	return tokens;
}

void GUIFormSpecMenu::parseSize(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,',');

	if ((parts.size() == 2) || parts.size() == 3) {
		v2f invsize;

		if (parts[1].find(';') != std::string::npos)
			parts[1] = parts[1].substr(0,parts[1].find(';'));

		invsize.X = stof(parts[0]);
		invsize.Y = stof(parts[1]);

		lockSize(false);
		if (parts.size() == 3) {
			if (parts[2] == "true") {
				lockSize(true,v2u32(800,600));
			}
		}

		if (m_lock) {
			v2u32 current_screensize = m_device->getVideoDriver()->getScreenSize();
			v2u32 delta = current_screensize - m_lockscreensize;

			if (current_screensize.Y > m_lockscreensize.Y)
				delta.Y /= 2;
			else
				delta.Y = 0;

			if (current_screensize.X > m_lockscreensize.X)
				delta.X /= 2;
			else
				delta.X = 0;

			offset = v2s32(delta.X,delta.Y);

			data->screensize = m_lockscreensize;
		}
		else {
			offset = v2s32(0,0);
		}

		padding = v2s32(data->screensize.Y/40, data->screensize.Y/40);
		spacing = v2s32(data->screensize.Y/12, data->screensize.Y/13);
		imgsize = v2s32(data->screensize.Y/15, data->screensize.Y/15);
		data->size = v2s32(
			padding.X*2+spacing.X*(invsize.X-1.0)+imgsize.X,
			padding.Y*2+spacing.Y*(invsize.Y-1.0)+imgsize.Y + (data->helptext_h-5)
		);
		data->rect = core::rect<s32>(
				data->screensize.X/2 - data->size.X/2 + offset.X,
				data->screensize.Y/2 - data->size.Y/2 + offset.Y,
				data->screensize.X/2 + data->size.X/2 + offset.X,
				data->screensize.Y/2 + data->size.Y/2 + offset.Y
		);

		DesiredRect = data->rect;
		recalculateAbsolutePosition(false);
		data->basepos = getBasePos();
		data->bp_set = 2;
		return;
	}
	errorstream<< "Invalid size element (" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseList(parserData* data,std::string element)
{
	if (m_gamedef == 0) {
		errorstream<<"WARNING: invalid use of 'list' with m_gamedef==0"<<std::endl;
		return;
	}

	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 4) || (parts.size() == 5)) {
		std::string location = parts[0];
		std::string listname = parts[1];
		std::vector<std::string> v_pos  = split(parts[2],',');
		std::vector<std::string> v_geom = split(parts[3],',');
		std::string startindex = "";
		if (parts.size() == 5)
			startindex = parts[4];

		MY_CHECKPOS("list",2);
		MY_CHECKGEOM("list",3);

		InventoryLocation loc;

		if(location == "context" || location == "current_name")
			loc = m_current_inventory_location;
		else
			loc.deSerialize(location);

		v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		v2s32 geom;
		geom.X = stoi(v_geom[0]);
		geom.Y = stoi(v_geom[1]);

		s32 start_i = 0;
		if(startindex != "")
			start_i = stoi(startindex);

		if (geom.X < 0 || geom.Y < 0 || start_i < 0) {
			errorstream<< "Invalid list element: '" << element << "'"  << std::endl;
			return;
		}

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of list without a size[] element"<<std::endl;
		m_inventorylists.push_back(ListDrawSpec(loc, listname, pos, geom, start_i));
		return;
	}
	errorstream<< "Invalid list element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseCheckbox(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 3) || (parts.size() == 4)) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::string name = parts[1];
		std::string label = parts[2];
		std::string selected = "";

		if (parts.size() == 4)
			selected = parts[3];

		MY_CHECKPOS("checkbox",0);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float) spacing.X;
		pos.Y += stof(v_pos[1]) * (float) spacing.Y;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y+((imgsize.Y/2)-15), pos.X+300, pos.Y+((imgsize.Y/2)+15));

		bool fselected = false;

		if (selected == "true")
			fselected = true;

		std::wstring wlabel = narrow_to_wide(label.c_str());

		FieldSpec spec = FieldSpec(
				narrow_to_wide(name.c_str()),
				L"",
				wlabel,
				258+m_fields.size()
			);

		spec.ftype = f_CheckBox;
		spec.flabel = wlabel; //Needed for displaying text on MSVC
		gui::IGUICheckBox* e = Environment->addCheckBox(fselected, rect, this,
					spec.fid, spec.flabel.c_str());

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		m_checkboxes.push_back(std::pair<FieldSpec,gui::IGUICheckBox*>(spec,e));
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid checkbox element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseImage(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 3) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = unescape_string(parts[2]);

		MY_CHECKPOS("image",0);
		MY_CHECKGEOM("image",1);

		v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
		pos.X += stof(v_pos[0]) * (float) spacing.X;
		pos.Y += stof(v_pos[1]) * (float) spacing.Y;

		v2s32 geom;
		geom.X = stof(v_geom[0]) * (float)imgsize.X;
		geom.Y = stof(v_geom[1]) * (float)imgsize.Y;

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of image without a size[] element"<<std::endl;
		m_images.push_back(ImageDrawSpec(name, pos, geom));
		return;
	}

	if (parts.size() == 2) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::string name = unescape_string(parts[1]);

		MY_CHECKPOS("image",0);

		v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
		pos.X += stof(v_pos[0]) * (float) spacing.X;
		pos.Y += stof(v_pos[1]) * (float) spacing.Y;

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of image without a size[] element"<<std::endl;
		m_images.push_back(ImageDrawSpec(name, pos));
		return;
	}
	errorstream<< "Invalid image element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseItemImage(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 3) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = parts[2];

		MY_CHECKPOS("itemimage",0);
		MY_CHECKGEOM("itemimage",1);

		v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
		pos.X += stof(v_pos[0]) * (float) spacing.X;
		pos.Y += stof(v_pos[1]) * (float) spacing.Y;

		v2s32 geom;
		geom.X = stoi(v_geom[0]) * (float)imgsize.X;
		geom.Y = stoi(v_geom[1]) * (float)imgsize.Y;

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of item_image without a size[] element"<<std::endl;
		m_itemimages.push_back(ImageDrawSpec(name, pos, geom));
		return;
	}
	errorstream<< "Invalid ItemImage element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseButton(parserData* data,std::string element,
		std::string type)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 4) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = parts[2];
		std::string label = parts[3];

		MY_CHECKPOS("button",0);
		MY_CHECKGEOM("button",1);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		v2s32 geom;
		geom.X = (stof(v_geom[0]) * (float)spacing.X)-(spacing.X-imgsize.X);
		pos.Y += (stof(v_geom[1]) * (float)imgsize.Y)/2;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y-15, pos.X+geom.X, pos.Y+15);

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of button without a size[] element"<<std::endl;

		label = unescape_string(label);

		std::wstring wlabel = narrow_to_wide(label.c_str());

		FieldSpec spec = FieldSpec(
			narrow_to_wide(name.c_str()),
			wlabel,
			L"",
			258+m_fields.size()
		);
		spec.ftype = f_Button;
		if(type == "button_exit")
			spec.is_exit = true;

		gui::IGUIButton* e = Environment->addButton(rect, this, spec.fid,
				spec.flabel.c_str());

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid button element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseBackground(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 3) || (parts.size() == 4)) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = unescape_string(parts[2]);

		MY_CHECKPOS("background",0);
		MY_CHECKGEOM("background",1);

		v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
		pos.X += stof(v_pos[0]) * (float)spacing.X - ((float)spacing.X-(float)imgsize.X)/2;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y - ((float)spacing.Y-(float)imgsize.Y)/2;

		v2s32 geom;
		geom.X = stof(v_geom[0]) * (float)spacing.X;
		geom.Y = stof(v_geom[1]) * (float)spacing.Y;

		if (parts.size() == 4) {
			m_clipbackground = is_yes(parts[3]);
			if (m_clipbackground) {
				pos.X = stoi(v_pos[0]); //acts as offset
				pos.Y = stoi(v_pos[1]); //acts as offset
			}
		}

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of background without a size[] element"<<std::endl;
		m_backgrounds.push_back(ImageDrawSpec(name, pos, geom));
		return;
	}
	errorstream<< "Invalid background element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseTableOptions(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	data->table_options.clear();
	for (size_t i = 0; i < parts.size(); ++i) {
		// Parse table option
		std::string opt = unescape_string(parts[i]);
		data->table_options.push_back(GUITable::splitOption(opt));
	}
}

void GUIFormSpecMenu::parseTableColumns(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	data->table_columns.clear();
	for (size_t i = 0; i < parts.size(); ++i) {
		std::vector<std::string> col_parts = split(parts[i],',');
		GUITable::TableColumn column;
		// Parse column type
		if (!col_parts.empty())
			column.type = col_parts[0];
		// Parse column options
		for (size_t j = 1; j < col_parts.size(); ++j) {
			std::string opt = unescape_string(col_parts[j]);
			column.options.push_back(GUITable::splitOption(opt));
		}
		data->table_columns.push_back(column);
	}
}

void GUIFormSpecMenu::parseTable(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 4) || (parts.size() == 5)) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = parts[2];
		std::vector<std::string> items = split(parts[3],',');
		std::string str_initial_selection = "";
		std::string str_transparent = "false";

		if (parts.size() >= 5)
			str_initial_selection = parts[4];

		MY_CHECKPOS("table",0);
		MY_CHECKGEOM("table",1);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		v2s32 geom;
		geom.X = stof(v_geom[0]) * (float)spacing.X;
		geom.Y = stof(v_geom[1]) * (float)spacing.Y;


		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

		std::wstring fname_w = narrow_to_wide(name.c_str());

		FieldSpec spec = FieldSpec(
			fname_w,
			L"",
			L"",
			258+m_fields.size()
		);

		spec.ftype = f_Table;

		for (unsigned int i = 0; i < items.size(); ++i) {
			items[i] = unescape_string(items[i]);
		}

		//now really show table
		GUITable *e = new GUITable(Environment, this, spec.fid, rect,
				m_tsrc);
		e->drop();  // IGUIElement maintains the remaining reference

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		e->setTable(data->table_options, data->table_columns, items);

		if (data->table_dyndata.find(fname_w) != data->table_dyndata.end()) {
			e->setDynamicData(data->table_dyndata[fname_w]);
		}

		if ((str_initial_selection != "") &&
				(str_initial_selection != "0"))
			e->setSelected(stoi(str_initial_selection.c_str()));

		m_tables.push_back(std::pair<FieldSpec,GUITable*>(spec, e));
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid table element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseTextList(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 4) || (parts.size() == 5) || (parts.size() == 6)) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = parts[2];
		std::vector<std::string> items = split(parts[3],',');
		std::string str_initial_selection = "";
		std::string str_transparent = "false";

		if (parts.size() >= 5)
			str_initial_selection = parts[4];

		if (parts.size() >= 6)
			str_transparent = parts[5];

		MY_CHECKPOS("textlist",0);
		MY_CHECKGEOM("textlist",1);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		v2s32 geom;
		geom.X = stof(v_geom[0]) * (float)spacing.X;
		geom.Y = stof(v_geom[1]) * (float)spacing.Y;


		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

		std::wstring fname_w = narrow_to_wide(name.c_str());

		FieldSpec spec = FieldSpec(
			fname_w,
			L"",
			L"",
			258+m_fields.size()
		);

		spec.ftype = f_Table;

		for (unsigned int i = 0; i < items.size(); ++i) {
			items[i] = unescape_string(items[i]);
		}

		//now really show list
		GUITable *e = new GUITable(Environment, this, spec.fid, rect,
				m_tsrc);
		e->drop();  // IGUIElement maintains the remaining reference

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		e->setTextList(items, is_yes(str_transparent));

		if (data->table_dyndata.find(fname_w) != data->table_dyndata.end()) {
			e->setDynamicData(data->table_dyndata[fname_w]);
		}

		if ((str_initial_selection != "") &&
				(str_initial_selection != "0"))
			e->setSelected(stoi(str_initial_selection.c_str()));

		m_tables.push_back(std::pair<FieldSpec,GUITable*>(spec, e));
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid textlist element(" << parts.size() << "): '" << element << "'"  << std::endl;
}


void GUIFormSpecMenu::parseDropDown(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 5) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::string name = parts[2];
		std::vector<std::string> items = split(parts[3],',');
		std::string str_initial_selection = "";
		str_initial_selection = parts[4];

		MY_CHECKPOS("dropdown",0);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		s32 width = stof(parts[1]) * (float)spacing.Y;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+width, pos.Y+30);

		std::wstring fname_w = narrow_to_wide(name.c_str());

		FieldSpec spec = FieldSpec(
			fname_w,
			L"",
			L"",
			258+m_fields.size()
		);

		spec.ftype = f_DropDown;
		spec.send = true;

		//now really show list
		gui::IGUIComboBox *e = Environment->addComboBox(rect, this,spec.fid);

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		for (unsigned int i=0; i < items.size(); i++) {
			e->addItem(narrow_to_wide(items[i]).c_str());
		}

		if (str_initial_selection != "")
			e->setSelected(stoi(str_initial_selection.c_str())-1);

		m_fields.push_back(spec);
		return;
	}
	errorstream << "Invalid dropdown element(" << parts.size() << "): '"
				<< element << "'"  << std::endl;
}

void GUIFormSpecMenu::parsePwdField(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 4 || parts.size() == 5) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string name = parts[2];
		std::string label = parts[3];
		std::string default_val;
		if (parts.size() == 5) default_val = parts[4];

		MY_CHECKPOS("pwdfield",0);
		MY_CHECKGEOM("pwdfield",1);

		v2s32 pos;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		v2s32 geom;
		geom.X = (stof(v_geom[0]) * (float)spacing.X)-(spacing.X-imgsize.X);

		pos.Y += (stof(v_geom[1]) * (float)imgsize.Y)/2;
		pos.Y -= 15;
		geom.Y = 30;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

		if(m_form_src && default_val.size())
			default_val = m_form_src->resolveText(default_val);

		default_val = unescape_string(default_val);
		label = unescape_string(label);

		std::wstring wlabel = narrow_to_wide(label.c_str());

		FieldSpec spec = FieldSpec(
			narrow_to_wide(name.c_str()),
			wlabel,
			narrow_to_wide(default_val.c_str()),
			258+m_fields.size()
			);

		spec.send = true;
		gui::IGUIEditBox * e = Environment->addEditBox(0, rect, true, this, spec.fid);

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		if (label.length() >= 1)
		{
			rect.UpperLeftCorner.Y -= 15;
			rect.LowerRightCorner.Y = rect.UpperLeftCorner.Y + 15;
			Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, 0);
		}

		e->setPasswordBox(true,L'*');

		irr::SEvent evt;
		evt.EventType            = EET_KEY_INPUT_EVENT;
		evt.KeyInput.Key         = KEY_END;
		evt.KeyInput.Char        = 0;
		evt.KeyInput.Control     = 0;
		evt.KeyInput.Shift       = 0;
		evt.KeyInput.PressedDown = true;
		e->OnEvent(evt);
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid pwdfield element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseSimpleField(parserData* data,
		std::vector<std::string> &parts)
{
	std::string name = parts[0];
	std::string label = parts[1];
	std::string default_val = parts[2];

	core::rect<s32> rect;

	if(!data->bp_set)
	{
		rect = core::rect<s32>(
			data->screensize.X/2 - 580/2,
			data->screensize.Y/2 - 300/2,
			data->screensize.X/2 + 580/2,
			data->screensize.Y/2 + 300/2
		);
		DesiredRect = rect;
		recalculateAbsolutePosition(false);
		data->basepos = getBasePos();
		data->bp_set = 1;
	}
	else if(data->bp_set == 2)
		errorstream<<"WARNING: invalid use of unpositioned \"field\" in inventory"<<std::endl;

	v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
	pos.Y = ((m_fields.size()+2)*60);
	v2s32 size = DesiredRect.getSize();

	rect = core::rect<s32>(size.X/2-150, pos.Y, (size.X/2-150)+300, pos.Y+30);


	if(m_form_src)
		default_val = m_form_src->resolveText(default_val);

	default_val = unescape_string(default_val);
	label = unescape_string(label);

	std::wstring wlabel = narrow_to_wide(label.c_str());

	FieldSpec spec = FieldSpec(
		narrow_to_wide(name.c_str()),
		wlabel,
		narrow_to_wide(default_val.c_str()),
		258+m_fields.size()
	);

	if (name == "")
	{
		// spec field id to 0, this stops submit searching for a value that isn't there
		Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, spec.fid);
	}
	else
	{
		spec.send = true;

		gui::IGUIEditBox *e = nullptr;
		#if USE_FREETYPE
		if (g_settings->getBool("freetype"))
			e = (gui::IGUIEditBox *) new gui::intlGUIEditBox(spec.fdefault.c_str(), true, Environment, this, spec.fid, rect);
		#endif
		if (!e)
			e = Environment->addEditBox(spec.fdefault.c_str(), rect, true, this, spec.fid);

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		irr::SEvent evt;
		evt.EventType            = EET_KEY_INPUT_EVENT;
		evt.KeyInput.Key         = KEY_END;
		evt.KeyInput.Char        = 0;
		evt.KeyInput.Control     = 0;
		evt.KeyInput.Shift       = 0;
		evt.KeyInput.PressedDown = true;
		e->OnEvent(evt);

		if (label.length() >= 1)
		{
			rect.UpperLeftCorner.Y -= 15;
			rect.LowerRightCorner.Y = rect.UpperLeftCorner.Y + 15;
			Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, 0);
		}
	}

	m_fields.push_back(spec);
}

void GUIFormSpecMenu::parseTextArea(parserData* data,
		std::vector<std::string>& parts,std::string type)
{

	std::vector<std::string> v_pos = split(parts[0],',');
	std::vector<std::string> v_geom = split(parts[1],',');
	std::string name = parts[2];
	std::string label = parts[3];
	std::string default_val = parts[4];

	MY_CHECKPOS(type,0);
	MY_CHECKGEOM(type,1);

	v2s32 pos;
	pos.X = stof(v_pos[0]) * (float) spacing.X;
	pos.Y = stof(v_pos[1]) * (float) spacing.Y;

	v2s32 geom;

	geom.X = (stof(v_geom[0]) * (float)spacing.X)-(spacing.X-imgsize.X);

	if (type == "textarea")
	{
		geom.Y = (stof(v_geom[1]) * (float)imgsize.Y) - (spacing.Y-imgsize.Y);
		pos.Y += 15;
	}
	else
	{
		pos.Y += (stof(v_geom[1]) * (float)imgsize.Y)/2;
		pos.Y -= 15;
		geom.Y = 30;
	}

	core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

	if(data->bp_set != 2)
		errorstream<<"WARNING: invalid use of positioned "<<type<<" without a size[] element"<<std::endl;

	if(m_form_src)
		default_val = m_form_src->resolveText(default_val);


	default_val = unescape_string(default_val);
	label = unescape_string(label);

	std::wstring wlabel = narrow_to_wide(label.c_str());

	FieldSpec spec = FieldSpec(
		narrow_to_wide(name.c_str()),
		wlabel,
		narrow_to_wide(default_val.c_str()),
		258+m_fields.size()
	);

	if (name == "")
	{
		// spec field id to 0, this stops submit searching for a value that isn't there
		Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, spec.fid);
	}
	else
	{
		spec.send = true;

		gui::IGUIEditBox *e = nullptr;
		#if USE_FREETYPE
		if (g_settings->getBool("freetype"))
			e = (gui::IGUIEditBox *) new gui::intlGUIEditBox(spec.fdefault.c_str(), true, Environment, this, spec.fid, rect);
		#endif
		if (!e)
			e = Environment->addEditBox(spec.fdefault.c_str(), rect, true, this, spec.fid);

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		if (type == "textarea")
		{
			e->setMultiLine(true);
			e->setWordWrap(true);
			e->setTextAlignment(gui::EGUIA_UPPERLEFT, gui::EGUIA_UPPERLEFT);
		} else {
			irr::SEvent evt;
			evt.EventType            = EET_KEY_INPUT_EVENT;
			evt.KeyInput.Key         = KEY_END;
			evt.KeyInput.Char        = 0;
			evt.KeyInput.Control     = 0;
			evt.KeyInput.Shift       = 0;
			evt.KeyInput.PressedDown = true;
			e->OnEvent(evt);
		}

		if (label.length() >= 1)
		{
			rect.UpperLeftCorner.Y -= 15;
			rect.LowerRightCorner.Y = rect.UpperLeftCorner.Y + 15;
			Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, 0);
		}
	}
	m_fields.push_back(spec);
}

void GUIFormSpecMenu::parseField(parserData* data,std::string element,
		std::string type)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 3) {
		parseSimpleField(data,parts);
		return;
	}

	if (parts.size() == 5) {
		parseTextArea(data,parts,type);
		return;
	}
	errorstream<< "Invalid field element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseLabel(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 2) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::string text = parts[1];

		MY_CHECKPOS("label",0);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y+((imgsize.Y/2)-15), pos.X+300, pos.Y+((imgsize.Y/2)+15));

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of label without a size[] element"<<std::endl;

		text = unescape_string(text);

		std::wstring wlabel = narrow_to_wide(text.c_str());

		FieldSpec spec = FieldSpec(
			L"",
			wlabel,
			L"",
			258+m_fields.size()
		);
		Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, spec.fid);
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid label element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseVertLabel(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 2) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::wstring text = narrow_to_wide(unescape_string(parts[1]));

		MY_CHECKPOS("vertlabel",1);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y+((imgsize.Y/2)-15), pos.X+15, pos.Y+300);

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of label without a size[] element"<<std::endl;

		std::wstring label = L"";

		for (unsigned int i=0; i < text.length(); i++) {
			label += text[i];
			label += L"\n";
		}

		FieldSpec spec = FieldSpec(
			L"",
			label,
			L"",
			258+m_fields.size()
		);
		gui::IGUIStaticText *t =
				Environment->addStaticText(spec.flabel.c_str(), rect, false, true, this, spec.fid);
		t->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid vertlabel element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseImageButton(parserData* data,std::string element,
		std::string type)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 5) || (parts.size() == 7) || (parts.size() == 8)) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string image_name = parts[2];
		std::string name = parts[3];
		std::string label = parts[4];

		MY_CHECKPOS("imagebutton",0);
		MY_CHECKGEOM("imagebutton",1);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;
		v2s32 geom;
		geom.X = (stof(v_geom[0]) * (float)spacing.X)-(spacing.X-imgsize.X);
		geom.Y = (stof(v_geom[1]) * (float)spacing.Y)-(spacing.Y-imgsize.Y);

		bool noclip = false;
		bool drawborder = true;

		if ((parts.size() >= 7)) {
			if (parts[5] == "true")
				noclip = true;

			if (parts[6] == "false")
				drawborder = false;
		}
		
		std::string pressed_image_name = "";
		
		if ((parts.size() == 8)) {
			pressed_image_name = parts[7];
		}

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of image_button without a size[] element"<<std::endl;

		image_name = unescape_string(image_name);
		pressed_image_name = unescape_string(pressed_image_name);
		label = unescape_string(label);

		std::wstring wlabel = narrow_to_wide(label.c_str());

		FieldSpec spec = FieldSpec(
			narrow_to_wide(name.c_str()),
			wlabel,
			narrow_to_wide(image_name.c_str()),
			258+m_fields.size()
		);
		spec.ftype = f_Button;
		if(type == "image_button_exit")
			spec.is_exit = true;

		video::ITexture *texture = 0;
		video::ITexture *pressed_texture = 0;
		texture = m_tsrc->getTexture(image_name);
		if (parts.size() == 8)
			pressed_texture = m_tsrc->getTexture(pressed_image_name);
		else
			pressed_texture = texture;

		gui::IGUIButton *e = Environment->addButton(rect, this, spec.fid, spec.flabel.c_str());

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		e->setUseAlphaChannel(true);
		e->setImage(texture);
		e->setPressedImage(pressed_texture);
		e->setScaleImage(true);
		e->setNotClipped(noclip);
		e->setDrawBorder(drawborder);

		m_fields.push_back(spec);
		return;
	}

	errorstream<< "Invalid imagebutton element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseTabHeader(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 4) || (parts.size() == 6)) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::string name = parts[1];
		std::vector<std::string> buttons = split(parts[2],',');
		std::string str_index = parts[3];
		bool show_background = true;
		bool show_border = true;
		int tab_index = stoi(str_index) -1;

		MY_CHECKPOS("tabheader",0);

		if (parts.size() == 6) {
			if (parts[4] == "true")
				show_background = false;
			if (parts[5] == "false")
				show_border = false;
		}

		FieldSpec spec = FieldSpec(
			narrow_to_wide(name.c_str()),
			L"",
			L"",
			258+m_fields.size()
		);

		spec.ftype = f_TabHeader;

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;
		v2s32 geom;
		geom.X = data->screensize.Y;
		geom.Y = 30;

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

		gui::IGUITabControl *e = Environment->addTabControl(rect,this,show_background,show_border,spec.fid);

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		e->setNotClipped(true);

		for (unsigned int i=0; i< buttons.size(); i++) {
			wchar_t* wbutton = 0;

			std::wstring wlabel = narrow_to_wide(buttons[i]); //Needed for displaying text on windows
			wbutton = (wchar_t*) wlabel.c_str();

			e->addTab(wbutton,-1);
		}

		if ((tab_index >= 0) &&
				(buttons.size() < INT_MAX) &&
				(tab_index < (int) buttons.size()))
			e->setActiveTab(tab_index);

		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid TabHeader element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseItemImageButton(parserData* data,std::string element)
{

	if (m_gamedef == 0) {
		errorstream<<"WARNING: invalid use of item_image_button with m_gamedef==0"<<std::endl;
		return;
	}

	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 5) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');
		std::string item_name = parts[2];
		std::string name = parts[3];
		std::string label = parts[4];

		MY_CHECKPOS("itemimagebutton",0);
		MY_CHECKGEOM("itemimagebutton",1);

		v2s32 pos = padding;
		pos.X += stof(v_pos[0]) * (float)spacing.X;
		pos.Y += stof(v_pos[1]) * (float)spacing.Y;
		v2s32 geom;
		geom.X = (stof(v_geom[0]) * (float)spacing.X)-(spacing.X-imgsize.X);
		geom.Y = (stof(v_geom[1]) * (float)spacing.Y)-(spacing.Y-imgsize.Y);

		core::rect<s32> rect = core::rect<s32>(pos.X, pos.Y, pos.X+geom.X, pos.Y+geom.Y);

		if(data->bp_set != 2)
			errorstream<<"WARNING: invalid use of item_image_button without a size[] element"<<std::endl;

		IItemDefManager *idef = m_gamedef->idef();
		ItemStack item;
		item.deSerialize(item_name, idef);
		video::ITexture *texture = idef->getInventoryTexture(item.getDefinition(idef).name, m_gamedef);
		std::string tooltip = item.getDefinition(idef).description;

		label = unescape_string(label);
		FieldSpec spec = FieldSpec(
			narrow_to_wide(name.c_str()),
			narrow_to_wide(label.c_str()),
			narrow_to_wide(item_name.c_str()),
			258+m_fields.size()
		);

		gui::IGUIButton *e = Environment->addButton(rect, this, spec.fid, spec.flabel.c_str());

		if (spec.fname == data->focused_fieldname) {
			Environment->setFocus(e);
		}

		e->setUseAlphaChannel(true);
		e->setImage(texture);
		e->setPressedImage(texture);
		e->setScaleImage(true);
		spec.ftype = f_Button;
		rect+=data->basepos-padding;
		spec.rect=rect;
		if (tooltip!="")
			spec.tooltip=tooltip;
		m_fields.push_back(spec);
		return;
	}
	errorstream<< "Invalid ItemImagebutton element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseBox(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if (parts.size() == 3) {
		std::vector<std::string> v_pos = split(parts[0],',');
		std::vector<std::string> v_geom = split(parts[1],',');

		MY_CHECKPOS("box",0);
		MY_CHECKGEOM("box",1);

		v2s32 pos = padding + AbsoluteRect.UpperLeftCorner;
		pos.X += stof(v_pos[0]) * (float) spacing.X;
		pos.Y += stof(v_pos[1]) * (float) spacing.Y;

		v2s32 geom;
		geom.X = stof(v_geom[0]) * (float)spacing.X;
		geom.Y = stof(v_geom[1]) * (float)spacing.Y;

		video::SColor tmp_color;

		if (parseColor(parts[2], tmp_color, false)) {
			BoxDrawSpec spec(pos, geom, tmp_color);

			m_boxes.push_back(spec);
		}
		else {
			errorstream<< "Invalid Box element(" << parts.size() << "): '" << element << "'  INVALID COLOR"  << std::endl;
		}
		return;
	}
	errorstream<< "Invalid Box element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseBackgroundColor(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 1) || (parts.size() == 2)) {
		parseColor(parts[0],m_bgcolor,false);

		if (parts.size() == 2) {
			std::string fullscreen = parts[1];
			m_bgfullscreen = is_yes(fullscreen);
		}
		return;
	}
	errorstream<< "Invalid bgcolor element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseListColors(parserData* data,std::string element)
{
	std::vector<std::string> parts = split(element,';');

	if ((parts.size() == 2) || (parts.size() == 3) || (parts.size() == 5)) {
		parseColor(parts[0], m_slotbg_n, false);
		parseColor(parts[1], m_slotbg_h, false);
		
		if (parts.size() >= 3) {
			if (parseColor(parts[2], m_slotbordercolor, false)) {
				m_slotborder = true;
			}
		}
		if (parts.size() == 5) {
			video::SColor tmp_color;

			if (parseColor(parts[3], tmp_color, false))
				m_tooltip_element->setBackgroundColor(tmp_color);
			if (parseColor(parts[4], tmp_color, false))
				m_tooltip_element->setOverrideColor(tmp_color);
		}
		return;
	}
	errorstream<< "Invalid listcolors element(" << parts.size() << "): '" << element << "'"  << std::endl;
}

void GUIFormSpecMenu::parseElement(parserData* data,std::string element)
{
	//some prechecks
	if (element == "")
		return;

	std::vector<std::string> parts = split(element,'[');

	// ugly workaround to keep compatibility
	if (parts.size() > 2) {
		if (trim(parts[0]) == "image") {
			for (unsigned int i=2;i< parts.size(); i++) {
				parts[1] += "[" + parts[i];
			}
		}
		else { return; }
	}

	if (parts.size() < 2) {
		return;
	}

	std::string type = trim(parts[0]);
	std::string description = trim(parts[1]);

	if (type == "size") {
		parseSize(data,description);
		return;
	}

	if (type == "invsize") {
		log_deprecated("Deprecated formspec element \"invsize\" is used");
		parseSize(data,description);
		return;
	}

	if (type == "list") {
		parseList(data,description);
		return;
	}

	if (type == "checkbox") {
		parseCheckbox(data,description);
		return;
	}

	if (type == "image") {
		parseImage(data,description);
		return;
	}

	if (type == "item_image") {
		parseItemImage(data,description);
		return;
	}

	if ((type == "button") || (type == "button_exit")) {
		parseButton(data,description,type);
		return;
	}

	if (type == "background") {
		parseBackground(data,description);
		return;
	}

	if (type == "tableoptions"){
		parseTableOptions(data,description);
		return;
	}

	if (type == "tablecolumns"){
		parseTableColumns(data,description);
		return;
	}

	if (type == "table"){
		parseTable(data,description);
		return;
	}

	if (type == "textlist"){
		parseTextList(data,description);
		return;
	}

	if (type == "dropdown"){
		parseDropDown(data,description);
		return;
	}

	if (type == "pwdfield") {
		parsePwdField(data,description);
		return;
	}

	if ((type == "field") || (type == "textarea")){
		parseField(data,description,type);
		return;
	}

	if (type == "label") {
		parseLabel(data,description);
		return;
	}

	if (type == "vertlabel") {
		parseVertLabel(data,description);
		return;
	}

	if (type == "item_image_button") {
		parseItemImageButton(data,description);
		return;
	}

	if ((type == "image_button") || (type == "image_button_exit")) {
		parseImageButton(data,description,type);
		return;
	}

	if (type == "tabheader") {
		parseTabHeader(data,description);
		return;
	}

	if (type == "box") {
		parseBox(data,description);
		return;
	}

	if (type == "bgcolor") {
		parseBackgroundColor(data,description);
		return;
	}

	if (type == "listcolors") {
		parseListColors(data,description);
		return;
	}

	// Ignore others
	infostream
		<< "Unknown DrawSpec: type="<<type<<", data=\""<<description<<"\""
		<<std::endl;
}



void GUIFormSpecMenu::regenerateGui(v2u32 screensize)
{
	parserData mydata;

	//preserve tables
	for (u32 i = 0; i < m_tables.size(); ++i) {
		std::wstring tablename = m_tables[i].first.fname;
		GUITable *table = m_tables[i].second;
		mydata.table_dyndata[tablename] = table->getDynamicData();
	}

	//preserve focus
	gui::IGUIElement *focused_element = Environment->getFocus();
	if (focused_element && focused_element->getParent() == this) {
		s32 focused_id = focused_element->getID();
		if (focused_id > 257) {
			for (u32 i=0; i<m_fields.size(); i++) {
				if (m_fields[i].fid == focused_id) {
					mydata.focused_fieldname =
						m_fields[i].fname;
					break;
				}
			}
		}
	}

	// Remove children
	removeChildren();

	mydata.size= v2s32(100,100);
	mydata.helptext_h = 15;
	mydata.screensize = screensize;

	// Base position of contents of form
	mydata.basepos = getBasePos();

	// State of basepos, 0 = not set, 1= set by formspec, 2 = set by size[] element
	// Used to adjust form size automatically if needed
	// A proceed button is added if there is no size[] element
	mydata.bp_set = 0;

	
	/* Convert m_init_draw_spec to m_inventorylists */
	
	m_inventorylists.clear();
	m_images.clear();
	m_backgrounds.clear();
	m_itemimages.clear();
	m_tables.clear();
	m_checkboxes.clear();
	m_fields.clear();
	m_boxes.clear();

	// Set default values (fits old formspec values)
	m_bgcolor = video::SColor(140,0,0,0);
	m_bgfullscreen = false;

	m_slotbg_n = video::SColor(255,128,128,128);
	m_slotbg_h = video::SColor(255,192,192,192);

	m_slotbordercolor = video::SColor(200,0,0,0);
	m_slotborder = false;

	m_clipbackground = false;
	// Add tooltip
	{
		// Note: parent != this so that the tooltip isn't clipped by the menu rectangle
		m_tooltip_element = Environment->addStaticText(L"",core::rect<s32>(0,0,110,18));
		m_tooltip_element->enableOverrideColor(true);
		m_tooltip_element->setBackgroundColor(video::SColor(255,56,121,65));
		m_tooltip_element->setDrawBackground(true);
		m_tooltip_element->setDrawBorder(true);
		m_tooltip_element->setOverrideColor(video::SColor(255,255,255,255));
		m_tooltip_element->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
		m_tooltip_element->setWordWrap(false);
		//we're not parent so no autograb for this one!
		m_tooltip_element->grab();
	}


	std::vector<std::string> elements = split(m_formspec_string,']');
	for (unsigned int i=0;i< elements.size();i++) {
		parseElement(&mydata,elements[i]);
	}

	// If there's fields, add a Proceed button
	if (m_fields.size() && mydata.bp_set != 2)
	{
		// if the size wasn't set by an invsize[] or size[] adjust it now to fit all the fields
		mydata.rect = core::rect<s32>(
				mydata.screensize.X/2 - 580/2,
				mydata.screensize.Y/2 - 300/2,
				mydata.screensize.X/2 + 580/2,
				mydata.screensize.Y/2 + 240/2+(m_fields.size()*60)
		);
		DesiredRect = mydata.rect;
		recalculateAbsolutePosition(false);
		mydata.basepos = getBasePos();

		{
			v2s32 pos = mydata.basepos;
			pos.Y = ((m_fields.size()+2)*60);

			v2s32 size = DesiredRect.getSize();
			mydata.rect = core::rect<s32>(size.X/2-70, pos.Y, (size.X/2-70)+140, pos.Y+30);
			wchar_t* text = wgettext("Proceed");
			Environment->addButton(mydata.rect, this, 257, text);
			delete[] text;
		}

	}

	//set initial focus if parser didn't set it
	focused_element = Environment->getFocus();
	if (!focused_element
			|| !isMyChild(focused_element)
			|| focused_element->getType() == gui::EGUIET_TAB_CONTROL)
		setInitialFocus();
}

GUIFormSpecMenu::ItemSpec GUIFormSpecMenu::getItemAtPos(v2s32 p) const
{
	core::rect<s32> imgrect(0,0,imgsize.X,imgsize.Y);
	
	for(u32 i=0; i<m_inventorylists.size(); i++)
	{
		const ListDrawSpec &s = m_inventorylists[i];

		for(s32 i=0; i<s.geom.X*s.geom.Y; i++)
		{
			s32 item_i = i + s.start_item_i;
			s32 x = (i%s.geom.X) * spacing.X;
			s32 y = (i/s.geom.X) * spacing.Y;
			v2s32 p0(x,y);
			core::rect<s32> rect = imgrect + s.pos + p0;
			if(rect.isPointInside(p))
			{
				return ItemSpec(s.inventoryloc, s.listname, item_i);
			}
		}
	}

	return ItemSpec(InventoryLocation(), "", -1);
}

void GUIFormSpecMenu::drawList(const ListDrawSpec &s, int phase)
{
	video::IVideoDriver* driver = Environment->getVideoDriver();

	// Get font
	gui::IGUIFont *font = NULL;
	gui::IGUISkin* skin = Environment->getSkin();
	if (skin)
		font = skin->getFont();
	
	Inventory *inv = m_invmgr->getInventory(s.inventoryloc);
	if(!inv){
		infostream<<"GUIFormSpecMenu::drawList(): WARNING: "
				<<"The inventory location "
				<<"\""<<s.inventoryloc.dump()<<"\" doesn't exist"
				<<std::endl;
		return;
	}
	InventoryList *ilist = inv->getList(s.listname);
	if(!ilist){
		infostream<<"GUIFormSpecMenu::drawList(): WARNING: "
				<<"The inventory list \""<<s.listname<<"\" @ \""
				<<s.inventoryloc.dump()<<"\" doesn't exist"
				<<std::endl;
		return;
	}
	
	core::rect<s32> imgrect(0,0,imgsize.X,imgsize.Y);
	
	for(s32 i=0; i<s.geom.X*s.geom.Y; i++)
	{
		s32 item_i = i + s.start_item_i;
		if(item_i >= (s32) ilist->getSize())
			break;
		s32 x = (i%s.geom.X) * spacing.X;
		s32 y = (i/s.geom.X) * spacing.Y;
		v2s32 p(x,y);
		core::rect<s32> rect = imgrect + s.pos + p;
		ItemStack item;
		if(ilist)
			item = ilist->getItem(item_i);

		bool selected = m_selected_item
			&& m_invmgr->getInventory(m_selected_item->inventoryloc) == inv
			&& m_selected_item->listname == s.listname
			&& m_selected_item->i == item_i;
		bool hovering = rect.isPointInside(m_pointer);

		if(phase == 0)
		{
			if(hovering)
				driver->draw2DRectangle(m_slotbg_h, rect, &AbsoluteClippingRect);
			else
				driver->draw2DRectangle(m_slotbg_n, rect, &AbsoluteClippingRect);
		}

		//Draw inv slot borders
		if (m_slotborder) {
			s32 x1 = rect.UpperLeftCorner.X;
			s32 y1 = rect.UpperLeftCorner.Y;
			s32 x2 = rect.LowerRightCorner.X;
			s32 y2 = rect.LowerRightCorner.Y;
			s32 border = 1;
			driver->draw2DRectangle(m_slotbordercolor,
				core::rect<s32>(v2s32(x1 - border, y1 - border),
								v2s32(x2 + border, y1)), NULL);
			driver->draw2DRectangle(m_slotbordercolor,
				core::rect<s32>(v2s32(x1 - border, y2),
								v2s32(x2 + border, y2 + border)), NULL);
			driver->draw2DRectangle(m_slotbordercolor,
				core::rect<s32>(v2s32(x1 - border, y1),
								v2s32(x1, y2)), NULL);
			driver->draw2DRectangle(m_slotbordercolor,
				core::rect<s32>(v2s32(x2, y1),
								v2s32(x2 + border, y2)), NULL);
		}

		if(phase == 1)
		{
			// Draw item stack
			if(selected)
			{
				item.takeItem(m_selected_amount);
			}
			if(!item.empty())
			{
				drawItemStack(driver, font, item,
						rect, &AbsoluteClippingRect, m_gamedef);
			}

			// Draw tooltip
			std::string tooltip_text = "";
			if(hovering && !m_selected_item)
				tooltip_text = item.getDefinition(m_gamedef->idef()).description;
			if(tooltip_text != "")
			{
				m_tooltip_element->setVisible(true);
				this->bringToFront(m_tooltip_element);
				m_tooltip_element->setText(narrow_to_wide(tooltip_text).c_str());
				s32 tooltip_x = m_pointer.X + 15;
				s32 tooltip_y = m_pointer.Y + 15;
				s32 tooltip_width = m_tooltip_element->getTextWidth() + 15;
				s32 tooltip_height = m_tooltip_element->getTextHeight() + 5;
				m_tooltip_element->setRelativePosition(core::rect<s32>(
						core::position2d<s32>(tooltip_x, tooltip_y),
						core::dimension2d<s32>(tooltip_width, tooltip_height)));
			}
		}
	}
}

void GUIFormSpecMenu::drawSelectedItem()
{
	if(!m_selected_item)
		return;

	video::IVideoDriver* driver = Environment->getVideoDriver();

	// Get font
	gui::IGUIFont *font = NULL;
	gui::IGUISkin* skin = Environment->getSkin();
	if (skin)
		font = skin->getFont();
	
	Inventory *inv = m_invmgr->getInventory(m_selected_item->inventoryloc);
	assert(inv);
	InventoryList *list = inv->getList(m_selected_item->listname);
	assert(list);
	ItemStack stack = list->getItem(m_selected_item->i);
	stack.count = m_selected_amount;

	core::rect<s32> imgrect(0,0,imgsize.X,imgsize.Y);
	core::rect<s32> rect = imgrect + (m_pointer - imgrect.getCenter());
	drawItemStack(driver, font, stack, rect, NULL, m_gamedef);
}

void GUIFormSpecMenu::drawMenu()
{
	if(m_form_src){
		std::string newform = m_form_src->getForm();
		if(newform != m_formspec_string){
			m_formspec_string = newform;
			regenerateGui(m_screensize_old);
		}
	}

	m_pointer = m_device->getCursorControl()->getPosition();

	updateSelectedItem();

	gui::IGUISkin* skin = Environment->getSkin();
	if (!skin)
		return;
	video::IVideoDriver* driver = Environment->getVideoDriver();
	
	v2u32 screenSize = driver->getScreenSize();
	core::rect<s32> allbg(0, 0, screenSize.X ,	screenSize.Y);
	if (m_bgfullscreen)
		driver->draw2DRectangle(m_bgcolor, allbg, &allbg);
	else
		driver->draw2DRectangle(m_bgcolor, AbsoluteRect, &AbsoluteClippingRect);

	m_tooltip_element->setVisible(false);

	/*
		Draw backgrounds
	*/
	for(u32 i=0; i<m_backgrounds.size(); i++)
	{
		const ImageDrawSpec &spec = m_backgrounds[i];
		video::ITexture *texture = m_tsrc->getTexture(spec.name);

		if (texture != 0) {
			// Image size on screen
			core::rect<s32> imgrect(0, 0, spec.geom.X, spec.geom.Y);
			// Image rectangle on screen
			core::rect<s32> rect = imgrect + spec.pos;

			if (m_clipbackground) {
				core::dimension2d<s32> absrec_size = AbsoluteRect.getSize();
				rect = core::rect<s32>(AbsoluteRect.UpperLeftCorner.X - spec.pos.X,
									AbsoluteRect.UpperLeftCorner.Y - spec.pos.Y,
									AbsoluteRect.UpperLeftCorner.X + absrec_size.Width + spec.pos.X,
									AbsoluteRect.UpperLeftCorner.Y + absrec_size.Height + spec.pos.Y);
			}

			const video::SColor color(255,255,255,255);
			const video::SColor colors[] = {color,color,color,color};
			driver->draw2DImage(texture, rect,
				core::rect<s32>(core::position2d<s32>(0,0),
						core::dimension2di(texture->getOriginalSize())),
				NULL/*&AbsoluteClippingRect*/, colors, true);
		}
		else {
			errorstream << "GUIFormSpecMenu::drawMenu() Draw backgrounds unable to load texture:" << std::endl;
			errorstream << "\t" << spec.name << std::endl;
		}
	}
	
	/*
		Draw Boxes
	*/
	for(u32 i=0; i<m_boxes.size(); i++)
	{
		const BoxDrawSpec &spec = m_boxes[i];

		irr::video::SColor todraw = spec.color;

		todraw.setAlpha(140);

		core::rect<s32> rect(spec.pos.X,spec.pos.Y,
							spec.pos.X + spec.geom.X,spec.pos.Y + spec.geom.Y);

		driver->draw2DRectangle(todraw, rect, 0);
	}
	/*
		Draw images
	*/
	for(u32 i=0; i<m_images.size(); i++)
	{
		const ImageDrawSpec &spec = m_images[i];
		video::ITexture *texture = m_tsrc->getTexture(spec.name);

		if (texture != 0) {
			const core::dimension2d<u32>& img_origsize = texture->getOriginalSize();
			// Image size on screen
			core::rect<s32> imgrect;

			if (spec.scale)
				imgrect = core::rect<s32>(0,0,spec.geom.X, spec.geom.Y);
			else {

				imgrect = core::rect<s32>(0,0,img_origsize.Width,img_origsize.Height);
			}
			// Image rectangle on screen
			core::rect<s32> rect = imgrect + spec.pos;
			const video::SColor color(255,255,255,255);
			const video::SColor colors[] = {color,color,color,color};
			driver->draw2DImage(texture, rect,
				core::rect<s32>(core::position2d<s32>(0,0),img_origsize),
				NULL/*&AbsoluteClippingRect*/, colors, true);
		}
		else {
			errorstream << "GUIFormSpecMenu::drawMenu() Draw images unable to load texture:" << std::endl;
			errorstream << "\t" << spec.name << std::endl;
		}
	}
	
	/*
		Draw item images
	*/
	for(u32 i=0; i<m_itemimages.size(); i++)
	{
		if (m_gamedef == 0)
			break;

		const ImageDrawSpec &spec = m_itemimages[i];
		IItemDefManager *idef = m_gamedef->idef();
		ItemStack item;
		item.deSerialize(spec.name, idef);
		video::ITexture *texture = idef->getInventoryTexture(item.getDefinition(idef).name, m_gamedef);
		// Image size on screen
		core::rect<s32> imgrect(0, 0, spec.geom.X, spec.geom.Y);
		// Image rectangle on screen
		core::rect<s32> rect = imgrect + spec.pos;
		const video::SColor color(255,255,255,255);
		const video::SColor colors[] = {color,color,color,color};
		driver->draw2DImage(texture, rect,
			core::rect<s32>(core::position2d<s32>(0,0),
					core::dimension2di(texture->getOriginalSize())),
			NULL/*&AbsoluteClippingRect*/, colors, true);
	}
	
	/*
		Draw items
		Phase 0: Item slot rectangles
		Phase 1: Item images; prepare tooltip
	*/
	int start_phase=0;
	for(int phase=start_phase; phase<=1; phase++)
	for(u32 i=0; i<m_inventorylists.size(); i++)
	{
		drawList(m_inventorylists[i], phase);
	}

	/*
		Call base class
	*/
	gui::IGUIElement::draw();
	
	/*
		Draw fields/buttons tooltips
	*/
	for(u32 i=0; i<m_fields.size(); i++)
	{
		const FieldSpec &spec = m_fields[i];
		if (spec.tooltip != "")
		{
			core::rect<s32> rect = spec.rect;
			if (rect.isPointInside(m_pointer))
			{
				m_tooltip_element->setVisible(true);
				this->bringToFront(m_tooltip_element);
				m_tooltip_element->setText(narrow_to_wide(spec.tooltip).c_str());
				s32 tooltip_x = m_pointer.X + 15;
				s32 tooltip_y = m_pointer.Y + 15;
				s32 tooltip_width = m_tooltip_element->getTextWidth() + 15;
				s32 tooltip_height = m_tooltip_element->getTextHeight() + 5;
				m_tooltip_element->setRelativePosition(core::rect<s32>(
				core::position2d<s32>(tooltip_x, tooltip_y),
				core::dimension2d<s32>(tooltip_width, tooltip_height)));
			}
		}
	}
	
	/*
		Draw dragged item stack
	*/
	drawSelectedItem();
}

void GUIFormSpecMenu::updateSelectedItem()
{
	// If the selected stack has become empty for some reason, deselect it.
	// If the selected stack has become inaccessible, deselect it.
	// If the selected stack has become smaller, adjust m_selected_amount.
	ItemStack selected = verifySelectedItem();

	// WARNING: BLACK MAGIC
	// See if there is a stack suited for our current guess.
	// If such stack does not exist, clear the guess.
	if(m_selected_content_guess.name != "" &&
			selected.name == m_selected_content_guess.name &&
			selected.count == m_selected_content_guess.count){
		// Selected item fits the guess. Skip the black magic.
	}
	else if(m_selected_content_guess.name != ""){
		bool found = false;
		for(u32 i=0; i<m_inventorylists.size() && !found; i++){
			const ListDrawSpec &s = m_inventorylists[i];
			Inventory *inv = m_invmgr->getInventory(s.inventoryloc);
			if(!inv)
				continue;
			InventoryList *list = inv->getList(s.listname);
			if(!list)
				continue;
			for(s32 i=0; i<s.geom.X*s.geom.Y && !found; i++){
				u32 item_i = i + s.start_item_i;
				if(item_i >= list->getSize())
					continue;
				ItemStack stack = list->getItem(item_i);
				if(stack.name == m_selected_content_guess.name &&
						stack.count == m_selected_content_guess.count){
					found = true;
					infostream<<"Client: Changing selected content guess to "
							<<s.inventoryloc.dump()<<" "<<s.listname
							<<" "<<item_i<<std::endl;
					delete m_selected_item;
					m_selected_item = new ItemSpec(s.inventoryloc, s.listname, item_i);
					m_selected_amount = stack.count;
				}
			}
		}
		if(!found){
			infostream<<"Client: Discarding selected content guess: "
					<<m_selected_content_guess.getItemString()<<std::endl;
			m_selected_content_guess.name = "";
		}
	}

	// If craftresult is nonempty and nothing else is selected, select it now.
	if(!m_selected_item)
	{
		for(u32 i=0; i<m_inventorylists.size(); i++)
		{
			const ListDrawSpec &s = m_inventorylists[i];
			if(s.listname == "craftpreview")
			{
				Inventory *inv = m_invmgr->getInventory(s.inventoryloc);
				InventoryList *list = inv->getList("craftresult");
				if(list && list->getSize() >= 1 && !list->getItem(0).empty())
				{
					m_selected_item = new ItemSpec;
					m_selected_item->inventoryloc = s.inventoryloc;
					m_selected_item->listname = "craftresult";
					m_selected_item->i = 0;
					m_selected_amount = 0;
					m_selected_dragging = false;
					break;
				}
			}
		}
	}

	// If craftresult is selected, keep the whole stack selected
	if(m_selected_item && m_selected_item->listname == "craftresult")
	{
		m_selected_amount = verifySelectedItem().count;
	}
}

ItemStack GUIFormSpecMenu::verifySelectedItem()
{
	// If the selected stack has become empty for some reason, deselect it.
	// If the selected stack has become inaccessible, deselect it.
	// If the selected stack has become smaller, adjust m_selected_amount.
	// Return the selected stack.

	if(m_selected_item)
	{
		if(m_selected_item->isValid())
		{
			Inventory *inv = m_invmgr->getInventory(m_selected_item->inventoryloc);
			if(inv)
			{
				InventoryList *list = inv->getList(m_selected_item->listname);
				if(list && (u32) m_selected_item->i < list->getSize())
				{
					ItemStack stack = list->getItem(m_selected_item->i);
					if(m_selected_amount > stack.count)
						m_selected_amount = stack.count;
					if(!stack.empty())
						return stack;
				}
			}
		}

		// selection was not valid
		delete m_selected_item;
		m_selected_item = NULL;
		m_selected_amount = 0;
		m_selected_dragging = false;
	}
	return ItemStack();
}

void GUIFormSpecMenu::acceptInput(FormspecQuitMode quitmode=quit_mode_no)
{
	if(m_text_dst)
	{
		std::map<std::string, std::string> fields;

		if (quitmode == quit_mode_accept) {
			fields["quit"] = "true";
		}

		if (quitmode == quit_mode_cancel) {
			fields["quit"] = "true";
			m_text_dst->gotText(fields);
			return;
		}

		if (current_keys_pending.key_down) {
			fields["key_down"] = "true";
			current_keys_pending.key_down = false;
		}

		if (current_keys_pending.key_up) {
			fields["key_up"] = "true";
			current_keys_pending.key_up = false;
		}

		if (current_keys_pending.key_enter) {
			fields["key_enter"] = "true";
			current_keys_pending.key_enter = false;
		}

		if (current_keys_pending.key_escape) {
			fields["key_escape"] = "true";
			current_keys_pending.key_escape = false;
		}

		for(u32 i=0; i<m_fields.size(); i++)
		{
			const FieldSpec &s = m_fields[i];
			if(s.send)
			{
				if(s.ftype == f_Button)
				{
					fields[wide_to_narrow(s.fname.c_str())] = wide_to_narrow(s.flabel.c_str());
				}
				else if(s.ftype == f_Table) {
					GUITable *table = getTable(s.fname);
					if (table) {
						fields[wide_to_narrow(s.fname.c_str())]
							= table->checkEvent();
					}
				}
				else if(s.ftype == f_DropDown) {
					// no dynamic cast possible due to some distributions shipped
					// without rtti support in irrlicht
					IGUIElement * element = getElementFromId(s.fid);
					gui::IGUIComboBox *e = NULL;
					if ((element) && (element->getType() == gui::EGUIET_COMBO_BOX)) {
						e = static_cast<gui::IGUIComboBox*>(element);
					}
					s32 selected = e->getSelected();
					if (selected >= 0) {
						fields[wide_to_narrow(s.fname.c_str())] =
							wide_to_narrow(e->getItem(selected));
					}
				}
				else if (s.ftype == f_TabHeader) {
					// no dynamic cast possible due to some distributions shipped
					// without rtti support in irrlicht
					IGUIElement * element = getElementFromId(s.fid);
					gui::IGUITabControl *e = NULL;
					if ((element) && (element->getType() == gui::EGUIET_TAB_CONTROL)) {
						e = static_cast<gui::IGUITabControl*>(element);
					}

					if (e != 0) {
						std::stringstream ss;
						ss << (e->getActiveTab() +1);
						fields[wide_to_narrow(s.fname.c_str())] = ss.str();
					}
				}
				else if (s.ftype == f_CheckBox) {
					// no dynamic cast possible due to some distributions shipped
					// without rtti support in irrlicht
					IGUIElement * element = getElementFromId(s.fid);
					gui::IGUICheckBox *e = NULL;
					if ((element) && (element->getType() == gui::EGUIET_CHECK_BOX)) {
						e = static_cast<gui::IGUICheckBox*>(element);
					}

					if (e != 0) {
						if (e->isChecked())
							fields[wide_to_narrow(s.fname.c_str())] = "true";
						else
							fields[wide_to_narrow(s.fname.c_str())] = "false";
					}
				}
				else
				{
					IGUIElement* e = getElementFromId(s.fid);
					if(e != NULL)
					{
						fields[wide_to_narrow(s.fname.c_str())] = wide_to_narrow(e->getText());
					}
				}
			}
		}

		m_text_dst->gotText(fields);
	}
}

bool GUIFormSpecMenu::preprocessEvent(const SEvent& event)
{
	// Fix Esc/Return key being eaten by checkboxen and tables
	if(event.EventType==EET_KEY_INPUT_EVENT)
	{
		KeyPress kp(event.KeyInput);
		if (kp == EscapeKey || kp == getKeySetting("keymap_inventory")
				|| event.KeyInput.Key==KEY_RETURN)
		{
			gui::IGUIElement *focused = Environment->getFocus();
			if (focused && isMyChild(focused) &&
					(focused->getType() == gui::EGUIET_LIST_BOX ||
					 focused->getType() == gui::EGUIET_CHECK_BOX)) {
				OnEvent(event);
				return true;
			}
		}
	}
	// Mouse wheel events: send to hovered element instead of focused
	if(event.EventType==EET_MOUSE_INPUT_EVENT
			&& event.MouseInput.Event == EMIE_MOUSE_WHEEL)
	{
		s32 x = event.MouseInput.X;
		s32 y = event.MouseInput.Y;
		gui::IGUIElement *hovered =
			Environment->getRootGUIElement()->getElementFromPoint(
				core::position2d<s32>(x, y));
		if (hovered && isMyChild(hovered)) {
			hovered->OnEvent(event);
			return true;
		}
	}
	return false;
}

bool GUIFormSpecMenu::OnEvent(const SEvent& event)
{
	if(event.EventType==EET_KEY_INPUT_EVENT)
	{
		KeyPress kp(event.KeyInput);
		if (event.KeyInput.PressedDown && (kp == EscapeKey ||
			kp == getKeySetting("keymap_inventory")))
		{
			if (m_allowclose){
				doPause = false;
				acceptInput(quit_mode_cancel);
				quitMenu();
			} else {
				m_text_dst->gotText(narrow_to_wide("MenuQuit"));
			}
			return true;
		}
		if (event.KeyInput.PressedDown &&
			(event.KeyInput.Key==KEY_RETURN ||
			 event.KeyInput.Key==KEY_UP ||
			 event.KeyInput.Key==KEY_DOWN)
			) {


			switch (event.KeyInput.Key) {
				case KEY_RETURN:
					current_keys_pending.key_enter = true;
					break;
				case KEY_UP:
					current_keys_pending.key_up = true;
					break;
				case KEY_DOWN:
					current_keys_pending.key_down = true;
					break;
				break;
				default:
					//can't happen at all!
					assert("reached a source line that can't ever been reached" == 0);
					break;
			}
			if (current_keys_pending.key_enter && m_allowclose) {
				acceptInput(quit_mode_accept);
				quitMenu();
			}
			else {
				acceptInput();
			}
			return true;
		}

	}
	if(event.EventType==EET_MOUSE_INPUT_EVENT
			&& event.MouseInput.Event != EMIE_MOUSE_MOVED)
	{
		// Mouse event other than movement

		// Get selected item and hovered/clicked item (s)

		updateSelectedItem();
		ItemSpec s = getItemAtPos(m_pointer);

		Inventory *inv_selected = NULL;
		Inventory *inv_s = NULL;

		if(m_selected_item)
		{
			inv_selected = m_invmgr->getInventory(m_selected_item->inventoryloc);
			assert(inv_selected);
			assert(inv_selected->getList(m_selected_item->listname) != NULL);
		}

		u32 s_count = 0;

		if(s.isValid())
		do{ // breakable
			inv_s = m_invmgr->getInventory(s.inventoryloc);

			if(!inv_s){
				errorstream<<"InventoryMenu: The selected inventory location "
						<<"\""<<s.inventoryloc.dump()<<"\" doesn't exist"
						<<std::endl;
				s.i = -1;  // make it invalid again
				break;
			}

			InventoryList *list = inv_s->getList(s.listname);
			if(list == NULL){
				verbosestream<<"InventoryMenu: The selected inventory list \""
						<<s.listname<<"\" does not exist"<<std::endl;
				s.i = -1;  // make it invalid again
				break;
			}

			if((u32)s.i >= list->getSize()){
				infostream<<"InventoryMenu: The selected inventory list \""
						<<s.listname<<"\" is too small (i="<<s.i<<", size="
						<<list->getSize()<<")"<<std::endl;
				s.i = -1;  // make it invalid again
				break;
			}

			s_count = list->getItem(s.i).count;
		}while(0);

		bool identical = (m_selected_item != NULL) && s.isValid() &&
			(inv_selected == inv_s) &&
			(m_selected_item->listname == s.listname) &&
			(m_selected_item->i == s.i);

		// buttons: 0 = left, 1 = right, 2 = middle
		// up/down: 0 = down (press), 1 = up (release), 2 = unknown event
		int button = 0;
		int updown = 2;
		if(event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
			{ button = 0; updown = 0; }
		else if(event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN)
			{ button = 1; updown = 0; }
		else if(event.MouseInput.Event == EMIE_MMOUSE_PRESSED_DOWN)
			{ button = 2; updown = 0; }
		else if(event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
			{ button = 0; updown = 1; }
		else if(event.MouseInput.Event == EMIE_RMOUSE_LEFT_UP)
			{ button = 1; updown = 1; }
		else if(event.MouseInput.Event == EMIE_MMOUSE_LEFT_UP)
			{ button = 2; updown = 1; }

		// Set this number to a positive value to generate a move action
		// from m_selected_item to s.
		u32 move_amount = 0;

		// Set this number to a positive value to generate a drop action
		// from m_selected_item.
		u32 drop_amount = 0;

		// Set this number to a positive value to generate a craft action at s.
		u32 craft_amount = 0;

		if(updown == 0)
		{
			// Some mouse button has been pressed

			//infostream<<"Mouse button "<<button<<" pressed at p=("
			//	<<p.X<<","<<p.Y<<")"<<std::endl;

			m_selected_dragging = false;

			if(s.isValid() && s.listname == "craftpreview")
			{
				// Craft preview has been clicked: craft
				craft_amount = (button == 2 ? 10 : 1);
			}
			else if(m_selected_item == NULL)
			{
				if(s_count != 0)
				{
					// Non-empty stack has been clicked: select it
					m_selected_item = new ItemSpec(s);

					if(button == 1)  // right
						m_selected_amount = (s_count + 1) / 2;
					else if(button == 2)  // middle
						m_selected_amount = MYMIN(s_count, 10);
					else  // left
						m_selected_amount = s_count;

					m_selected_dragging = true;
				}
			}
			else  // m_selected_item != NULL
			{
				assert(m_selected_amount >= 1);

				if(s.isValid())
				{
					// Clicked a slot: move
					if(button == 1)  // right
						move_amount = 1;
					else if(button == 2)  // middle
						move_amount = MYMIN(m_selected_amount, 10);
					else  // left
						move_amount = m_selected_amount;

					if(identical)
					{
						if(move_amount >= m_selected_amount)
							m_selected_amount = 0;
						else
							m_selected_amount -= move_amount;
						move_amount = 0;
					}
				}
				else if (!getAbsoluteClippingRect().isPointInside(m_pointer))
				{
					// Clicked outside of the window: drop
					if(button == 1)  // right
						drop_amount = 1;
					else if(button == 2)  // middle
						drop_amount = MYMIN(m_selected_amount, 10);
					else  // left
						drop_amount = m_selected_amount;
				}
			}
		}
		else if(updown == 1)
		{
			// Some mouse button has been released

			//infostream<<"Mouse button "<<button<<" released at p=("
			//	<<p.X<<","<<p.Y<<")"<<std::endl;

			if(m_selected_item != NULL && m_selected_dragging && s.isValid())
			{
				if(!identical)
				{
					// Dragged to different slot: move all selected
					move_amount = m_selected_amount;
				}
			}
			else if(m_selected_item != NULL && m_selected_dragging &&
				!(getAbsoluteClippingRect().isPointInside(m_pointer)))
			{
				// Dragged outside of window: drop all selected
				drop_amount = m_selected_amount;
			}

			m_selected_dragging = false;
		}

		// Possibly send inventory action to server
		if(move_amount > 0)
		{
			// Send IACTION_MOVE

			assert(m_selected_item && m_selected_item->isValid());
			assert(s.isValid());

			assert(inv_selected && inv_s);
			InventoryList *list_from = inv_selected->getList(m_selected_item->listname);
			InventoryList *list_to = inv_s->getList(s.listname);
			assert(list_from && list_to);
			ItemStack stack_from = list_from->getItem(m_selected_item->i);
			ItemStack stack_to = list_to->getItem(s.i);

			// Check how many items can be moved
			move_amount = stack_from.count = MYMIN(move_amount, stack_from.count);
			ItemStack leftover = stack_to.addItem(stack_from, m_gamedef->idef());
			// If source stack cannot be added to destination stack at all,
			// they are swapped
			if(leftover.count == stack_from.count && leftover.name == stack_from.name)
			{
				m_selected_amount = stack_to.count;
				// In case the server doesn't directly swap them but instead
				// moves stack_to somewhere else, set this
				m_selected_content_guess = stack_to;
				m_selected_content_guess_inventory = s.inventoryloc;
			}
			// Source stack goes fully into destination stack
			else if(leftover.empty())
			{
				m_selected_amount -= move_amount;
				m_selected_content_guess = ItemStack(); // Clear
			}
			// Source stack goes partly into destination stack
			else
			{
				move_amount -= leftover.count;
				m_selected_amount -= move_amount;
				m_selected_content_guess = ItemStack(); // Clear
			}

			infostream<<"Handing IACTION_MOVE to manager"<<std::endl;
			IMoveAction *a = new IMoveAction();
			a->count = move_amount;
			a->from_inv = m_selected_item->inventoryloc;
			a->from_list = m_selected_item->listname;
			a->from_i = m_selected_item->i;
			a->to_inv = s.inventoryloc;
			a->to_list = s.listname;
			a->to_i = s.i;
			m_invmgr->inventoryAction(a);
		}
		else if(drop_amount > 0)
		{
			m_selected_content_guess = ItemStack(); // Clear

			// Send IACTION_DROP

			assert(m_selected_item && m_selected_item->isValid());
			assert(inv_selected);
			InventoryList *list_from = inv_selected->getList(m_selected_item->listname);
			assert(list_from);
			ItemStack stack_from = list_from->getItem(m_selected_item->i);

			// Check how many items can be dropped
			drop_amount = stack_from.count = MYMIN(drop_amount, stack_from.count);
			assert(drop_amount > 0 && drop_amount <= m_selected_amount);
			m_selected_amount -= drop_amount;

			infostream<<"Handing IACTION_DROP to manager"<<std::endl;
			IDropAction *a = new IDropAction();
			a->count = drop_amount;
			a->from_inv = m_selected_item->inventoryloc;
			a->from_list = m_selected_item->listname;
			a->from_i = m_selected_item->i;
			m_invmgr->inventoryAction(a);
		}
		else if(craft_amount > 0)
		{
			m_selected_content_guess = ItemStack(); // Clear

			// Send IACTION_CRAFT

			assert(s.isValid());
			assert(inv_s);

			infostream<<"Handing IACTION_CRAFT to manager"<<std::endl;
			ICraftAction *a = new ICraftAction();
			a->count = craft_amount;
			a->craft_inv = s.inventoryloc;
			m_invmgr->inventoryAction(a);
		}

		// If m_selected_amount has been decreased to zero, deselect
		if(m_selected_amount == 0)
		{
			delete m_selected_item;
			m_selected_item = NULL;
			m_selected_amount = 0;
			m_selected_dragging = false;
			m_selected_content_guess = ItemStack();
		}
	}
	if(event.EventType==EET_GUI_EVENT)
	{

		if(event.GUIEvent.EventType==gui::EGET_TAB_CHANGED
						&& isVisible())
		{
			// find the element that was clicked
			for(u32 i=0; i<m_fields.size(); i++)
			{
				FieldSpec &s = m_fields[i];
				// if its a button, set the send field so
				// lua knows which button was pressed
				if ((s.ftype == f_TabHeader) && (s.fid == event.GUIEvent.Caller->getID()))
				{
					s.send = true;
					acceptInput();
					s.send = false;
					return true;
				}
			}
		}
		if(event.GUIEvent.EventType==gui::EGET_ELEMENT_FOCUS_LOST
				&& isVisible())
		{
			if(!canTakeFocus(event.GUIEvent.Element))
			{
				infostream<<"GUIFormSpecMenu: Not allowing focus change."
						<<std::endl;
				// Returning true disables focus change
				return true;
			}
		}
		if((event.GUIEvent.EventType==gui::EGET_BUTTON_CLICKED) ||
				(event.GUIEvent.EventType==gui::EGET_CHECKBOX_CHANGED))
		{
			unsigned int btn_id = event.GUIEvent.Caller->getID();

			if (btn_id == 257) {
				if (m_allowclose) {
					acceptInput(quit_mode_accept);
					quitMenu();
				} else {
					acceptInput();
					m_text_dst->gotText(narrow_to_wide("ExitButton"));
				}
				// quitMenu deallocates menu
				return true;
			}

			// find the element that was clicked
			for(u32 i=0; i<m_fields.size(); i++)
			{
				FieldSpec &s = m_fields[i];
				// if its a button, set the send field so
				// lua knows which button was pressed
				if (((s.ftype == f_Button) || (s.ftype == f_CheckBox)) &&
						(s.fid == event.GUIEvent.Caller->getID()))
				{
					s.send = true;
					if(s.is_exit){
						if (m_allowclose) {
							acceptInput(quit_mode_accept);
							quitMenu();
						} else {
							m_text_dst->gotText(narrow_to_wide("ExitButton"));
						}
						return true;
					}else{
						acceptInput();
						s.send = false;
						return true;
					}
				}
			}
		}
		if(event.GUIEvent.EventType==gui::EGET_EDITBOX_ENTER)
		{
			if(event.GUIEvent.Caller->getID() > 257)
			{

				if (m_allowclose) {
					acceptInput(quit_mode_accept);
					quitMenu();
				}
				else {
					current_keys_pending.key_enter = true;
					acceptInput();
				}
				// quitMenu deallocates menu
				return true;
			}
		}

		if(event.GUIEvent.EventType==gui::EGET_TABLE_CHANGED)
		{
			int current_id = event.GUIEvent.Caller->getID();
			if(current_id > 257)
			{
				// find the element that was clicked
				for(u32 i=0; i<m_fields.size(); i++)
				{
					FieldSpec &s = m_fields[i];
					// if it's a table, set the send field
					// so lua knows which table was changed
					if ((s.ftype == f_Table) && (s.fid == current_id))
					{
						s.send = true;
						acceptInput();
						s.send=false;
					}
				}
				return true;
			}
		}
	}

	return Parent ? Parent->OnEvent(event) : false;
}

bool GUIFormSpecMenu::parseColor(const std::string &value, video::SColor &color,
		bool quiet)
{
	if (!::parseColor(value, color) && !quiet) {
		errorstream<<"Invalid color: \""<<value<<"\""<<std::endl;
		return false;
	}
	return true;
}
