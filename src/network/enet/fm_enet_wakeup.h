// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <enet/enet.h>
#include <stdexcept>
#ifndef _WIN32
#include <poll.h>
#endif

namespace con
{
// A loopback datagram interrupts the same socket wait used for incoming traffic.
// Only socket operations happen on producers; ENetHost stays on its worker.
class EnetWakeup
{
public:
	EnetWakeup()
	{
		m_read = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
		m_write = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
#if defined(ENET_IPV6)
		const char *loopback = "::1";
#else
		const char *loopback = "127.0.0.1";
#endif
		if (m_read == ENET_SOCKET_NULL || m_write == ENET_SOCKET_NULL ||
				enet_address_set_host_ip(&m_address, loopback) < 0 ||
				enet_socket_set_option(m_read, ENET_SOCKOPT_NONBLOCK, 1) < 0 ||
				enet_socket_set_option(m_write, ENET_SOCKOPT_NONBLOCK, 1) < 0 ||
				enet_socket_bind(m_read, &m_address) < 0 ||
				enet_socket_get_address(m_read, &m_address) < 0) {
			close();
			throw std::runtime_error("Unable to create ENet worker wakeup sockets");
		}
	}

	~EnetWakeup() { close(); }
	EnetWakeup(const EnetWakeup &) = delete;
	EnetWakeup &operator=(const EnetWakeup &) = delete;

	void notify()
	{
		char byte = 0;
		ENetBuffer buffer{};
		buffer.data = &byte;
		buffer.dataLength = sizeof(byte);
		// Nonblocking: a full queue already contains a wakeup. The worker also
		// bounds timer servicing, so a dropped local datagram cannot stall it.
		enet_socket_send(m_write, &m_address, &buffer, 1);
	}

	void drain()
	{
		char bytes[256];
		ENetBuffer buffer{};
		buffer.data = bytes;
		buffer.dataLength = sizeof(bytes);
		while (enet_socket_receive(m_read, nullptr, &buffer, 1) > 0) {
		}
	}

	bool wait(ENetSocket socket, unsigned timeout_ms)
	{
#ifdef _WIN32
		WSAPOLLFD descriptors[2]{};
#else
		pollfd descriptors[2]{};
#endif
		descriptors[0].fd = m_read;
		descriptors[0].events = POLLIN;
		descriptors[1].fd = socket;
		descriptors[1].events = POLLIN;
		const unsigned count = socket == ENET_SOCKET_NULL ? 1 : 2;
#ifdef _WIN32
		return WSAPoll(descriptors, count, static_cast<int>(timeout_ms)) > 0;
#else
		return poll(descriptors, count, static_cast<int>(timeout_ms)) > 0;
#endif
	}

private:
	void close()
	{
		if (m_read != ENET_SOCKET_NULL)
			enet_socket_destroy(m_read);
		if (m_write != ENET_SOCKET_NULL)
			enet_socket_destroy(m_write);
		m_read = m_write = ENET_SOCKET_NULL;
	}

	ENetSocket m_read = ENET_SOCKET_NULL;
	ENetSocket m_write = ENET_SOCKET_NULL;
	ENetAddress m_address{};
};
} // namespace con
