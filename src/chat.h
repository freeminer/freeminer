/*
chat.h
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

#ifndef CHAT_HEADER
#define CHAT_HEADER

#include "irrlichttypes.h"
#include <string>
#include <vector>
#include <list>

#include "FMColoredString.h"

// Chat console related classes

struct ChatLine
{
	// age in seconds
	f32 age;
	// name of sending player, or empty if sent by server
	std::wstring name;
	// message text
	std::wstring text;

	ChatLine(std::wstring a_name, std::wstring a_text):
		age(0.0),
		name(a_name),
		text(a_text)
	{
	}
};

struct ChatFormattedFragment
{
	// text string
	FMColoredString text;
	// starting column
	u32 column;
	// formatting
	//u8 bold:1;
};

struct ChatFormattedLine
{
	// Array of text fragments
	std::vector<ChatFormattedFragment> fragments;
	// true if first line of one formatted ChatLine
	bool first;
};

class ChatBuffer
{
public:
	ChatBuffer(u32 scrollback);
	~ChatBuffer();

	// Append chat line
	// Removes oldest chat line if scrollback size is reached
	void addLine(std::wstring name, std::wstring text);

	// Remove all chat lines
	void clear();

	// Get number of lines currently in buffer.
	u32 getLineCount() const;
	// Get scrollback size, maximum number of lines in buffer.
	u32 getScrollback() const;
	// Get reference to i-th chat line.
	const ChatLine& getLine(u32 index) const;

	// Increase each chat line's age by dtime.
	void step(f32 dtime);
	// Delete oldest N chat lines.
	void deleteOldest(u32 count);
	// Delete lines older than maxAge.
	void deleteByAge(f32 maxAge);

	// Get number of columns, 0 if reformat has not been called yet.
	u32 getColumns() const;
	// Get number of rows, 0 if reformat has not been called yet.
	u32 getRows() const;
	// Update console size and reformat all formatted lines.
	void reformat(u32 cols, u32 rows);
	// Get formatted line for a given row (0 is top of screen).
	// Only valid after reformat has been called at least once
	const ChatFormattedLine& getFormattedLine(u32 row) const;
	// Scrolling in formatted buffer (relative)
	// positive rows == scroll up, negative rows == scroll down
	void scroll(s32 rows);
	// Scrolling in formatted buffer (absolute)
	void scrollAbsolute(s32 scroll);
	// Scroll to bottom of buffer (newest)
	void scrollBottom();
	// Scroll to top of buffer (oldest)
	void scrollTop();

	// Format a chat line for the given number of columns.
	// Appends the formatted lines to the destination array and
	// returns the number of formatted lines.
	u32 formatChatLine(const ChatLine& line, u32 cols,
			std::vector<ChatFormattedLine>& destination) const;

protected:
	s32 getTopScrollPos() const;
	s32 getBottomScrollPos() const;

private:
	// Scrollback size
	u32 m_scrollback;
	// Array of unformatted chat lines
	std::vector<ChatLine> m_unformatted;

	// Number of character columns in console
	u32 m_cols;
	// Number of character rows in console
	u32 m_rows;
	// Scroll position (console's top line index into m_formatted)
	s32 m_scroll;
	// Array of formatted lines
	std::vector<ChatFormattedLine> m_formatted;
	// Empty formatted line, for error returns
	ChatFormattedLine m_empty_formatted_line;
};

class ChatPrompt
{
public:
	ChatPrompt(std::wstring prompt, u32 history_limit);
	~ChatPrompt();

	// Input character or string
	void input(wchar_t ch);
	void input(const std::wstring &str);

	// Submit, clear and return current line
	std::wstring submit();

	// Clear the current line
	void clear();

	// Replace the current line with the given text
	void replace(std::wstring line);

	// Add a line to history
	void historyPush(std::wstring line);
	// Select previous command from history
	void historyPrev();
	// Select next command from history
	void historyNext();

	// Nick completion
	void nickCompletion(const std::list<std::string>& names, bool backwards);

	// Update console size and reformat the visible portion of the prompt
	void reformat(u32 cols);
	// Get visible portion of the prompt.
	std::wstring getVisiblePortion() const;
	// Get cursor position (relative to visible portion). -1 if invalid
	s32 getVisibleCursorPosition() const;

	// Cursor operations
	enum CursorOp {
		CURSOROP_MOVE,
		CURSOROP_DELETE
	};

	// Cursor operation direction
	enum CursorOpDir {
		CURSOROP_DIR_LEFT,
		CURSOROP_DIR_RIGHT
	};

	// Cursor operation scope
	enum CursorOpScope {
		CURSOROP_SCOPE_CHARACTER,
		CURSOROP_SCOPE_WORD,
		CURSOROP_SCOPE_LINE
	};

	// Cursor operation
	// op specifies whether it's a move or delete operation
	// dir specifies whether the operation goes left or right
	// scope specifies how far the operation will reach (char/word/line)
	// Examples:
	//   cursorOperation(CURSOROP_MOVE, CURSOROP_DIR_RIGHT, CURSOROP_SCOPE_LINE)
	//     moves the cursor to the end of the line.
	//   cursorOperation(CURSOROP_DELETE, CURSOROP_DIR_LEFT, CURSOROP_SCOPE_WORD)
	//     deletes the word to the left of the cursor.
	void cursorOperation(CursorOp op, CursorOpDir dir, CursorOpScope scope);

protected:
	// set m_view to ensure that 0 <= m_view <= m_cursor < m_view + m_cols
	// if line can be fully shown, set m_view to zero
	// else, also ensure m_view <= m_line.size() + 1 - m_cols
	void clampView();

private:
	// Prompt prefix
	std::wstring m_prompt;
	// Currently edited line
	std::wstring m_line;
	// History buffer
	std::vector<std::wstring> m_history;
	// History index (0 <= m_history_index <= m_history.size())
	u32 m_history_index;
	// Maximum number of history entries
	u32 m_history_limit;

	// Number of columns excluding columns reserved for the prompt
	s32 m_cols;
	// Start of visible portion (index into m_line)
	s32 m_view;
	// Cursor (index into m_line)
	s32 m_cursor;

	// Last nick completion start (index into m_line)
	s32 m_nick_completion_start;
	// Last nick completion start (index into m_line)
	s32 m_nick_completion_end;
};

class ChatBackend
{
public:
	ChatBackend();
	~ChatBackend();

	// Add chat message
	void addMessage(std::wstring name, std::wstring text);
	// Parse and add unparsed chat message
	void addUnparsedMessage(std::wstring line);

	// Get the console buffer
	ChatBuffer& getConsoleBuffer();
	// Get the recent messages buffer
	ChatBuffer& getRecentBuffer();
	// Concatenate all recent messages
	std::wstring getRecentChat();
	// Get the console prompt
	ChatPrompt& getPrompt();

	// Reformat all buffers
	void reformat(u32 cols, u32 rows);

	// Clear all recent messages
	void clearRecentChat();

	// Age recent messages
	void step(float dtime);

	// Scrolling
	void scroll(s32 rows);
	void scrollPageDown();
	void scrollPageUp();

private:
	ChatBuffer m_console_buffer;
	ChatBuffer m_recent_buffer;
	ChatPrompt m_prompt;
};

#endif

