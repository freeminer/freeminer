/*
profiler.h
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

#ifndef PROFILER_HEADER
#define PROFILER_HEADER

#include <algorithm>
#include "irrlichttypes.h"
#include <string>
#include <map>

#include "jthread/jmutex.h"
#include "jthread/jmutexautolock.h"
#include "util/timetaker.h"
#include "util/numeric.h" // paging()
#include "debug.h" // assert()

/*
	Time profiler
*/
extern bool g_profiler_enabled;

struct ProfValue {
	unsigned int calls;
	float sum, min, max, avg;
	ProfValue(float value = 0) {
		calls = 1;
		sum = min = max = avg = value;
	}
	void add(float value = 0) {
		++calls;
		sum += value;
		min = std::min(min, value);
		max = std::max(max, value);
		//avg += (avg > value ? -1 : 1) * value/100;
		avg = sum/calls;
	}
};

class Profiler
{
public:
	Profiler()
	{
	}

	void add(const std::string &name, float value)
	{
		if(!g_profiler_enabled)
			return;
		JMutexAutoLock lock(m_mutex);
		{
			auto n = m_data.find(name);
			if(n == m_data.end())
				m_data[name] = ProfValue(value);
			else
				n->second.add(value);
		}
	}
	void avg(const std::string &name, float value)
	{
		add(name, value);
	}

	void clear()
	{
		JMutexAutoLock lock(m_mutex);
		m_data.clear();
	}

	void print(std::ostream &o)
	{
		printPage(o, 1, 1);
	}

	void printPage(std::ostream &o, u32 page, u32 pagecount)
	{
		JMutexAutoLock lock(m_mutex);

		u32 minindex, maxindex;
		paging(m_data.size(), page, pagecount, minindex, maxindex);

		for(auto & i : m_data)
		{
			if(maxindex == 0)
				break;
			maxindex--;

			if(minindex != 0)
			{
				minindex--;
				continue;
			}

			const std::string & name = i.first;
			o<<"  "<<name<<": ";
			s32 clampsize = 40;
			s32 space = clampsize - name.size();
			for(s32 j=0; j<space; j++)
			{
				if(j%2 == 0 && j < space - 1)
					o<<"-";
				else
					o<<" ";
			}

			if (i.second.sum == i.second.calls || !i.second.sum)
				o<<i.second.calls;
			else
				o<<i.second.calls<<" * "<<i.second.avg<<" = "<<i.second.sum;
			//o<<(i->second / avgcount);
			o<<std::endl;
		}
	}

	typedef std::map<std::string, float> GraphValues;

	void graphAdd(const std::string &id, float value)
	{
		JMutexAutoLock lock(m_mutex);
		std::map<std::string, float>::iterator i =
				m_graphvalues.find(id);
		if(i == m_graphvalues.end())
			m_graphvalues[id] = value;
		else
			i->second += value;
	}
	void graphGet(GraphValues &result)
	{
		JMutexAutoLock lock(m_mutex);
		result = m_graphvalues;
		m_graphvalues.clear();
	}

	void remove(const std::string& name)
	{
		JMutexAutoLock lock(m_mutex);
		m_data.erase(name);
	}

private:
	JMutex m_mutex;
	GraphValues m_graphvalues;
	std::map<std::string, ProfValue> m_data;
};

enum ScopeProfilerType{
	SPT_ADD,
	SPT_AVG,
	SPT_GRAPH_ADD
};

class ScopeProfiler
{
public:
	ScopeProfiler(Profiler *profiler, const std::string &name,
			enum ScopeProfilerType type = SPT_ADD):
		m_profiler(profiler),
		m_name(name),
		m_timer(NULL),
		m_type(type)
	{
		if(m_profiler)
			m_timer = new TimeTaker(m_name.c_str());
	}
	~ScopeProfiler()
	{
		if(m_timer)
		{
			float duration_ms = m_timer->stop(true);
			float duration = duration_ms / 1000.0;
			if(m_profiler){
				m_profiler->add(m_name, duration);
				if (m_type == SPT_GRAPH_ADD)
					m_profiler->graphAdd(m_name, duration);
			}
			delete m_timer;
		}
	}
private:
	Profiler *m_profiler;
	std::string m_name;
	TimeTaker *m_timer;
	enum ScopeProfilerType m_type;
};


// Global profiler
class Profiler;
extern Profiler *g_profiler;

#endif

