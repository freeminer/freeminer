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

private:
	int m_handle;
	int m_timeout_ms;
	int m_addr_family;
};
