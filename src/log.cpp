/*
log.cpp
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

#include "log.h"

#include <map>
#include <list>
#include <sstream>
#include <algorithm>
#include "threads.h"
#include "debug.h"
#include "gettime.h"
#include "porting.h"
#include "config.h"
#include "jthread/jmutexautolock.h"

#ifdef __ANDROID__
unsigned int android_log_level_mapping[] {
		/* LMT_ERROR */   ANDROID_LOG_ERROR,
		/* LMT_ACTION */  ANDROID_LOG_WARN,
		/* LMT_INFO */    ANDROID_LOG_INFO,
		/* LMT_VERBOSE */ ANDROID_LOG_VERBOSE
	};
#endif

std::list<ILogOutput*> log_outputs[LMT_NUM_VALUES];
std::map<threadid_t, std::string> log_threadnames;
JMutex                            log_threadnamemutex;

void log_add_output(ILogOutput *out, enum LogMessageLevel lev)
{
	log_outputs[lev].push_back(out);
}

void log_add_output_maxlev(ILogOutput *out, enum LogMessageLevel lev)
{
	for(int i=0; i<=lev; i++)
		log_outputs[i].push_back(out);
}

void log_add_output_all_levs(ILogOutput *out)
{
	for(int i=0; i<LMT_NUM_VALUES; i++)
		log_outputs[i].push_back(out);
}

void log_remove_output(ILogOutput *out)
{
	for(int i=0; i<LMT_NUM_VALUES; i++){
		std::list<ILogOutput*>::iterator it =
				std::find(log_outputs[i].begin(), log_outputs[i].end(), out);
		if(it != log_outputs[i].end())
			log_outputs[i].erase(it);
	}
}

void log_set_lev_silence(enum LogMessageLevel lev, bool silence)
{
	log_threadnamemutex.Lock();

	for (std::list<ILogOutput *>::iterator
			it = log_outputs[lev].begin();
			it != log_outputs[lev].end();
			++it) {
		ILogOutput *out = *it;
		out->silence = silence;
	}

	log_threadnamemutex.Unlock();
}

void log_register_thread(const std::string &name)
{
	threadid_t id = get_current_thread_id();
	log_threadnamemutex.Lock();
	log_threadnames[id] = name;
	log_threadnamemutex.Unlock();
}

void log_deregister_thread()
{
	threadid_t id = get_current_thread_id();
	log_threadnamemutex.Lock();
	log_threadnames.erase(id);
	log_threadnamemutex.Unlock();
}

static std::string get_lev_string(enum LogMessageLevel lev)
{
	switch(lev){
	case LMT_ERROR:
		return "ERROR";
	case LMT_ACTION:
		return "ACTION";
	case LMT_INFO:
		return "INFO";
	case LMT_VERBOSE:
		return "VERBOSE";
	case LMT_NUM_VALUES:
		break;
	}
	return "(unknown level)";
}

void log_printline(enum LogMessageLevel lev, const std::string &text)
{
	log_threadnamemutex.Lock();
	std::string threadname = "(unknown thread)";
	std::map<threadid_t, std::string>::const_iterator i;
	i = log_threadnames.find(get_current_thread_id());
	if(i != log_threadnames.end())
		threadname = i->second;
	std::string levelname = get_lev_string(lev);
	std::ostringstream os(std::ios_base::binary);
	os<<getTimestamp()<<": "<<levelname<<"["<<threadname<<"]: "<<text;
	for(std::list<ILogOutput*>::iterator i = log_outputs[lev].begin();
			i != log_outputs[lev].end(); i++){
		ILogOutput *out = *i;
		if (out->silence)
			continue;

		out->printLog(os.str());
		out->printLog(os.str(), lev);
		out->printLog(lev, text);
	}
	log_threadnamemutex.Unlock();
}

class Logbuf : public std::streambuf
{
public:
	Logbuf(enum LogMessageLevel lev):
		m_lev(lev)
	{
	}

	~Logbuf()
	{
	}

	int overflow(int c)
	{
		bufchar(c);
		return c;
	}
	std::streamsize xsputn(const char *s, std::streamsize n)
	{
		for(int i=0; i<n; i++)
			bufchar(s[i]);
		return n;
	}

	void printbuf()
	{
		log_printline(m_lev, m_buf);
#ifdef __ANDROID__
		__android_log_print(android_log_level_mapping[m_lev], PROJECT_NAME, "%s", m_buf.c_str());
#endif
	}

	void bufchar(char c)
	{
		JMutexAutoLock lock(m_log_mutex);
		if(c == '\n' || c == '\r'){
			if(m_buf != "")
				printbuf();
			m_buf = "";
			return;
		}
		m_buf += c;
	}

private:
	JMutex m_log_mutex;
	enum LogMessageLevel m_lev;
	std::string m_buf;
};

Logbuf errorbuf(LMT_ERROR);
Logbuf actionbuf(LMT_ACTION);
Logbuf infobuf(LMT_INFO);
Logbuf verbosebuf(LMT_VERBOSE);
std::ostream errorstream(&errorbuf);
std::ostream actionstream(&actionbuf);
std::ostream infostream(&infobuf);
std::ostream verbosestream(&verbosebuf);

bool log_trace_level_enabled = false;

