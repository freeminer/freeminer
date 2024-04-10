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

#pragma once

#include <algorithm>
#include "irrlichttypes.h"
#include <cassert>
#include <string>
#include <map>
#include <ostream>

#include "threading/mutex_auto_lock.h"
#include "util/timetaker.h"
#include "util/numeric.h"      // paging()

// Global profiler
class Profiler;
extern Profiler *g_profiler;

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
	Profiler();

	void add(const std::string &name, float value);
	void avg(const std::string &name, float value);
	void max(const std::string &name, float value);
	void clear();

	float getValue(const std::string &name) const;
	int getAvgCount(const std::string &name) const;
	u64 getElapsedMs() const;

	typedef std::map<std::string, float> GraphValues;

	// Returns the line count
	int print(std::ostream &o, u32 page = 1, u32 pagecount = 1);
	void getPage(GraphValues &o, u32 page, u32 pagecount);


	void graphAdd(const std::string &id, float value)
	{
		MutexAutoLock lock(m_mutex);
		std::map<std::string, float>::iterator i =
				m_graphvalues.find(id);
		if(i == m_graphvalues.end())
			m_graphvalues[id] = value;
		else
			i->second += value;
	}
	void graphGet(GraphValues &result)
	{
		MutexAutoLock lock(m_mutex);
		result = m_graphvalues;
		m_graphvalues.clear();
	}

	void remove(const std::string& name)
	{
		MutexAutoLock lock(m_mutex);
		m_data.erase(name);
	}

private:
	mutable std::mutex m_mutex;
	std::map<std::string, float> m_data;
	std::map<std::string, int> m_avgcounts;
	std::map<std::string, float> m_graphvalues;
	u64 m_start_time;
};

enum ScopeProfilerType{
	SPT_ADD,
	SPT_AVG,
	SPT_GRAPH_ADD,
	SPT_MAX
};

class ScopeProfiler
{
public:
	ScopeProfiler(Profiler *profiler, const std::string &name,
			ScopeProfilerType type = SPT_ADD);
	~ScopeProfiler();
private:
	Profiler *m_profiler = nullptr;
	std::string m_name;
	TimeTaker *m_timer = nullptr;
	enum ScopeProfilerType m_type;
};


// Global profiler
class Profiler;
extern Profiler *g_profiler;
