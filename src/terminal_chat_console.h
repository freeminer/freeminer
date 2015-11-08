/*
Minetest
Copyright (C) 2015 est31 <MTest31@outlook.com>

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

#ifndef TERMINAL_CHAT_CONSOLE_H
#define TERMINAL_CHAT_CONSOLE_H

#include "chat.h"
#include "threading/thread.h"
#include "chat_interface.h"
#include "log.h"

#include <sstream>

class TermLogOutput : public ILogOutput {
public:

	void logRaw(LogLevel lev, const std::string &line)
	{
		queue.push_back(std::make_pair(lev, line));
	}

	virtual void log(LogLevel lev, const std::string &combined,
		const std::string &time, const std::string &thread_name,
		const std::string &payload_text)
	{
		std::ostringstream os(std::ios_base::binary);
		os << time << ": [" << thread_name << "] " << payload_text;

		queue.push_back(std::make_pair(lev, os.str()));
	}

	MutexedQueue<std::pair<LogLevel, std::string> > queue;
};

class TerminalChatConsole : public Thread {
public:

	TerminalChatConsole() :
		Thread("TerminalThread"),
		m_log_level(LL_ACTION),
		m_utf8_bytes_to_wait(0),
		m_kill_requested(NULL),
		m_esc_mode(false),
		m_game_time(0),
		m_time_of_day(0)
	{}

	void setup(
		ChatInterface *iface,
		bool *kill_requested,
		const std::string &nick)
	{
		m_nick = nick;
		m_kill_requested = kill_requested;
		m_chat_interface = iface;
	}

	virtual void *run();

	// Highly required!
	void clearKillStatus() { m_kill_requested = NULL; }

	void stopAndWaitforThread();

private:
	// these have stupid names so that nobody missclassifies them
	// as curses functions. Oh, curses has stupid names too?
	// Well, at least it was worth a try...
	void initOfCurses();
	void deInitOfCurses();

	void draw_text();

	void typeChatMessage(const std::wstring &m);

	void handleInput(int ch, bool &complete_redraw_needed);

	void step(int ch);

	// Used to ensure the deinitialisation is always called.
	struct CursesInitHelper {
		TerminalChatConsole *cons;
		CursesInitHelper(TerminalChatConsole * a_console)
			: cons(a_console)
		{ cons->initOfCurses(); }
		~CursesInitHelper() { cons->deInitOfCurses(); }
	};

	int m_log_level;
	std::string m_nick;

	u8 m_utf8_bytes_to_wait;
	std::string m_pending_utf8_bytes;

	std::list<std::string> m_nicks;

	int m_cols;
	int m_rows;
	bool m_can_draw_text;

	bool *m_kill_requested;
	ChatBackend m_chat_backend;
	ChatInterface *m_chat_interface;

	TermLogOutput m_log_output;

	bool m_esc_mode;

	u64 m_game_time;
	u32 m_time_of_day;
};

extern TerminalChatConsole g_term_console;

#endif
