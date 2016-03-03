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

#include "fm_lan.h"
#include "../socket.h"
#include "../util/string.h"
#include "../log_types.h"
#include "../settings.h"
#include "../version.h"
#include "networkprotocol.h"

//copypaste from ../socket.cpp
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Without this some of the network functions are not found on mingw
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define LAST_SOCKET_ERR() WSAGetLastError()
typedef SOCKET socket_t;
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#define LAST_SOCKET_ERR() (errno)
typedef int socket_t;
#endif


const static unsigned short int adv_port = 29998;
static std::string ask_str;

lan_adv::lan_adv() { }

void lan_adv::ask() {
	restart();

	if (ask_str.empty()) {
		Json::FastWriter writer;
		Json::Value j;
		j["cmd"] = "ask";
		j["proto"] = g_settings->get("server_proto");
		ask_str = writer.write(j);
	}

	send_string(ask_str);
}

void lan_adv::send_string(std::string str) {

	/*
		TODO:
		send from all interfaces
	*/
	try {
		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(adv_port);
		addr.sin_port = adv_port;
		addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		UDPSocket socket_send(false);
		int set_option_on = 1;
		setsockopt(socket_send.GetHandle(), SOL_SOCKET, SO_BROADCAST, (const char*) &set_option_on, sizeof(set_option_on));
		socket_send.Send(Address(addr), str.c_str(), str.size());
	} catch(std::exception e) {
		// errorstream << "udp broadcast send4 fail " << e.what() << "\n";
	}

	struct addrinfo hints { };
	hints.ai_socktype = SOCK_DGRAM;
	struct addrinfo *result;
	if (!getaddrinfo("ff02::1", nullptr, &hints, &result)) {
		for (auto info = result; info; info = info->ai_next) {
			try {
				sockaddr_in6 addr = *((struct sockaddr_in6*)info->ai_addr);
				addr.sin6_port = adv_port;
				UDPSocket socket_send(true);
				int set_option_on = 1;
				setsockopt(socket_send.GetHandle(), SOL_SOCKET, SO_BROADCAST, (const char*) &set_option_on, sizeof(set_option_on));
				socket_send.Send(Address(addr), str.c_str(), str.size());
			} catch(std::exception e) {
				// errorstream << "udp broadcast send6 fail " << e.what() << "\n";
			}
		}
		freeaddrinfo(result);
	}
}

void lan_adv::serve(unsigned short port) {
	server_port = port;
	restart();
}

void * lan_adv::run() {

	reg("LanAdv" + (server_port ? std::string("Server") : std::string("Client")));

	UDPSocket socket_recv(true);
	int set_option_off = 0, set_option_on = 1;
	setsockopt(socket_recv.GetHandle(), SOL_SOCKET, SO_REUSEADDR, (const char*) &set_option_on, sizeof(set_option_on));
#ifdef SO_REUSEPORT
	setsockopt(socket_recv.GetHandle(), SOL_SOCKET, SO_REUSEPORT, (const char*) &set_option_on, sizeof(set_option_on));
#endif
	setsockopt(socket_recv.GetHandle(), SOL_SOCKET, SO_BROADCAST, (const char*) &set_option_on, sizeof(set_option_on));
	setsockopt(socket_recv.GetHandle(), IPPROTO_IPV6, IPV6_V6ONLY, (const char*) &set_option_off, sizeof(set_option_off));
	socket_recv.setTimeoutMs(200);
	Address addr_bind(in6addr_any, adv_port);
	socket_recv.Bind(addr_bind);
	std::unordered_map<std::string, unsigned int> limiter;

	const auto proto = g_settings->get("server_proto");

	const unsigned int packet_maxsize = 16384;
	char buffer [packet_maxsize];
	Json::Reader reader;
	Json::FastWriter writer;
	std::string answer_str;
	if (server_port) {
		Json::Value server;

		server["name"]         = g_settings->get("server_name");
		server["description"]  = g_settings->get("server_description");
		server["version"]      = g_version_string;
		bool strict_checking = g_settings->getBool("strict_protocol_version_checking");
		server["proto_min"]    = strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MIN;
		server["proto_max"]    = strict_checking ? LATEST_PROTOCOL_VERSION : SERVER_PROTOCOL_VERSION_MAX;
		server["url"]          = g_settings->get("server_url");
		server["creative"]     = g_settings->getBool("creative_mode");
		server["damage"]       = g_settings->getBool("enable_damage");
		server["password"]     = g_settings->getBool("disallow_empty_password");
		server["pvp"]          = g_settings->getBool("enable_pvp");
		server["port"] = server_port;
		server["proto"] = g_settings->get("server_proto");

		answer_str = writer.write(server);

		send_string(answer_str);
	}
	while(!stopRequested()) {
		try {
			Address addr;
			int rlen = socket_recv.Receive(addr, buffer, packet_maxsize);
			if (rlen <= 0)
				continue;
			Json::Value p;
			if (!reader.parse(std::string(buffer, rlen), p)) {
				//errorstream << "cant parse "<< s << "\n";
				continue;
			}
			auto addr_str = addr.serializeString();
			auto now = porting::getTimeMs();
			//errorstream << " a=" << addr.serializeString() << " : " << addr.getPort() << " l=" << rlen << " b=" << p << " ;  server=" << server_port << "\n";
			if (server_port) {
				if (p["cmd"] == "ask" && limiter[addr_str] < now) {
					limiter[addr_str] = now + 3000;
					UDPSocket socket_send(true);
					addr.setPort(adv_port);
					socket_send.Send(addr, answer_str.c_str(), answer_str.size());
					infostream << "lan: want play " << addr_str << std::endl;
				}
			} else {
				if (p["cmd"] == "ask") {
					actionstream << "lan: want play " << addr_str << " " << p["proto"] << std::endl;
				}
				if (p["port"].isInt()) {
					p["address"] = addr_str;
					auto key = addr_str + ":" + p["port"].asString();
					if (p["cmd"].asString() == "shutdown") {
						//infostream << "server shutdown " << key << "\n";
						collected.erase(key);
						fresh = true;
					} else if (p["proto"] == proto) {
						if (!collected.count(key))
							actionstream << "lan server start " << key << "\n";
						collected.set(key, p);
						fresh = true;
					}
				}

				//errorstream<<" current list: ";for (auto & i : collected) {errorstream<< i.first <<" ; ";}errorstream<<std::endl;
			}

#if !EXEPTION_DEBUG
		} catch(std::exception &e) {
			errorstream << m_name << ": exception: " << e.what() << std::endl;
		} catch (...) {
			errorstream << m_name << ": Ooops..." << std::endl;
#else
		} catch (int) { //nothing
#endif
		}
	}

	if (server_port) {
		Json::Value answer_json;
		answer_json["port"] = server_port;
		answer_json["cmd"] = "shutdown";
		send_string(writer.write(answer_json));
	}

	return nullptr;
}
