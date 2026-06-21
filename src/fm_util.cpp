/*
Copyright (C) 2026 proller <proler@gmail.com>
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

#include "fm_util.h"
#include "filesys.h"
#include "log.h"
#include "porting.h"
#include "settings.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

PIDFileHandler::PIDFileHandler(const Settings &cmd_args)
	: m_created(false)
{
	if (!cmd_args.exists("pid"))
		return;

	m_pidfile = cmd_args.get("pid");
	if (m_pidfile.empty())
		return;

	std::ofstream file(m_pidfile);
	if (!file.is_open()) {
		errorstream << "Failed to create PID file: " << m_pidfile << std::endl;
		return;
	}

#ifdef _WIN32
	file << GetCurrentProcessId() << std::endl;
#else
	file << getpid() << std::endl;
#endif
	file.close();

	verbosestream << "Created PID file: " << m_pidfile << std::endl;
	m_created = true;
}

PIDFileHandler::~PIDFileHandler()
{
	if (m_created) {
		if (std::remove(m_pidfile.c_str()) == 0) {
			verbosestream << "Removed PID file: " << m_pidfile << std::endl;
		} else {
			errorstream << "Failed to remove PID file: " << m_pidfile << std::endl;
		}
	}
}