/*
Copyright (C) 2023 proller <proler@gmail.com>
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

/*

Very experimental sctp networking

to build:
add submodules:
git submodule add https://github.com/sctplab/usrsctp.git src/external/usrsctp
git submodule add https://github.com/proller/android-ifaddrs.git
build/android/jni/android-ifaddrs

and make with:
cmake . -DENABLE_SCTP=1

*/

/*
https://chromium.googlesource.com/external/webrtc/+/master/talk/media/sctp/sctpdataengine.cc
*/

#include <cerrno>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include "config.h"

#if USE_SCTP

#include "log.h"
#include "network/fm_connection_sctp.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "porting.h"
#include "profiler.h"
#include "serialization.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"
#include <cstdarg>

#include "external/usrsctp/usrsctplib/usrsctp.h"

#ifdef __EMSCRIPTEN__
#include <emsocket.h>
#endif

namespace con_sctp
{

#define BUFFER_SIZE (1 << 16)

// very ugly windows hack
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

int inet_pton(int af, const char *src, void *dst)
{
	struct sockaddr_storage ss;
	int size = sizeof(ss);
	char src_copy[INET6_ADDRSTRLEN + 1];

	ZeroMemory(&ss, sizeof(ss));
	/* stupid non-const API */
	strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
	src_copy[INET6_ADDRSTRLEN] = 0;

	if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
		switch (af) {
		case AF_INET:
			*(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
			return 1;
		case AF_INET6:
			*(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
			return 1;
		}
	}
	return 0;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	struct sockaddr_storage ss;
	unsigned long s = size;

	ZeroMemory(&ss, sizeof(ss));
	ss.ss_family = af;

	switch (af) {
	case AF_INET:
		((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
		break;
	default:
		return NULL;
	}
	/* cannot direclty use &size because of strict aliasing rules */
	return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0)
				   ? dst
				   : NULL;
}
#endif

/*
	Connection
*/
void debug_printf(const char *format, ...)
{
	printf("SCTP_DEBUG: ");
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

// auto & cs = verbosestream; //errorstream; // remove after debug
#if SCTP_DEBUG
auto &cs = errorstream; // remove after debug
#else
auto &cs = verbosestream; // remove after debug
#endif

static int conn_output(void *addr, void *buf, size_t length, uint8_t tos, uint8_t set_df)
{
	DUMP((long)addr, (long)buf, length, tos, set_df);

	char *dump_buf;
	int *fdp;

	fdp = (int *)addr;

	if ((dump_buf = usrsctp_dumppacket(buf, length, SCTP_DUMP_OUTBOUND)) != NULL) {
		fprintf(stderr, "%s", dump_buf);
		DUMP("Out", dump_buf);
		usrsctp_freedumpbuffer(dump_buf);
	}
	if (send(*fdp, buf, length, 0) < 0) {
		return (errno);
	} else {
		return (0);
	}
}

#define MAX_PACKET_SIZE (1 << 16)
#define BUFFER_SIZE 80
#define DISCARD_PPID 39

static void *handle_packets(void *arg)
{
DUMP("handle_packets thrd", MAX_PACKET_SIZE, arg);
	int *fdp;
	char *dump_buf;
	ssize_t length;
	char buf[MAX_PACKET_SIZE];

	fdp = (int *)arg;
	for (;;) {
		DUMP("recv call", fdp);
		length = recv(*fdp, buf, MAX_PACKET_SIZE, 0);
		//DUMP("recv In", length, std::string(buf, length));
		DUMP("recv In", length );
		if (length < 0) { return NULL; }

		if (length > 0) {
			if ((dump_buf = usrsctp_dumppacket(buf, (size_t)length, SCTP_DUMP_INBOUND)) !=
					NULL) {
				fprintf(stderr, "%s", dump_buf);
				//DUMP(dump_buf);
				usrsctp_freedumpbuffer(dump_buf);
			}
		DUMP("go usrsctp_conninput", length );
			usrsctp_conninput(fdp, buf, (size_t)length, 0);
		}
	}
	return (NULL);
}

Connection::Connection(u32 max_packet_size, float timeout, bool ipv6,
		PeerHandler *peerhandler) :
		thread_vector("Connection"),
		//m_protocol_id(protocol_id),
		m_max_packet_size(max_packet_size),
		m_timeout(timeout), sock(nullptr), m_peer_id(0), m_bc_peerhandler(peerhandler),
		m_last_recieved(0), m_last_recieved_warn(0)

#ifdef __EMSCRIPTEN__
		, domain{AF_CONN}
#endif
{

	if (domain == AF_CONN) {
		sctp_conn_output = conn_output;
		// domain = AF_CONN;
	}
	// #endif

	start();
}

bool Connection::sctp_inited = false;

Connection::~Connection()
{

	join();
	deletePeer(0);

	disconnect();

	if (sctp_inited_by_me) {

		for (int i = 0; i < 100; ++i) {
			if (!usrsctp_finish())
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			// cs << "Connection::~Connection() wait " << i << std::endl;
		}

		sctp_inited = false;
	}
}

/* Internal stuff */

void *Connection::run()
{
	while (!stopRequested()) {
		while (!m_command_queue.empty()) {
			auto c = m_command_queue.pop_frontNoEx();
			processCommand(c);
		}
		if (receive() <= 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	disconnect();

	return nullptr;
}

/* Internal stuff */

void Connection::putEvent(ConnectionEventPtr e)
{
	assert(e->type != CONNEVENT_NONE); // Pre-condition
	m_event_queue.push_back(e);
}

void Connection::processCommand(ConnectionCommandPtr c)
{
	switch (c->type) {
	case CONNCMD_NONE:
		dout_con << getDesc() << " processing CONNCMD_NONE" << std::endl;
		return;
	case CONNCMD_SERVE:
		dout_con << getDesc() << " processing CONNCMD_SERVE port=" << c->address.getPort()
				 << std::endl;
		serve(c->address);
		return;
	case CONNCMD_CONNECT:
		dout_con << getDesc() << " processing CONNCMD_CONNECT" << std::endl;
		connect_addr(c->address);
		return;
	case CONNCMD_DISCONNECT:
		dout_con << getDesc() << " processing CONNCMD_DISCONNECT" << std::endl;
		disconnect();
		return;
	case CONNCMD_DISCONNECT_PEER:
		dout_con << getDesc() << " processing CONNCMD_DISCONNECT" << std::endl;
		deletePeer(c->peer_id, false); // its correct ?
		// DisconnectPeer(c.peer_id);
		return;
	case CONNCMD_SEND:
		dout_con << getDesc() << " processing CONNCMD_SEND" << std::endl;
		send(c->peer_id, c->channelnum, c->data, c->reliable);
		return;
	case CONNCMD_SEND_TO_ALL:
		dout_con << getDesc() << " processing CONNCMD_SEND_TO_ALL" << std::endl;
		sendToAll(c->channelnum, c->data, c->reliable);
		return;
		/*	case CONNCMD_DELETE_PEER:
				dout_con << getDesc() << " processing CONNCMD_DELETE_PEER" << std::endl;
				deletePeer(c.peer_id, false);
				return;
		*/
	case CONCMD_ACK:
	case CONCMD_CREATE_PEER:
		break;
	}
}

void Connection::sctp_setup(u16 port)
{
	if (sctp_inited)
		return;
	sctp_inited = true;
	sctp_inited_by_me = true;

	auto debug_func = debug_printf;
	debug_func = nullptr;
#if SCTP_DEBUG
	debug_func = debug_printf;
#endif

	cs << "sctp_setup(" << port << ")" << std::endl;
	DUMP("sctp_conn_output", (long)sctp_conn_output);
	usrsctp_init(port, sctp_conn_output, debug_func);
	// usrsctp_init_nothreads(port, nullptr, debug_func);

#if SCTP_DEBUG
	// usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_NONE);
	usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
	usrsctp_sysctl_set_sctp_logging_level(0xffffffff);
#endif

#ifdef __EMSCRIPTEN__
	usrsctp_sysctl_set_sctp_auto_asconf(0);
	// usrsctp_sysctl_set_sctp_multiple_asconfs(0);
	usrsctp_sysctl_set_sctp_ecn_enable(0);
	usrsctp_sysctl_set_sctp_asconf_enable(0);
#endif

	// #if __ANDROID__
#ifndef __EMSCRIPTEN__
	usrsctp_sysctl_set_sctp_mobility_fasthandoff(1);
	usrsctp_sysctl_set_sctp_mobility_base(1);

	usrsctp_sysctl_set_sctp_inits_include_nat_friendly(1);
#endif

	usrsctp_sysctl_set_sctp_cmt_on_off(1); // SCTP_CMT_MAX
	usrsctp_sysctl_set_sctp_cmt_use_dac(1);
	usrsctp_sysctl_set_sctp_buffer_splitting(1);

	// usrsctp_sysctl_set_sctp_max_retran_chunk(5); // def 30
	usrsctp_sysctl_set_sctp_shutdown_guard_time_default(20); // def 180
	usrsctp_sysctl_set_sctp_heartbeat_interval_default(10);
	// usrsctp_sysctl_set_sctp_init_rtx_max_default(5); //def 8
	// usrsctp_sysctl_set_sctp_assoc_rtx_max_default(5); //def 10
	// usrsctp_sysctl_set_sctp_max_retran_chunk(5); //30

	// #if !defined(SCTP_WITH_NO_CSUM)
	// usrsctp_sysctl_set_sctp_no_csum_on_loopback(1);
	// #endif
}

// Receive packets from the network and buffers and create ConnectionEvents
int Connection::receive()
{
	int n = 0;
	{
		const auto lock = m_peers.lock_unique_rec();
		for (const auto &i : m_peers) {
			const auto [nn, brk] = recv_(i.first, i.second);
			n += nn;
			if (brk)
				break;
		}
	}

	if (sock_connect && sock) {
		const auto [nn, brk] = recv(PEER_ID_SERVER, sock);
		n += nn;
	}

	if (sock_listen && sock) {

		usrsctp_set_non_blocking(sock, 1);

		struct socket *conn_sock = nullptr;
		struct sockaddr_in6 remote_addr;
		socklen_t addr_len = sizeof(remote_addr);
		if ((conn_sock = usrsctp_accept(
					 sock, (struct sockaddr *)&remote_addr, &addr_len)) == NULL) {
			if (errno == EWOULDBLOCK) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				return n;
			} else {
				cs << "usrsctp_accept failed.  exiting...\n";
				return n;
			}
		}

		u16 peer_id = m_next_remote_peer_id;
		if (m_peers.size() > 0) {
			for (int i = 0; i < 1000; ++i) {
				if (peer_id > PEER_SCTP_MAX)
					peer_id = PEER_SCTP_MIN;
				++peer_id;
				if (!m_peers.count(peer_id)) {
					break;
				}
			}
		}
		m_next_remote_peer_id = peer_id + 1;
		if (m_next_remote_peer_id > PEER_SCTP_MAX)
			m_next_remote_peer_id = PEER_SCTP_MIN;

		cs << "receive() accepted " << conn_sock << " addr_len=" << addr_len
		   << " id=" << peer_id << std::endl;

		m_peers.insert_or_assign(peer_id, conn_sock);
		Address sender(remote_addr.sin6_addr, remote_addr.sin6_port);
		m_peers_address.insert_or_assign(peer_id, sender);

		putEvent(ConnectionEvent::peerAdded(peer_id, sender));
		++n;
	}
	return n;
}

// static
void Connection::handle_association_change_event(
		u16 peer_id, const struct sctp_assoc_change *sac)
{
	unsigned int i, n;

	cs << ("Association change ");
	switch (sac->sac_state) {
	case SCTP_COMM_UP:
		cs << ("SCTP_COMM_UP");
		break;
	case SCTP_COMM_LOST:
		cs << ("SCTP_COMM_LOST");
		// deletePeer(peer_id,  false);
		break;
	case SCTP_RESTART:
		cs << ("SCTP_RESTART");
		break;
	case SCTP_SHUTDOWN_COMP:
		cs << ("SCTP_SHUTDOWN_COMP");
		deletePeer(peer_id, false);
		break;
	case SCTP_CANT_STR_ASSOC:
		cs << ("SCTP_CANT_STR_ASSOC");
		deletePeer(peer_id, false);
		break;
	default:
		cs << ("UNKNOWN");
		break;
	}
	// cs << ", streams (in/out) = (" << sac->sac_inbound_streams << "/" <<
	// sac->sac_outbound_streams << ")";
	n = sac->sac_length - sizeof(struct sctp_assoc_change);
	if (((sac->sac_state == SCTP_COMM_UP) || (sac->sac_state == SCTP_RESTART)) &&
			(n > 0)) {
		cs << (", supports");
		for (i = 0; i < n; i++) {
			switch (sac->sac_info[i]) {
			case SCTP_ASSOC_SUPPORTS_PR:
				cs << (" PR");
				break;
			case SCTP_ASSOC_SUPPORTS_AUTH:
				cs << (" AUTH");
				break;
			case SCTP_ASSOC_SUPPORTS_ASCONF:
				cs << (" ASCONF");
				break;
			case SCTP_ASSOC_SUPPORTS_MULTIBUF:
				cs << (" MULTIBUF");
				break;
			case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
				cs << (" RE-CONFIG");
				break;
			default:
				cs << " UNKNOWN(" << sac->sac_info[i] << ")";
				break;
			}
		}
	} else if (((sac->sac_state == SCTP_COMM_LOST) ||
					   (sac->sac_state == SCTP_CANT_STR_ASSOC)) &&
			   (n > 0)) {
		cs << (", ABORT =");
		for (i = 0; i < n; i++) {
			cs << " " << sac->sac_info[i];
		}
	}
	cs << (".\n");
	if ((sac->sac_state == SCTP_CANT_STR_ASSOC) ||
			(sac->sac_state == SCTP_SHUTDOWN_COMP) ||
			(sac->sac_state == SCTP_COMM_LOST)) {
	}
	return;
}

static void handle_peer_address_change_event(const struct sctp_paddr_change *spc)
{
	char addr_buf[INET6_ADDRSTRLEN];
	const char *addr;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;

	switch (spc->spc_aaddr.ss_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)&spc->spc_aaddr;
		addr = inet_ntop(AF_INET, &sin->sin_addr, addr_buf, INET_ADDRSTRLEN);
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)&spc->spc_aaddr;
		addr = inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
		break;
	default:
#ifdef _WIN32
		_snprintf(addr_buf, INET6_ADDRSTRLEN, "Unknown family %d",
				spc->spc_aaddr.ss_family);
#else
		snprintf(addr_buf, INET6_ADDRSTRLEN, "Unknown family %d",
				spc->spc_aaddr.ss_family);
#endif
		addr = addr_buf;
		break;
	}
	cs << "Peer address " << addr << " is now ";
	switch (spc->spc_state) {
	case SCTP_ADDR_AVAILABLE:
		cs << ("SCTP_ADDR_AVAILABLE");
		break;
	case SCTP_ADDR_UNREACHABLE:
		cs << ("SCTP_ADDR_UNREACHABLE");
		break;
	case SCTP_ADDR_REMOVED:
		cs << ("SCTP_ADDR_REMOVED");
		break;
	case SCTP_ADDR_ADDED:
		cs << ("SCTP_ADDR_ADDED");
		break;
	case SCTP_ADDR_MADE_PRIM:
		cs << ("SCTP_ADDR_MADE_PRIM");
		break;
	case SCTP_ADDR_CONFIRMED:
		cs << ("SCTP_ADDR_CONFIRMED");
		break;
	default:
		cs << ("UNKNOWN");
		break;
	}
	char buf[100];
	sprintf(buf, " (error = 0x%08x).\n", spc->spc_error);
	cs << buf;
	return;
}

std::pair<int, bool> Connection::recv(session_t peer_id, struct socket *sock)
{

	if (!sock) {
		return {0, false};
	}

	usrsctp_set_non_blocking(sock, 1);

	struct sockaddr_in6 addr = {};
	socklen_t from_len = (socklen_t)sizeof(addr);
	int flags = 0;
	struct sctp_rcvinfo rcv_info = {};
	socklen_t infolen = (socklen_t)sizeof(rcv_info);
	unsigned int infotype = {};
	char buffer[BUFFER_SIZE]; // move to class
	auto n = usrsctp_recvv(sock, (void *)buffer, BUFFER_SIZE, (struct sockaddr *)&addr,
			&from_len, (void *)&rcv_info, &infolen, &infotype, &flags);
	if (n > 0) {
		// cs << "receive() ... " << __LINE__ << " n=" << n << "
		// rcv_sid="<<(int)rcv_info.rcv_sid<< " rcv_assoc_id="<<rcv_info.rcv_assoc_id<<
		// std::endl;
		if (flags & MSG_NOTIFICATION) {
			cs << "Notification of length " << n << " received.\n";

			const sctp_notification &notification =
					reinterpret_cast<const sctp_notification &>(buffer);
			if (notification.sn_header.sn_length != n) {
				cs << " wrong notification" << std::endl;
			}

			switch (notification.sn_header.sn_type) {
			case SCTP_ASSOC_CHANGE:
				// cs << "SCTP_ASSOC_CHANGE" << std::endl;
				// OnNotificationAssocChange(notification.sn_assoc_change);
				{
					switch (notification.sn_assoc_change.sac_state) {
					/*
						case SCTP_CANT_STR_ASSOC:
							cs<<("SCTP_CANT_STR_ASSOC");
							deletePeer(peer_id,  false);
							break;
					*/
					case SCTP_COMM_UP:
						m_peers_address.insert_or_assign(
								peer_id, Address(addr.sin6_addr, addr.sin6_port));
						/*							ConnectionEvent e;
													e.peerAdded(peer_id);
													putEvent(e);*/
						break;
					}
					handle_association_change_event(
							peer_id, &(notification.sn_assoc_change));
#if 0
					const sctp_assoc_change& change = notification.sn_assoc_change;
					switch (change.sac_state) {
					case SCTP_COMM_UP:
						cs << "Association change SCTP_COMM_UP" << std::endl;

						{
							m_peers_address.set(peer_id, Address(addr.sin6_addr, addr.sin6_port));
							ConnectionEvent e;
							e.peerAdded(peer_id);
							putEvent(e);
						}
						break;
					case SCTP_COMM_LOST: {
						cs << "Association change SCTP_COMM_LOST" << std::endl;
						deletePeer(peer_id,  false);
					}
					break;
					case SCTP_RESTART:
						cs << "Association change SCTP_RESTART" << std::endl;
						break;
					case SCTP_SHUTDOWN_COMP:
						cs << "Association change SCTP_SHUTDOWN_COMP" << std::endl;
						break;
					case SCTP_CANT_STR_ASSOC:
						cs << "Association change SCTP_CANT_STR_ASSOC" << std::endl;
						break;
					default:
						cs << "Association change UNKNOWN " << std::endl;
						break;
					}
#endif
				}

				break;
			case SCTP_PEER_ADDR_CHANGE: {
				const sctp_paddr_change *spc = &notification.sn_paddr_change;
				// printf("SCTP_PEER_ADDR_CHANGE: state=%d, error=%d\n",spc->spc_state,
				// spc->spc_error);
				handle_peer_address_change_event(spc);
				cs << "SCTP_PEER_ADDR_CHANGE state=" << spc->spc_state
				   << " error=" << spc->spc_error << std::endl;
				break;
			}
			case SCTP_REMOTE_ERROR:
				cs << "SCTP_REMOTE_ERROR" << std::endl;
				break;
			case SCTP_SHUTDOWN_EVENT:
				cs << "SCTP_SHUTDOWN_EVENT" << std::endl;
				deletePeer(peer_id, false);
				return {n, true};
				break;
			case SCTP_ADAPTATION_INDICATION:
				cs << "SCTP_ADAPTATION_INDICATION" << std::endl;
				break;
			case SCTP_PARTIAL_DELIVERY_EVENT:
				cs << "SCTP_PARTIAL_DELIVERY_EVENT" << std::endl;
				break;
			case SCTP_AUTHENTICATION_EVENT:
				cs << "SCTP_AUTHENTICATION_EVENT" << std::endl;
				break;
			case SCTP_SENDER_DRY_EVENT:
				cs << "SCTP_SENDER_DRY_EVENT" << std::endl;
				// SignalReadyToSend(true);
				break;
			// TODO(ldixon): Unblock after congestion.
			case SCTP_NOTIFICATIONS_STOPPED_EVENT:
				cs << "SCTP_NOTIFICATIONS_STOPPED_EVENT" << std::endl;
				break;
			case SCTP_SEND_FAILED_EVENT:
				cs << "SCTP_SEND_FAILED_EVENT" << std::endl;
				break;
			case SCTP_STREAM_RESET_EVENT:
				cs << "SCTP_STREAM_RESET_EVENT" << std::endl;
				// OnStreamResetEvent(&notification.sn_strreset_event);
				break;
			case SCTP_ASSOC_RESET_EVENT:
				cs << "SCTP_ASSOC_RESET_EVENT" << std::endl;
				break;
			case SCTP_STREAM_CHANGE_EVENT:
				cs << "SCTP_STREAM_CHANGE_EVENT" << std::endl;
				// An acknowledgment we get after our stream resets have gone through,
				// if they've failed.  We log the message, but don't react -- we don't
				// keep around the last-transmitted set of SSIDs we wanted to close for
				// error recovery.  It doesn't seem likely to occur, and if so, likely
				// harmless within the lifetime of a single SCTP association.
				break;
			default:
				cs << "Unknown SCTP event: " << notification.sn_header.sn_type
				   << std::endl;
				break;
			}

		} else {
			char name[INET6_ADDRSTRLEN];
			if (infotype == SCTP_RECVV_RCVINFO) {
				char buf[1000];
				sprintf(buf,
						"Msg of length %llu received from %s:%u on stream %d with SSN %u "
						"and TSN %u, PPID %d, context %u, complete %d.\n",
						(unsigned long long)n,
						inet_ntop(AF_INET6, &addr.sin6_addr, name, INET6_ADDRSTRLEN),
						ntohs(addr.sin6_port), rcv_info.rcv_sid, rcv_info.rcv_ssn,
						rcv_info.rcv_tsn, ntohl(rcv_info.rcv_ppid), rcv_info.rcv_context,
						(flags & MSG_EOR) ? 1 : 0);
				cs << buf;
			} else {
				/*
								printf("Msg of length %llu received from %s:%u, complete
				   %d.\n", (unsigned long long)n, inet_ntop(AF_INET6, &addr.sin6_addr,
				   name, INET6_ADDRSTRLEN), ntohs(addr.sin6_port), (flags & MSG_EOR) ? 1 :
				   0);
				*/
				recv_buf[peer_id][rcv_info.rcv_sid] +=
						std::string(buffer, n); // optimize here if firs packet complete`
				// cs <<  "recieved data n="<< n << " peer="<<peer_id<<"
				// sid="<<rcv_info.rcv_sid<< " flags="<<flags<<" complete="<<(flags &
				// MSG_EOR)<< " buf="<<recv_buf[peer_id].size()<<" from sock="<<sock<<"
				// hash="<<std::dec<<std::hash<std::string>()(std::string(buffer,
				// n))<<std::endl;
				if ((flags & MSG_EOR)) {
					// cs<<"recv: msg complete peer="<<peer_id<<"
					// sid="<<rcv_info.rcv_sid<<"
					// size="<<recv_buf[peer_id][rcv_info.rcv_sid].size()<<"
					// hash="<<std::dec<<std::hash<std::string>()(recv_buf[peer_id][rcv_info.rcv_sid])<<std::endl;
					// SharedBuffer<u8> resultdata((const unsigned char*)buffer, n);
					SharedBuffer<u8> resultdata(
							(const unsigned char *)recv_buf[peer_id][rcv_info.rcv_sid]
									.c_str(),
							recv_buf[peer_id][rcv_info.rcv_sid].size());
					putEvent(ConnectionEvent::dataReceived(peer_id, resultdata));
					// recv_buf[rcv_info.rcv_sid].erase(peer_id);
					recv_buf[peer_id][rcv_info.rcv_sid].clear();
				}
			}
		}
	} else if (n == 0 || errno == EINPROGRESS || errno == EAGAIN) {
		// nothing
	} else {

		// drop peer here
		cs << "receive() ... drop on" << __LINE__ << " peer_id=" << peer_id
		   << " sock=" << (long)sock << " errno=" << errno
		   << " EINPROGRESS=" << EINPROGRESS << " n=" << n << std::endl;
		// if (m_peers.count(peer_id)) { //ugly fix. todo: fix enet and remove
		deletePeer(peer_id, false);
		//}
		// break;
		return {0, true};
	}

	return {0, false};
}

static uint16_t event_types[] = {SCTP_ASSOC_CHANGE, SCTP_PEER_ADDR_CHANGE,
		SCTP_REMOTE_ERROR, SCTP_SHUTDOWN_EVENT, SCTP_ADAPTATION_INDICATION,
		SCTP_SEND_FAILED_EVENT, SCTP_STREAM_RESET_EVENT, SCTP_STREAM_CHANGE_EVENT};

void Connection::sock_setup(/*session_t peer_id,*/ struct socket *sock)
{

usrsctp_set_non_blocking(sock, 1);

	struct sctp_event event = {};
	event.se_assoc_id = SCTP_ALL_ASSOC;
	event.se_on = 1;
	for (unsigned int i = 0; i < sizeof(event_types) / sizeof(uint16_t); i++) {
		event.se_type = event_types[i];
		if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) <
				0) {
			perror("setsockopt SCTP_EVENT");
		}
	}

	const int one = 1;
	if (usrsctp_setsockopt(
				sock, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR, &one, sizeof(one)) < 0) {
		perror("usrsctp_setsockopt SCTP_I_WANT_MAPPED_V4_ADDR");
	}

	if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_EXPLICIT_EOR, &one, sizeof(one)) <
			0) {
		perror("setsockopt SCTP_EXPLICIT_EOR");
	}

	if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_NODELAY, &one, sizeof(one))) {
		// cs << " setsockopt error: SCTP_NODELAY" << peer_id << std::endl;
		// return NULL;
		perror("setsockopt SCTP_NODELAY");
	}

	/*
		if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_REUSE_PORT, &on, sizeof(int)) < 0)
	   { perror("setsockopt SCTP_REUSE_PORT");
		}
	*/
}

// host
void Connection::serve(const Address &bind_address)
{
	infostream << getDesc() << "SCTP serving at " << bind_address.serializeString() << ":"
			   << std::to_string(bind_address.getPort()) << std::endl;

	sctp_setup(bind_address.getPort());

	if ((sock = usrsctp_socket(domain, SOCK_STREAM, IPPROTO_SCTP, NULL, server_send_cb, 0,
				 NULL)) == NULL) {
		cs << ("usrsctp_socket is NULL") << std::endl;
		putEvent(ConnectionEvent::bindFailed());
		return;
	}

	sock_setup(/*0,*/ sock);

if (domain == AF_INET6 || domain == AF_INET) {
	struct sockaddr_in6 addr = {};

	if (!bind_address.isIPv6()) {
		cs << "connect() transform to v6 " << __LINE__ << std::endl;

		inet_pton(AF_INET6, ("::ffff:" + bind_address.serializeString()).c_str(),
				&addr.sin6_addr);
	} else {
		addr = bind_address.getAddress6();
	}

#ifdef HAVE_SIN6_LEN
	addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(bind_address.getPort()); // htons(13);
	cs << "Waiting for connections on sctp port " << ntohs(addr.sin6_port) << "\n";
	if (usrsctp_bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in6)) < 0) {
		perror("usrsctp_bind1");
	}
} else if(domain == AF_CONN) {

			struct sockaddr_conn sconn
			{
			};



			sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
			sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif

			 sconn.sconn_port =  htons(bind_address.getPort());
			 //ntohs(((struct sockaddr_conn *)firstaddr)->sconn_port)
			//sconn.sconn_addr = (void *)&fd[0];
			//sconn.sconn_addr = (void *)&fd[1];
			//sconn.sconn_addr = &sock;
			//sconn.sconn_addr =ulp_info.get();
			if (usrsctp_bind(sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
				perror("usrsctp_bind2");
			}
}
	if (usrsctp_listen(sock, 10) < 0) {
		perror("usrsctp_listen");
	}

	cs << "serve() ok " << sock << std::endl;

	sock_listen = true;
}

// peer
void Connection::connect_addr(const Address &address)
{
	infostream << getDesc() << "SCTP connect to " << address.serializeString() << ":"
			   << std::to_string(address.getPort()) << std::endl;

	sctp_setup(address.getPort() + myrand_range(100, 1000));

	m_last_recieved = porting::getTimeMs();
	auto node = m_peers.find(PEER_ID_SERVER);
	if (node != m_peers.end()) {
		// throw ConnectionException("Already connected to a server");
		putEvent(ConnectionEvent::connectFailed());
	}

	// void * ulp_info = nullptr;

	if (domain == AF_CONN) {
		return connect_conn(address);
	}
	//struct socket *sock = nullptr;

	/*
		if (domain == AF_CONN) {
			//if (!(sock = conn_socket(address))) {
	if (!(sock = usrsctp_socket(domain, SOCK_STREAM, IPPROTO_SCTP, NULL,
							 client_send_cb, 0, NULL))) {
				putEvent(ConnectionEvent::bindFailed());
				return;
			}

		} else
	*/
	if (!(sock = usrsctp_socket(
				  domain, SOCK_STREAM, IPPROTO_SCTP, NULL, client_send_cb, 0, nullptr))) {
		cs << ("usrsctp_socket=") << sock << std::endl;
		putEvent(ConnectionEvent::bindFailed());
		return;
	}

	sock_setup(/*PEER_ID_SERVER,*/ sock);

	m_peers.insert_or_assign(PEER_ID_SERVER, sock);

	cs << "connect() using encaps " << address.getPort() << std::endl;

	sctp_udpencaps encaps = {};
	encaps.sue_address.ss_family = AF_INET6;
	encaps.sue_port = htons(address.getPort());
	if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT,
				(const void *)&encaps, (socklen_t)sizeof(encaps)) < 0) {
		cs << ("connect setsockopt fail") << std::endl;
		putEvent(ConnectionEvent::connectFailed());
	}

	// #define SCTP_V4 1

#if SCTP_V4
	struct sockaddr_in addr = {};
#ifdef HAVE_SIN_LEN
	addr6.sin_len = sizeof(struct sockaddr_in);
#endif
	addr.sin_family = AF_INET6;
	addr.sin_addr.s_addr = INADDR_ANY;
#else
	struct sockaddr_in6 addr = {};
#ifdef HAVE_SIN6_LEN
	addr6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	addr.sin6_family = domain;
	addr.sin6_addr = in6addr_any;
	// addr.sin6_port = 65000;
// addr6.sin6_addr = in6addr_loopback;
#endif

	if (usrsctp_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("usrsctp_bind");
	}

	addr = {};

#if SCTP_V4
	addr = address.getAddress();
#else
	if (!address.isIPv6()) {
		cs << "connect() transform to v6 " << address.serializeString() << std::endl;
		if (address.serializeString() == "127.0.0.1")
			addr.sin6_addr = in6addr_loopback;
		else
			inet_pton(AF_INET6, ("::ffff:" + address.serializeString()).c_str(),
					&addr.sin6_addr);
	} else {
		addr = address.getAddress6();
	}
#endif

#if SCTP_V4
#ifdef HAVE_SIN6_LEN
	addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
	addr.sin_family = AF_INET;
	addr.sin_port = htons(address.getPort());
#else
#ifdef HAVE_SIN6_LEN
	addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
	addr.sin6_family = domain;
	addr.sin6_port = htons(address.getPort());
#endif

	usrsctp_set_non_blocking(sock, 1);

	cs << "connect() ... " << __LINE__
#if !SCTP_V4
	   << " scope=" << addr.sin6_scope_id
#endif
	   << std::endl;
	if (auto connect_result =
					usrsctp_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		if (connect_result < 0 && errno != EINPROGRESS) {
			perror("usrsctp_connect fail");
			sock = nullptr;
		}
	}

	cs << "connect() ok sock=" << sock << std::endl;

	sock_connect = true;
}

static int receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
		size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{

	if (data) {
		if (flags & MSG_NOTIFICATION) {
			printf("Notification of length %d received.\n", (int)datalen);
		} else {
			printf("Msg of length %d received via %p:%u on stream %u with SSN %u and TSN "
				   "%u, PPID %u, context %u.\n",
					(int)datalen, addr.sconn.sconn_addr, ntohs(addr.sconn.sconn_port),
					rcv.rcv_sid, rcv.rcv_ssn, rcv.rcv_tsn, (uint32_t)ntohl(rcv.rcv_ppid),
					rcv.rcv_context);
		}
		free(data);
	} else {
		usrsctp_deregister_address(ulp_info);
		usrsctp_close(sock);
	}
	return (1);
}

void Connection::connect_conn(const Address &address)
{
	// struct socket *sock = nullptr;

	/*
		if (domain == AF_CONN) {
			//if (!(sock = conn_socket(address))) {
	if (!(sock = usrsctp_socket(domain, SOCK_STREAM, IPPROTO_SCTP, NULL,
							 client_send_cb, 0, NULL))) {
				putEvent(ConnectionEvent::bindFailed());
				return;
			}

		} else
	*/
	int fd = 0;
	// struct socket *s;
	// struct sockaddr_in sin;

	// if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		putEvent(ConnectionEvent::connectFailed());
		DUMP("no sock", fd);
	}
	DUMP(fd);
	/*
		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
	#ifdef HAVE_SIN_LEN
		sin.sin_len = sizeof(struct sockaddr_in);
	#endif
		sin.sin_port = htons(atoi(argv[2]));
		if (!inet_pton(AF_INET, argv[1], &sin.sin_addr.s_addr)){
			fprintf(stderr, "error: invalid address\n");
			exit(EXIT_FAILURE);
		}
		if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
			perror("bind");
			exit(EXIT_FAILURE);
		}
		*/
	/*
		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
	#ifdef HAVE_SIN_LEN
		sin.sin_len = sizeof(struct sockaddr_in);
	#endif
		sin.sin_port = htons(address.getPort());
		if (!inet_pton(AF_INET, address., &sin.sin_addr.s_addr)){
			printf("error: invalid address\n");
			exit(EXIT_FAILURE);
		}
	*/
	auto sina = address.getAddress();
	sina.sin_family = AF_INET;
	// sina.sin_port =address.getPort();
	sina.sin_port = htons(address.getPort());

	DUMP(sina);
	if (connect(fd, (struct sockaddr *)&sina, sizeof(sina)) < 0) {
		perror("connect");
		putEvent(ConnectionEvent::connectFailed());
		DUMP(fd);
		return;
	}
	// std::string t {"TESTFIRST"};
	// DUMP(write(fd, t.c_str(), t.size()));

	usrsctp_sysctl_set_sctp_ecn_enable(0);
	usrsctp_register_address((void *)&fd);
	int rc = 0;
	DUMP(fd);
	pthread_t tid{};
DUMP("creating handle_packets thrd");
/*
	if ((rc = pthread_create(&tid, NULL, &handle_packets, (void *)&fd)) != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(rc));
		exit(EXIT_FAILURE);
	}
*/
//std::thread t1(handle_packets, (void *)&fd);
handle_packets_thread =  std::thread {handle_packets, (void *)&fd};

DUMP("created handle_packets thrd", rc);



	if ((sock = usrsctp_socket(
				 AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb, NULL, 0, &fd)) == NULL) {
		perror("usrsctp_socket");
		putEvent(ConnectionEvent::connectFailed());
		DUMP((long)sock);
		return;
	}

	/*
		if (!(sock = usrsctp_socket(domain, SOCK_STREAM, IPPROTO_SCTP, NULL,
							 client_send_cb, 0, nullptr))) {
			cs << ("usrsctp_socket=") << sock << std::endl;
			putEvent(ConnectionEvent::bindFailed());
			return;
		}
	*/
	// sock_setup(/*PEER_ID_SERVER,*/ sock);

	m_peers.insert_or_assign(PEER_ID_SERVER, sock);

	struct sockaddr_conn sconn = {};
	/*
		// memset(&sconn, 0, sizeof(struct sockaddr_conn));
		sconn.sconn_family = AF_CONN;
	#ifdef HAVE_SCONN_LEN
		sconn.sconn_len = sizeof(struct sockaddr_conn);
	#endif
		sconn.sconn_port = htons(0);
		sconn.sconn_addr = NULL;
		if (usrsctp_bind(sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
			perror("usrsctp_bind");
			putEvent(ConnectionEvent::bindFailed());
			return;
		}
	*/
	usrsctp_set_non_blocking(sock, 1);

	sconn = {};
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(address.getPort());
	sconn.sconn_addr = &fd;
	if (usrsctp_connect(sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
		perror("usrsctp_connect");
		putEvent(ConnectionEvent::connectFailed());
		return;
	}

	cs << "connect() ok sock=" << sock << std::endl;

	sock_connect = true;
}

void Connection::disconnect()
{
	if (sock)
		usrsctp_close(sock);
	sock = nullptr;
	{
		const auto lock = m_peers.lock_unique_rec();

		for (auto i = m_peers.begin(); i != m_peers.end(); ++i) {
			usrsctp_close(i->second);
		}
		m_peers.clear();
	}
	m_peers_address.clear();
}

void Connection::sendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	const auto lock = m_peers.lock_unique_rec();
	for (const auto &i : m_peers)
		send(i.first, channelnum, data, reliable);
}

void Connection::send(
		session_t peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	// cs<<" === sending to peer_id="<<peer_id <<" channelnum="<<(int)channelnum<< "
	// reliable="<<reliable<< " bytes="<<data.getSize()<<" hash=" <<std::dec<<
	// std::hash<std::string>()(std::string((const char*)*data,
	// data.getSize()))<<std::endl;
	{
		if (m_peers.find(peer_id) == m_peers.end()) {
			cs << " === send no peer " << peer_id << std::endl;
			return;
		}
	}
	dout_con << getDesc() << " sending to peer_id=" << peer_id << std::endl;

	if (channelnum >= CHANNEL_COUNT) {
		cs << " === send no chan " << channelnum << "/" << CHANNEL_COUNT << std::endl;
		return;
	}

	auto sock = getPeer(peer_id);
	if (!sock) {
		cs << " === send no peer sock" << std::endl;
		deletePeer(peer_id, false);
		return;
	}

	// cs<<" === send to peer " << peer_id<< "sock="<< peer<<std::endl;

	usrsctp_set_non_blocking(sock, 1);

	uint32_t flags = 0;

	struct sctp_sendv_spa spa = {};
	spa.sendv_sndinfo.snd_sid = channelnum + 1 + (!reliable) * 4;

	if (!reliable) {
		spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
		spa.sendv_prinfo.pr_value = 1; // units?
		spa.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
		flags = SCTP_UNORDERED;
		spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
	}

	// spa.sendv_sndinfo = sndinfo;
	spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;

	size_t maxlen = 0xffff - 1000;
	// size_t maxlen = 1400;
	size_t buflen = data.getSize();
	size_t sendlen = std::min(buflen, maxlen);
	size_t remlen = buflen;
	size_t curpos = 0;

	while (remlen > 0) {
		if (remlen <= maxlen) {
			spa.sendv_sndinfo.snd_flags |= SCTP_EOR;
		}

		// cs<<" psend" << " remlen=" << remlen << " curpos="<<curpos<< "
		// sendlen="<<sendlen << " buflen="<<buflen<< " nowsent="<<(curpos+sendlen)<<"
		// flags="<<spa.sendv_sndinfo.snd_flags<< " sid="<<spa.sendv_sndinfo.snd_sid
		//<<" hash=" <<std::dec<< std::hash<std::string>()(std::string((const char*)*data,
		// data.getSize()))
		//<<std::endl;

		// int len = usrsctp_sendv(sock, *data + curpos, sendlen, NULL, 0, (void
		// *)&spa.sendv_sndinfo, sizeof(spa.sendv_sndinfo), SCTP_SENDV_SNDINFO, 0);
		int len = usrsctp_sendv(sock, *data + curpos, sendlen, NULL, 0, (void *)&spa,
				sizeof(spa), SCTP_SENDV_SPA, flags);
		if (len > 0) {
			curpos += len;
			remlen -= len;
			sendlen = std::min(remlen, maxlen);
		}
		if (errno == EWOULDBLOCK) {
			cs << "send EWOULDBLOCK len=" << len << std::endl;
			usrsctp_set_non_blocking(sock, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

			continue;
		}
		if (errno == EAGAIN) {
			cs << "send EAGAIN len=" << len << std::endl;
			continue;
		}
		if (len < 0) {
			perror("usrsctp_sendv");
			deletePeer(peer_id, 0);

			cs << " === sending FAILED to peer_id=" << peer_id
			   << " bytes=" << data.getSize() << " sock=" << sock << " len=" << len
			   << " curpos=" << curpos << std::endl;
			break;
		}

		// if(len != buflen)
		// cs<<" part send" << " len="<<len<< " / "<<buflen<<std::endl;
	}
}

struct socket *Connection::getPeer(session_t peer_id)
{
	auto node = m_peers.find(peer_id);

	if (node == m_peers.end())
		return NULL;

	return node->second;
}

bool Connection::deletePeer(session_t peer_id, bool timeout)
{
	cs << "Connection::deletePeer " << peer_id << ", " << timeout << std::endl;
	Address peer_address;
	// any peer has a primary address this never fails!
	// peer->getAddress(MTP_PRIMARY, peer_address);

	if (!peer_id) {
		if (sock) {
			usrsctp_close(sock);
			sock = nullptr;

			putEvent(ConnectionEvent::peerRemoved(peer_id, timeout, {}));

			return true;
		} else {
			return false;
		}
	}
	if (m_peers.find(peer_id) == m_peers.end())
		return false;

	// Create event
	putEvent(ConnectionEvent::peerRemoved(peer_id, timeout, {}));

	{
		const auto lock = m_peers.lock_unique_rec();
		auto sock = m_peers.get(peer_id);
		if (sock)
			usrsctp_close(sock);

		// delete m_peers[peer_id]; -- enet should handle this
		m_peers.erase(peer_id);
	}
	m_peers_address.erase(peer_id);
	return true;
}

/* Interface */
/*
ConnectionEvent Connection::getEvent() {
	if(m_event_queue.empty()) {
		ConnectionEvent e;
		e.type = CONNEVENT_NONE;
		return e;
	}
	return m_event_queue.pop_frontNoEx();
}
*/

ConnectionEventPtr Connection::waitEvent(u32 timeout_ms)
{
	try {
		return m_event_queue.pop_front(timeout_ms);
	} catch (const ItemNotFoundException &ex) {
		return ConnectionEvent::create(CONNEVENT_NONE);
	}
}

void Connection::putCommand(ConnectionCommandPtr c)
{
	// TODO? if (!m_shutting_down)
	{
		m_command_queue.push_back(c);
		// m_sendThread->Trigger();
	}
}

void Connection::Serve(Address bind_addr)
{
	putCommand(ConnectionCommand::serve(bind_addr));
}

void Connection::Connect(Address address)
{
	putCommand(ConnectionCommand::connect(address));
}

bool Connection::Connected()
{
	auto node = m_peers.find(PEER_ID_SERVER);
	if (node == m_peers.end())
		return false;

	// TODO: why do we even need to know our peer id?
	if (!m_peer_id)
		m_peer_id = 2;

	if (m_peer_id == PEER_ID_INEXISTENT)
		return false;

	return true;
}

void Connection::Disconnect()
{
	putCommand(ConnectionCommand::disconnect());
}

u32 Connection::Receive(NetworkPacket *pkt, int timeout)
{
	for (;;) {
		auto e = waitEvent(timeout);
		if (e->type != CONNEVENT_NONE)
			dout_con << getDesc() << ": Receive: got event: " << e->describe()
					 << std::endl;
		switch (e->type) {
		case CONNEVENT_NONE:
			// throw NoIncomingDataException("No incoming data");
			return 0;
		case CONNEVENT_DATA_RECEIVED:
			if (e->data.getSize() < 2) {
				continue;
			}
			pkt->putRawPacket(*e->data, e->data.getSize(), e->peer_id);
			return e->data.getSize();
		case CONNEVENT_PEER_ADDED: {
			if (m_bc_peerhandler)
				m_bc_peerhandler->peerAdded(e->peer_id);
			continue;
		}
		case CONNEVENT_PEER_REMOVED: {
			if (m_bc_peerhandler)
				m_bc_peerhandler->deletingPeer(e->peer_id, e->timeout);
			continue;
		}
		case CONNEVENT_BIND_FAILED:
			throw ConnectionBindFailed("Failed to bind socket "
									   "(port already in use?)");
		case CONNEVENT_CONNECT_FAILED:
			throw ConnectionException("Failed to connect");
		}
	}
	return 0;
}

bool Connection::TryReceive(NetworkPacket *pkt)
{
	return Receive(pkt, 0);
}

/*
void Connection::SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable) {
	assert(channelnum < CHANNEL_COUNT);

	ConnectionCommand c;
	c.sendToAll(channelnum, data, reliable);
	putCommand(c);
}
*/

void Connection::Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	putCommand(ConnectionCommand::send(peer_id, channelnum, pkt, reliable));
}

void Connection::Send(
		session_t peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT); // Pre-condition

	putCommand(ConnectionCommand::send(peer_id, channelnum, data, reliable));
}

void Connection::Send(
		session_t peer_id, u8 channelnum, const msgpack::sbuffer &buffer, bool reliable)
{
	SharedBuffer<u8> data((unsigned char *)buffer.data(), buffer.size());
	Send(peer_id, channelnum, data, reliable);
}

Address Connection::GetPeerAddress(session_t peer_id)
{
	if (!m_peers_address.count(peer_id))
		return Address();
	return m_peers_address.get(peer_id);
}

/*
void Connection::DeletePeer(u16 peer_id) {
	ConnectionCommand c;
	c.deletePeer(peer_id);
	putCommand(c);
}
*/

void Connection::PrintInfo(std::ostream &out)
{
	out << getDesc() << ": ";
}

void Connection::PrintInfo()
{
	PrintInfo(dout_con);
}

std::string Connection::getDesc()
{
	return "";
	// return std::string("con(")+itos(m_socket.GetHandle())+"/"+itos(m_peer_id)+")";
}

float Connection::getPeerStat(session_t peer_id, rtt_stat_type type)
{
	return 0;
}

float Connection::getLocalStat(con::rate_stat_type type)
{
	return 0;
}

void Connection::DisconnectPeer(session_t peer_id)
{
	putCommand(ConnectionCommand::disconnect_peer(peer_id));
}

} // namespace

#endif