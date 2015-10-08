/*
quicktune.cpp
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

#include "quicktune.h"
#include "threading/mutex.h"
#include "threading/mutex_auto_lock.h"
#include "util/string.h"

std::string QuicktuneValue::getString()
{
	switch(type){
	case QVT_NONE:
		return "(none)";
	case QVT_FLOAT:
		return ftos(value_QVT_FLOAT.current);
	}
	return "<invalid type>";
}
void QuicktuneValue::relativeAdd(float amount)
{
	switch(type){
	case QVT_NONE:
		break;
	case QVT_FLOAT:
		value_QVT_FLOAT.current += amount * (value_QVT_FLOAT.max - value_QVT_FLOAT.min);
		if(value_QVT_FLOAT.current > value_QVT_FLOAT.max)
			value_QVT_FLOAT.current = value_QVT_FLOAT.max;
		if(value_QVT_FLOAT.current < value_QVT_FLOAT.min)
			value_QVT_FLOAT.current = value_QVT_FLOAT.min;
		break;
	}
}

static std::map<std::string, QuicktuneValue> g_values;
static std::vector<std::string> g_names;
Mutex *g_mutex = NULL;

static void makeMutex()
{
	if(!g_mutex){
		g_mutex = new Mutex();
	}
}

std::vector<std::string> getQuicktuneNames()
{
	return g_names;
}

QuicktuneValue getQuicktuneValue(const std::string &name)
{
	makeMutex();
	MutexAutoLock lock(*g_mutex);
	std::map<std::string, QuicktuneValue>::iterator i = g_values.find(name);
	if(i == g_values.end()){
		QuicktuneValue val;
		val.type = QVT_NONE;
		return val;
	}
	return i->second;
}

void setQuicktuneValue(const std::string &name, const QuicktuneValue &val)
{
	makeMutex();
	MutexAutoLock lock(*g_mutex);
	g_values[name] = val;
	g_values[name].modified = true;
}

void updateQuicktuneValue(const std::string &name, QuicktuneValue &val)
{
	makeMutex();
	MutexAutoLock lock(*g_mutex);
	std::map<std::string, QuicktuneValue>::iterator i = g_values.find(name);
	if(i == g_values.end()){
		g_values[name] = val;
		g_names.push_back(name);
		return;
	}
	QuicktuneValue &ref = i->second;
	if(ref.modified)
		val = ref;
	else{
		ref = val;
		ref.modified = false;
	}
}

