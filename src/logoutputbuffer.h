/*
logoutputbuffer.h
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

#ifndef LOGOUTPUTBUFFER_HEADER
#define LOGOUTPUTBUFFER_HEADER

#include "log.h"
#include <queue>

class LogOutputBuffer : public ILogOutput
{
public:
	LogOutputBuffer(LogMessageLevel maxlev)
	{
		log_add_output(this, maxlev);
	}
	~LogOutputBuffer()
	{
		log_remove_output(this);
	}
	virtual void printLog(const std::string &line)
	{
		m_buf.push(line);
	}
	std::string get()
	{
		if(empty())
			return "";
		std::string s = m_buf.front();
		m_buf.pop();
		return s;
	}
	bool empty()
	{
		return m_buf.empty();
	}
private:
	std::queue<std::string> m_buf;
};

#endif

