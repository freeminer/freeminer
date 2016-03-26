/*
Copyright (C) 2016 proller

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

#ifndef FM_LAN_HEADER
#define FM_LAN_HEADER

#include <string>
#include <atomic>
#include "json/json.h"
#include "../threading/thread_pool.h"
#include "../threading/concurrent_map.h"


class lan_adv : public thread_pool {
public:
	void * run();

	lan_adv();
	void ask();
	void send_string(std::string str);

	void serve(unsigned short port);

	concurrent_map<std::string, Json::Value> collected;
	std::atomic_bool fresh;

private:
	unsigned short server_port = 0;
};


#endif