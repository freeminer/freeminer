/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "log.h"

#include "threading/mutex_auto_lock.h"
#include "debug.h"
#include "gettime.h"
#include "porting.h"
#include "settings.h"
#include "config.h"
#include "exceptions.h"
#include "util/numeric.h"
#include "log.h"
#include "filesys.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#if !defined(_WIN32)
#include <unistd.h> // isatty
#endif

#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>

class LevelTarget : public LogTarget {
public:
	LevelTarget(Logger &logger, LogLevel level, bool raw = false) :
		m_logger(logger),
		m_level(level),
		m_raw(raw)
	{}

	virtual bool hasOutput() override {
		return m_logger.hasOutput(m_level);
	}

	virtual void log(const std::string &buf) override {
		if (!m_raw) {
			m_logger.log(m_level, buf);
		} else {
			m_logger.logRaw(m_level, buf);
		}
	}

private:
	Logger &m_logger;
	LogLevel m_level;
	bool m_raw;
};

////
//// Globals
////

Logger g_logger;

#ifdef __ANDROID__
AndroidLogOutput stdout_output;
AndroidLogOutput stderr_output;
#else
StreamLogOutput stdout_output(std::cout);
StreamLogOutput stderr_output(std::cerr);
#endif

LevelTarget none_target_raw(g_logger, LL_NONE, true);
LevelTarget none_target(g_logger, LL_NONE);
LevelTarget error_target(g_logger, LL_ERROR);
LevelTarget warning_target(g_logger, LL_WARNING);
LevelTarget action_target(g_logger, LL_ACTION);
LevelTarget info_target(g_logger, LL_INFO);
LevelTarget verbose_target(g_logger, LL_VERBOSE);
LevelTarget trace_target(g_logger, LL_TRACE);

thread_local LogStream dstream(none_target);
thread_local LogStream rawstream(none_target_raw);
thread_local LogStream errorstream(error_target);
thread_local LogStream warningstream(warning_target);
thread_local LogStream actionstream(action_target);
thread_local LogStream infostream(info_target);
thread_local LogStream verbosestream(verbose_target);
thread_local LogStream tracestream(trace_target);
thread_local LogStream derr_con(verbose_target);
thread_local LogStream dout_con(trace_target);

// Android
#ifdef __ANDROID__

static unsigned int g_level_to_android[] = {
	ANDROID_LOG_INFO,     // LL_NONE
	ANDROID_LOG_ERROR,    // LL_ERROR
	ANDROID_LOG_WARN,     // LL_WARNING
	ANDROID_LOG_INFO,     // LL_ACTION
	ANDROID_LOG_DEBUG,    // LL_INFO
	ANDROID_LOG_VERBOSE,  // LL_VERBOSE
	ANDROID_LOG_VERBOSE,  // LL_TRACE
};

void AndroidLogOutput::logRaw(LogLevel lev, const std::string &line)
{
	static_assert(ARRLEN(g_level_to_android) == LL_MAX,
		"mismatch between android and internal loglevels");
	__android_log_write(g_level_to_android[lev], PROJECT_NAME_C, line.c_str());
}
#endif

///////////////////////////////////////////////////////////////////////////////


////
//// Logger
////

LogLevel Logger::stringToLevel(const std::string &name)
{
	if (name == "none")
		return LL_NONE;
	else if (name == "error")
		return LL_ERROR;
	else if (name == "warning")
		return LL_WARNING;
	else if (name == "action")
		return LL_ACTION;
	else if (name == "info")
		return LL_INFO;
	else if (name == "verbose")
		return LL_VERBOSE;
	else if (name == "trace")
		return LL_TRACE;
	else
		return LL_MAX;
}

void Logger::addOutput(ILogOutput *out)
{
	addOutputMaxLevel(out, (LogLevel)(LL_MAX - 1));
}

void Logger::addOutput(ILogOutput *out, LogLevel lev)
{
	addOutputMasked(out, LOGLEVEL_TO_MASKLEVEL(lev));
}

void Logger::addOutputMasked(ILogOutput *out, LogLevelMask mask)
{
	MutexAutoLock lock(m_mutex);
	for (size_t i = 0; i < LL_MAX; i++) {
		if (mask & LOGLEVEL_TO_MASKLEVEL(i)) {
			m_outputs[i].push_back(out);
			m_has_outputs[i] = true;
		}
	}
}

void Logger::addOutputMaxLevel(ILogOutput *out, LogLevel lev)
{
	MutexAutoLock lock(m_mutex);
	assert(lev < LL_MAX);
	for (size_t i = 0; i <= lev; i++) {
		m_outputs[i].push_back(out);
		m_has_outputs[i] = true;
	}
}

LogLevelMask Logger::removeOutput(ILogOutput *out)
{
	MutexAutoLock lock(m_mutex);
	LogLevelMask ret_mask = 0;
	for (size_t i = 0; i < LL_MAX; i++) {
		auto it = std::find(m_outputs[i].begin(), m_outputs[i].end(), out);
		if (it != m_outputs[i].end()) {
			ret_mask |= LOGLEVEL_TO_MASKLEVEL(i);
			m_outputs[i].erase(it);
			m_has_outputs[i] = !m_outputs[i].empty();
		}
	}
	return ret_mask;
}

void Logger::setLevelSilenced(LogLevel lev, bool silenced)
{
	m_silenced_levels[lev] = silenced;
}

void Logger::registerThread(const std::string &name)
{
	std::thread::id id = std::this_thread::get_id();
	MutexAutoLock lock(m_mutex);
	m_thread_names[id] = name;
}

void Logger::deregisterThread()
{
	std::thread::id id = std::this_thread::get_id();
	MutexAutoLock lock(m_mutex);
	m_thread_names.erase(id);
}

const char *Logger::getLevelLabel(LogLevel lev)
{
	static const char *names[] = {
		"",
		"ERROR",
		"WARNING",
		"ACTION",
		"INFO",
		"VERBOSE",
		"TRACE",
	};
	static_assert(ARRLEN(names) == LL_MAX,
		"mismatch between loglevel names and enum");
	assert(lev < LL_MAX && lev >= 0);
	return names[lev];
}

LogColor Logger::color_mode = LOG_COLOR_AUTO;

const std::string &Logger::getThreadName()
{
	std::thread::id id = std::this_thread::get_id();

	auto it = m_thread_names.find(id);
	if (it != m_thread_names.end())
		return it->second;

	thread_local std::string fallback_name;
	if (fallback_name.empty()) {
		std::ostringstream os;
		os << "#0x" << std::hex << id;
		fallback_name = os.str();
	}
	return fallback_name;
}

void Logger::log(LogLevel lev, const std::string &text)
{
	if (isLevelSilenced(lev))
		return;

	const std::string &thread_name = getThreadName();
	const char *label = getLevelLabel(lev);
	const std::string timestamp = getTimestamp();

	std::string line = timestamp;
	line.append(": ").append(label).append("[").append(thread_name)
		.append("]: ").append(text);

	logToOutputs(lev, line, timestamp, thread_name, text);
}

void Logger::logRaw(LogLevel lev, const std::string &text)
{
	if (isLevelSilenced(lev))
		return;

	logToOutputsRaw(lev, text);
}

void Logger::logToOutputsRaw(LogLevel lev, const std::string &line)
{
	MutexAutoLock lock(m_mutex);
	for (size_t i = 0; i != m_outputs[lev].size(); i++)
		m_outputs[lev][i]->logRaw(lev, line);
}

void Logger::logToOutputs(LogLevel lev, const std::string &combined,
	const std::string &time, const std::string &thread_name,
	const std::string &payload_text)
{
	MutexAutoLock lock(m_mutex);
	for (size_t i = 0; i != m_outputs[lev].size(); i++)
		m_outputs[lev][i]->log(lev, combined, time, thread_name, payload_text);
}

////
//// *LogOutput methods
////

void FileLogOutput::setFile(const std::string &filename, s64 file_size_max)
{
	// Only move debug.txt if there is a valid maximum file size
	bool is_too_large = false;
	if (file_size_max > 0) {
		std::ifstream ifile(filename, std::ios::binary | std::ios::ate);
		if (ifile.good())
			is_too_large = ifile.tellg() > file_size_max;
	}
	if (is_too_large) {
		std::string filename_secondary = filename + ".1";
		actionstream << "The log file grew too big; it is moved to " <<
			filename_secondary << std::endl;
		fs::DeleteSingleFileOrEmptyDirectory(filename_secondary);
		fs::Rename(filename, filename_secondary);
	}

	// Intentionally not using open_ofstream() to keep the text mode
	if (!fs::OpenStream(*m_stream.rdbuf(), filename.c_str(), std::ios::out | std::ios::app, true, false))
		throw FileNotGoodException("Failed to open log file");

	m_stream << "\n\n"
		"-------------\n" <<
		"  Separator\n" <<
		"-------------\n" << std::endl;
}

StreamLogOutput::StreamLogOutput(std::ostream &stream) :
	m_stream(stream)
{
#if !defined(_WIN32)
	if (&stream == &std::cout)
		is_tty = isatty(STDOUT_FILENO);
	else if (&stream == &std::cerr)
		is_tty = isatty(STDERR_FILENO);
#endif
}

void StreamLogOutput::logRaw(LogLevel lev, const std::string &line)
{
	bool colored_message = (Logger::color_mode == LOG_COLOR_ALWAYS) ||
		(Logger::color_mode == LOG_COLOR_AUTO && is_tty);
	if (colored_message) {
		switch (lev) {
		case LL_ERROR:
			// error is red
			m_stream << "\033[91m";
			break;
		case LL_WARNING:
			// warning is yellow
			m_stream << "\033[93m";
			break;
		case LL_INFO:
			// info is a bit dark
			m_stream << "\033[37m";
			break;
		case LL_VERBOSE:
		case LL_TRACE:
			// verbose is darker than info
			m_stream << "\033[2m";
			break;
		default:
			// action is white
			colored_message = false;
		}
	}

	m_stream << line << std::endl;

	if (colored_message) {
		// reset to white color
		m_stream << "\033[0m";
	}
}

void LogOutputBuffer::updateLogLevel()
{
	const std::string &conf_loglev = g_settings->get("chat_log_level");
	LogLevel log_level = Logger::stringToLevel(conf_loglev);
	if (log_level == LL_MAX) {
		warningstream << "Supplied unrecognized chat_log_level; "
			"showing none." << std::endl;
		log_level = LL_NONE;
	}

	m_logger.removeOutput(this);
	m_logger.addOutputMaxLevel(this, log_level);
}

void LogOutputBuffer::logRaw(LogLevel lev, const std::string &line)
{
	std::string color;

	if (!g_settings->getBool("disable_escape_sequences")) {
		switch (lev) {
		case LL_ERROR: // red
			color = "\x1b(c@#F00)";
			break;
		case LL_WARNING: // yellow
			color = "\x1b(c@#EE0)";
			break;
		case LL_INFO: // grey
			color = "\x1b(c@#BBB)";
			break;
		case LL_VERBOSE: // dark grey
		case LL_TRACE:
			color = "\x1b(c@#888)";
			break;
		default: break;
		}
	}
	MutexAutoLock lock(m_buffer_mutex);
	m_buffer.emplace(color.append(line));
}
