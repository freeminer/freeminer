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

#include <map>
#include <queue>
#include <string>
#include <fstream>
#include "threads.h"
#include "threading/mutex.h"
#include "threading/mutex_auto_lock.h"
#include "irrlichttypes.h"

class ILogOutput;

enum LogLevel {
	LL_NONE, // Special level that is always printed
	LL_ERROR,
	LL_WARNING,
	LL_ACTION,  // In-game actions
	LL_INFO,
	LL_VERBOSE,
	LL_MAX,
};

typedef u8 LogLevelMask;
#define LOGLEVEL_TO_MASKLEVEL(x) (1 << x)

class Logger {
public:
	void addOutput(ILogOutput *out);
	void addOutput(ILogOutput *out, LogLevel lev);
	void addOutputMasked(ILogOutput *out, LogLevelMask mask);
	void addOutputMaxLevel(ILogOutput *out, LogLevel lev);
	LogLevelMask removeOutput(ILogOutput *out);
	void setLevelSilenced(LogLevel lev, bool silenced);

	void registerThread(const std::string &name);
	void deregisterThread();

	void log(LogLevel lev, const std::string &text);
	// Logs without a prefix
	void logRaw(LogLevel lev, const std::string &text);

	void setTraceEnabled(bool enable) { m_trace_enabled = enable; }
	bool getTraceEnabled() { return m_trace_enabled; }

	static LogLevel stringToLevel(const std::string &name);
	static const std::string getLevelLabel(LogLevel lev);

private:
	void logToOutputsRaw(LogLevel, const std::string &line);
	void logToOutputs(LogLevel, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		const std::string &payload_text);

	const std::string getThreadName();

	std::vector<ILogOutput *> m_outputs[LL_MAX];

	// Should implement atomic loads and stores (even though it's only
	// written to when one thread has access currently).
	// Works on all known architectures (x86, ARM, MIPS).
	volatile bool m_silenced_levels[LL_MAX];
	std::map<threadid_t, std::string> m_thread_names;
protected:
	mutable Mutex m_mutex;
private:
	bool m_trace_enabled;
};

class ILogOutput {
public:
	virtual void logRaw(LogLevel, const std::string &line) = 0;
	virtual void log(LogLevel, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		const std::string &payload_text) = 0;
};

class ICombinedLogOutput : public ILogOutput {
public:
	void log(LogLevel lev, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		const std::string &payload_text)
	{
		logRaw(lev, combined);
	}
};

class StreamLogOutput : public ICombinedLogOutput {
public:
	StreamLogOutput(std::ostream &stream) :
		m_stream(stream)
	{
	}

	void logRaw(LogLevel lev, const std::string &line)
	{
		m_stream << line << std::endl;
	}

private:
	std::ostream &m_stream;
};

class FileLogOutput : public ICombinedLogOutput {
public:
	void open(const std::string &filename);

	void logRaw(LogLevel lev, const std::string &line)
	{
		m_stream << line << std::endl;
	}

private:
	std::ofstream m_stream;
};

class LogOutputBuffer : public ICombinedLogOutput {
public:
	LogOutputBuffer(Logger &logger, LogLevel lev) :
		m_logger(logger)
	{
		m_logger.addOutput(this, lev);
	}

	~LogOutputBuffer()
	{
		m_logger.removeOutput(this);
	}

	void logRaw(LogLevel lev, const std::string &line)
	{
		//MutexAutoLock lock(m_mutex);
		m_buffer.push(line);
	}

	bool empty()
	{
		//MutexAutoLock lock(m_mutex);
		return m_buffer.empty();
	}

	std::string get()
	{
		if (empty())
			return "";
		//MutexAutoLock lock(m_mutex);
		std::string s = m_buffer.front();
		m_buffer.pop();
		return s;
	}

private:
	mutable Mutex m_mutex;

	std::queue<std::string> m_buffer;
	Logger &m_logger;
};


extern StreamLogOutput stdout_output;
extern StreamLogOutput stderr_output;
extern std::ostream null_stream;

extern std::ostream *dout_con_ptr;
extern std::ostream *derr_con_ptr;
extern std::ostream *dout_server_ptr;
extern std::ostream *derr_server_ptr;

#ifndef SERVER
extern std::ostream *dout_client_ptr;
extern std::ostream *derr_client_ptr;
#endif

extern Logger g_logger;

// Writes directly to all LL_NONE log outputs for g_logger with no prefix.
extern std::ostream rawstream;

extern std::ostream errorstream;
extern std::ostream warningstream;
extern std::ostream actionstream;
extern std::ostream infostream;
extern std::ostream verbosestream;
extern std::ostream dstream;

#define TRACEDO(x) do {               \
	if (g_logger.getTraceEnabled()) { \
		x;                            \
	}                                 \
} while (0)

#define TRACESTREAM(x) TRACEDO(verbosestream x)

#define dout_con (*dout_con_ptr)
#define derr_con (*derr_con_ptr)
#define dout_server (*dout_server_ptr)
#define derr_server (*derr_server_ptr)

#ifndef SERVER
	#define dout_client (*dout_client_ptr)
	#define derr_client (*derr_client_ptr)
#endif


#endif
