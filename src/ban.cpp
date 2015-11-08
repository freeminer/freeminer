/*
ban.cpp
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

#include "ban.h"
#include <fstream>
#include "threading/mutex_auto_lock.h"
#include <sstream>
#include <set>
#include "strfnd.h"
#include "util/string.h"
#include "log.h"
#include "filesys.h"

BanManager::BanManager(const std::string &banfilepath):
		m_banfilepath(banfilepath),
		m_modified(false)
{
	try{
		load();
	}
	catch(SerializationError &e)
	{
		warningstream<<"BanManager: creating "
				<<m_banfilepath<<std::endl;
	}
}

BanManager::~BanManager()
{
	save();
}

void BanManager::load()
{
	MutexAutoLock lock(m_mutex);
	infostream<<"BanManager: loading from "<<m_banfilepath<<std::endl;
	std::ifstream is(m_banfilepath.c_str(), std::ios::binary);
	if(is.good() == false)
	{
		infostream<<"BanManager: failed loading from "<<m_banfilepath<<std::endl;
		throw SerializationError("BanManager::load(): Couldn't open file");
	}

	while(!is.eof() && is.good())
	{
		std::string line;
		std::getline(is, line, '\n');
		Strfnd f(line);
		std::string ip = trim(f.next("|"));
		std::string name = trim(f.next("|"));
		if(!ip.empty()) {
			m_ips[ip] = name;
		}
	}
	m_modified = false;
}

void BanManager::save()
{
	MutexAutoLock lock(m_mutex);
	infostream << "BanManager: saving to " << m_banfilepath << std::endl;
	std::ostringstream ss(std::ios_base::binary);

	for (StringMap::iterator it = m_ips.begin(); it != m_ips.end(); ++it)
		ss << it->first << "|" << it->second << "\n";

	if (!fs::safeWriteToFile(m_banfilepath, ss.str())) {
		infostream << "BanManager: failed saving to " << m_banfilepath << std::endl;
		throw SerializationError("BanManager::save(): Couldn't write file");
	}

	m_modified = false;
}

bool BanManager::isIpBanned(const std::string &ip)
{
	MutexAutoLock lock(m_mutex);
	return m_ips.find(ip) != m_ips.end();
}

std::string BanManager::getBanDescription(const std::string &ip_or_name)
{
	MutexAutoLock lock(m_mutex);
	std::string s = "";
	for (StringMap::iterator it = m_ips.begin(); it != m_ips.end(); ++it) {
		if (it->first  == ip_or_name || it->second == ip_or_name
				|| ip_or_name == "") {
			s += it->first + "|" + it->second + ", ";
		}
	}
	s = s.substr(0, s.size() - 2);
	return s;
}

std::string BanManager::getBanName(const std::string &ip)
{
	MutexAutoLock lock(m_mutex);
	StringMap::iterator it = m_ips.find(ip);
	if (it == m_ips.end())
		return "";
	return it->second;
}

void BanManager::add(const std::string &ip, const std::string &name)
{
	MutexAutoLock lock(m_mutex);
	m_ips[ip] = name;
	m_modified = true;
}

void BanManager::remove(const std::string &ip_or_name)
{
	MutexAutoLock lock(m_mutex);
	for (StringMap::iterator it = m_ips.begin(); it != m_ips.end();) {
		if ((it->first == ip_or_name) || (it->second == ip_or_name)) {
			m_ips.erase(it++);
		} else {
			++it;
		}
	}
	m_modified = true;
}


bool BanManager::isModified()
{
	MutexAutoLock lock(m_mutex);
	return m_modified;
}

