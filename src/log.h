/*
log.h
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

#ifndef LOG_HEADER
#define LOG_HEADER

#include <string>
#include <iostream> // for std::endl

/*
	Use this for logging everything.

	If you need to explicitly print something, use dstream or cout or cerr.
*/

enum LogMessageLevel {
	LMT_ERROR, /* Something failed ("invalid map data on disk, block (2,2,1)") */
	LMT_ACTION, /* In-game actions ("celeron55 placed block at (12,4,-5)") */
	LMT_INFO, /* More deep info ("saving map on disk (only_modified=true)") */
	LMT_VERBOSE, /* Flood-style ("loaded block (2,2,2) from disk") */
	LMT_NUM_VALUES,
};

class ILogOutput
{
public:
	ILogOutput() :
		silence(false)
	{}

	/* line: Full line with timestamp, level and thread */
	virtual void printLog(const std::string &line){};
	/* line: Full line with timestamp, level and thread */
	virtual void printLog(const std::string &line, enum LogMessageLevel lev){};
	/* line: Only actual printed text */
	virtual void printLog(enum LogMessageLevel lev, const std::string &line){};

	bool silence;
};

void log_add_output(ILogOutput *out, enum LogMessageLevel lev);
void log_add_output_maxlev(ILogOutput *out, enum LogMessageLevel lev);
void log_add_output_all_levs(ILogOutput *out);
void log_remove_output(ILogOutput *out);
void log_set_lev_silence(enum LogMessageLevel lev, bool silence);

void log_register_thread(const std::string &name);
void log_deregister_thread();

void log_printline(enum LogMessageLevel lev, const std::string &text);

#define LOGLINEF(lev, ...)\
{\
	char buf[10000];\
	snprintf(buf, 10000, __VA_ARGS__);\
	log_printline(lev, buf);\
}

extern std::ostream errorstream;
extern std::ostream actionstream;
extern std::ostream infostream;
extern std::ostream verbosestream;

extern bool log_trace_level_enabled;

#define TRACESTREAM(x){ if(log_trace_level_enabled) verbosestream x; }
#define TRACEDO(x){ if(log_trace_level_enabled){ x ;} }

extern std::ostream *dout_con_ptr;
extern std::ostream *derr_con_ptr;
extern std::ostream *dout_server_ptr;
extern std::ostream *derr_server_ptr;
#define dout_con (*dout_con_ptr)
#define derr_con (*derr_con_ptr)
#define dout_server (*dout_server_ptr)
#define derr_server (*derr_server_ptr)

#ifndef SERVER
extern std::ostream *dout_client_ptr;
extern std::ostream *derr_client_ptr;
#define dout_client (*dout_client_ptr)
#define derr_client (*derr_client_ptr)

#endif

#endif

