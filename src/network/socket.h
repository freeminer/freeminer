/*
database-dummy.cpp
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

#include <ostream>
#include <cstring>
#include "address.h"
#include "irrlichttypes.h"
#include "networkexceptions.h"

<<<<<<< HEAD:src/database-dummy.cpp
bool Database_Dummy::saveBlock(const v3s16 &pos, const std::string &data)
{
	m_database.set(getBlockAsString(pos), data);
	return true;
}

void Database_Dummy::loadBlock(const v3s16 &pos, std::string *block)
{
	auto i = getBlockAsString(pos);
	auto lock = m_database.lock_shared_rec();
	auto it = m_database.find(i);
	if (it == m_database.end())
		*block = "";
		return;
	*block = it->second;
}

bool Database_Dummy::deleteBlock(const v3s16 &pos)
{
	m_database.erase(getBlockAsString(pos));
	return true;
}

void Database_Dummy::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	dst.reserve(m_database.size());
	for(auto &x : m_database) {
		dst.push_back(getStringAsBlock(x.first));
	}
}
=======
extern bool socket_enable_debug_output;

void sockets_init();
void sockets_cleanup();

class UDPSocket
{
public:
	UDPSocket() = default;

	UDPSocket(bool ipv6);
	~UDPSocket();
	void Bind(Address addr);

	bool init(bool ipv6, bool noExceptions = false);

	void Send(const Address &destination, const void *data, int size);
	// Returns -1 if there is no data
	int Receive(Address &sender, void *data, int size);
	int GetHandle(); // For debugging purposes only
	void setTimeoutMs(int timeout_ms);
	// Returns true if there is data, false if timeout occurred
	bool WaitData(int timeout_ms);
>>>>>>> 5.5.0:src/network/socket.h

private:
	int m_handle;
	int m_timeout_ms;
	int m_addr_family;
};
