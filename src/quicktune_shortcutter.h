/*
quicktune_shortcutter.h
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

#ifndef QVT_SHORTCUTTER_HEADER
#define QVT_SHORTCUTTER_HEADER

#include "quicktune.h"

class QuicktuneShortcutter
{
private:
	std::vector<std::string> m_names;
	u32 m_selected_i;
	std::string m_message;
public:
	bool hasMessage()
	{
		return m_message != "";
	}

	std::string getMessage()
	{
		std::string s = m_message;
		m_message = "";
		if(s != "")
			return std::string("[quicktune] ") + s;
		return "";
	}
	std::string getSelectedName()
	{
		if(m_selected_i < m_names.size())
			return m_names[m_selected_i];
		return "(nothing)";
	}
	void next()
	{
		m_names = getQuicktuneNames();
		if(m_selected_i < m_names.size()-1)
			m_selected_i++;
		else
			m_selected_i = 0;
		m_message = std::string("Selected \"")+getSelectedName()+"\"";
	}
	void prev()
	{
		m_names = getQuicktuneNames();
		if(m_selected_i > 0)
			m_selected_i--;
		else
			m_selected_i = m_names.size()-1;
		m_message = std::string("Selected \"")+getSelectedName()+"\"";
	}
	void inc()
	{
		QuicktuneValue val = getQuicktuneValue(getSelectedName());
		val.relativeAdd(0.05);
		m_message = std::string("\"")+getSelectedName()
				+"\" = "+val.getString();
		setQuicktuneValue(getSelectedName(), val);
	}
	void dec()
	{
		QuicktuneValue val = getQuicktuneValue(getSelectedName());
		val.relativeAdd(-0.05);
		m_message = std::string("\"")+getSelectedName()
				+"\" = "+val.getString();
		setQuicktuneValue(getSelectedName(), val);
	}
};

#endif

