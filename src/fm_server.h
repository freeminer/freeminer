/*
Copyright (C) 2024 proller <proler@gmail.com>
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

#include "threading/thread_vector.h"

class Server;

class ServerThread : public thread_vector
{
public:
	ServerThread(Server *server);

	void *run();

private:
	Server *m_server;
};

class MapThread : public thread_vector
{
	Server *m_server;

public:
	MapThread(Server *server);

	void *run();
};

class SendBlocksThread : public thread_vector
{
	Server *m_server;

public:
	SendBlocksThread(Server *server);

	void *run();
};

class LiquidThread : public thread_vector
{
	Server *m_server;

public:
	LiquidThread(Server *server);

	void *run();
};

class EnvThread : public thread_vector
{
	Server *m_server;

public:
	EnvThread(Server *server);

	void *run();
};

class AbmThread : public thread_vector
{
	Server *m_server;

public:
	AbmThread(Server *server);

	void *run();
};

class AbmWorldThread : public thread_vector
{
	Server *m_server;

public:
	AbmWorldThread(Server *server);

	void *run();
};

class WorldMergeThread : public thread_vector
{
	Server *m_server;

public:
	WorldMergeThread(Server *server);

	void *run();
};
