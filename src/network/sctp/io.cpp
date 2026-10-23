/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include <cstdint>
#include "network/sctp/internal.h"
#include "network/sctp/fm_sctp_address.h"

#if USE_SCTP

namespace con_sctp
{

// very ugly windows hack
#if 0 && defined(_WIN32)

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

#define MAX_PACKET_SIZE (1 << 16)
#define BUFFER_SIZE (1 << 16)
#define DISCARD_PPID 39

static void *handle_packets(void *arg)
{
	//DUMP("handle_packets thrd", MAX_PACKET_SIZE, arg);
	int *fdp;
	char *dump_buf;
	ssize_t length;
	char buf[MAX_PACKET_SIZE];

	fdp = (int *)arg;
	for (;;) {
		//DUMP("recv call", fdp);
		length = recv(*fdp, buf, MAX_PACKET_SIZE, 0);
		//DUMP("recv In", length, std::string(buf, length));
		//DUMP("recv In", length);
		if (length < 0) {
			return NULL;
		}

		if (length > 0) {
			if ((dump_buf = usrsctp_dumppacket(buf, (size_t)length, SCTP_DUMP_INBOUND)) !=
					NULL) {
				fprintf(stderr, "%s", dump_buf);
				//DUMP(dump_buf);
				usrsctp_freedumpbuffer(dump_buf);
			}
			//DUMP("go usrsctp_conninput", length);
			usrsctp_conninput(fdp, buf, (size_t)length, 0);
		}
	}
	return (NULL);
}

int Connection::receive()
{
	int n = 0;
	{
		const auto lock = m_peers.lock_unique_rec();
		for (const auto &i : m_peers) {
			bool removed = false;
			for (unsigned budget = 0; budget < 16; ++budget) {
				const auto [nn, brk] = recv_(i.first, i.second);
				n += nn;
				removed = brk;
				if (brk || nn <= 0)
					break;
			}
			if (removed)
				break;
		}
	}

	if (sock_listen && sock) {

		usrsctp_set_non_blocking(sock, 1);

		struct socket *conn_sock = nullptr;
		struct sockaddr_storage remote_addr = {};
		socklen_t addr_len = sizeof(remote_addr);
		if ((conn_sock = usrsctp_accept(
					 sock, (struct sockaddr *)&remote_addr, &addr_len)) == NULL) {
			if (errno == EWOULDBLOCK) {
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
		Address sender = fmSctpPeerAddress(
				reinterpret_cast<const sockaddr *>(&remote_addr), addr_len);
		m_peers_address.insert_or_assign(peer_id, sender);

		putEvent(ConnectionEvent::peerAdded(peer_id, sender));
		++n;
	}
	return n;
}

void Connection::handle_association_change_event(
		session_t peer_id, const struct sctp_assoc_change *sac)
{
	const bool was_ready = m_established_peers.count(peer_id) != 0;
	bool remove = false;
	bool setup_failed = false;
	const char *state = "UNKNOWN";
	switch (sac->sac_state) {
	case SCTP_COMM_UP:
		state = "SCTP_COMM_UP";
		// Streams 1..3 are reliable and 5..7 are unreliable.
		if (sac->sac_inbound_streams < 8 || sac->sac_outbound_streams < 8) {
			warningstream << "SCTP peer " << peer_id << " negotiated too few streams: in="
						  << sac->sac_inbound_streams
						  << " out=" << sac->sac_outbound_streams << std::endl;
			remove = setup_failed = true;
		} else {
			m_established_peers.insert(peer_id);
			if (peer_id == PEER_ID_SERVER)
				m_association_ready.store(true, std::memory_order_release);
		}
		break;
	case SCTP_COMM_LOST:
		state = "SCTP_COMM_LOST";
		remove = true;
		setup_failed = !was_ready;
		break;
	case SCTP_RESTART:
		state = "SCTP_RESTART";
		// A transport restart does not restore the game's login/session state.
		// Require a fresh game connection instead of reusing queued messages.
		remove = true;
		setup_failed = !was_ready;
		break;
	case SCTP_SHUTDOWN_COMP:
		state = "SCTP_SHUTDOWN_COMP";
		remove = true;
		setup_failed = !was_ready;
		break;
	case SCTP_CANT_STR_ASSOC:
		state = "SCTP_CANT_STR_ASSOC";
		remove = setup_failed = true;
		break;
	default:
		break;
	}

	auto &out = remove && sac->sac_state != SCTP_SHUTDOWN_COMP ? warningstream : cs;
	out << "SCTP peer " << peer_id << ": " << state << " error=" << sac->sac_error
		<< " streams(in/out)=" << sac->sac_inbound_streams << "/"
		<< sac->sac_outbound_streams;
	const size_t extra =
			sac->sac_length >= sizeof(*sac) ? sac->sac_length - sizeof(*sac) : 0;
	if (sac->sac_state == SCTP_COMM_UP || sac->sac_state == SCTP_RESTART) {
		out << " supports:";
		for (size_t i = 0; i < extra; ++i) {
			switch (sac->sac_info[i]) {
			case SCTP_ASSOC_SUPPORTS_PR:
				out << " PR";
				break;
			case SCTP_ASSOC_SUPPORTS_AUTH:
				out << " AUTH";
				break;
			case SCTP_ASSOC_SUPPORTS_ASCONF:
				out << " ASCONF";
				break;
			case SCTP_ASSOC_SUPPORTS_MULTIBUF:
				out << " MULTIBUF";
				break;
			case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
				out << " RE-CONFIG";
				break;
			case SCTP_ASSOC_SUPPORTS_INTERLEAVING:
				out << " I-DATA";
				break;
			default:
				out << " UNKNOWN(" << unsigned(sac->sac_info[i]) << ")";
				break;
			}
		}
	} else if (extra) {
		out << " abort data:";
		for (size_t i = 0; i < extra; ++i)
			out << " " << unsigned(sac->sac_info[i]);
	}
	out << std::endl;

	// Deliver every association event to derived transports before cleanup.
	onAssociationChange(peer_id, sac);
	if (remove) {
		deletePeer(peer_id, false);
		if (setup_failed && peer_id == PEER_ID_SERVER)
			putEvent(ConnectionEvent::connectFailed());
	}
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

std::pair<int, bool> Connection::recv_(session_t peer_id, struct socket *sock)
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
	alignas(sctp_notification) char buffer[BUFFER_SIZE];
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
			if (n < static_cast<decltype(n)>(sizeof(notification.sn_header)) ||
					notification.sn_header.sn_length != static_cast<size_t>(n) ||
					!(flags & MSG_EOR)) {
				deletePeer(peer_id, false);
				return {0, true};
			}
			size_t required = sizeof(notification.sn_header);
			switch (notification.sn_header.sn_type) {
			case SCTP_ASSOC_CHANGE:
				required = sizeof(sctp_assoc_change);
				break;
			case SCTP_PARTIAL_DELIVERY_EVENT:
				required = sizeof(sctp_pdapi_event);
				break;
			case SCTP_PEER_ADDR_CHANGE:
				required = sizeof(sctp_paddr_change);
				break;
			default:
				break;
			}
			if (static_cast<size_t>(n) < required) {
				deletePeer(peer_id, false);
				return {0, true};
			}

			switch (notification.sn_header.sn_type) {
			case SCTP_ASSOC_CHANGE: {
				if (notification.sn_assoc_change.sac_state == SCTP_COMM_UP) {
					const auto peer_address = fmSctpPeerAddress(
							reinterpret_cast<const sockaddr *>(&addr), from_len);
					// Keep the scoped endpoint saved by connect/accept.
					// AF_CONN carries an opaque pointer, not an IP address.
					if (peer_address.isValid() && !m_peers_address.count(peer_id))
						m_peers_address.insert_or_assign(peer_id, peer_address);
				}
				handle_association_change_event(peer_id, &notification.sn_assoc_change);
				if (!getPeer(peer_id))
					return {static_cast<int>(n), true};
				break;
			}
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
				if (notification.sn_pdapi_event.pdapi_indication ==
						SCTP_PARTIAL_DELIVERY_ABORTED) {
					auto &streams = recv_buf[peer_id];
					const auto stream = notification.sn_pdapi_event.pdapi_stream;
					if (stream < streams.size())
						streams[stream].clear();
				}
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
			}

			const size_t stream_id =
					infotype == SCTP_RECVV_RCVINFO ? rcv_info.rcv_sid : 0;
			auto &peer_recv_buf = recv_buf[peer_id];
			if (stream_id >= peer_recv_buf.size()) {
				cs << "Dropping SCTP data for peer " << peer_id << " on invalid stream "
				   << stream_id << std::endl;
				return {0, false};
			}

			auto &stream_recv_buf = peer_recv_buf[stream_id];
			stream_recv_buf.append(buffer, static_cast<size_t>(n));
			if (flags & MSG_EOR) {
				SharedBuffer<u8> resultdata((const unsigned char *)stream_recv_buf.data(),
						stream_recv_buf.size());
				putEvent(ConnectionEvent::dataReceived(peer_id, resultdata));
				stream_recv_buf.clear();
			}
		}
	} else if (n == 0) {
		deletePeer(peer_id, false);
		return {0, true};
	} else if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK ||
			   errno == EINTR) {
		// nothing
	} else {

		// drop peer here
		cs << "receive() ... drop on" << __LINE__ << " peer_id=" << peer_id
		   << " sock=" << (uintptr_t)sock << " errno=" << errno
		   << " EINPROGRESS=" << EINPROGRESS << " n=" << n << std::endl;
		// if (m_peers.count(peer_id)) { //ugly fix. todo: fix enet and remove
		deletePeer(peer_id, false);
		//}
		// break;
		return {0, true};
	}

	return {static_cast<int>(std::max<ssize_t>(n, 0)), false};
}

static uint16_t event_types[] = {SCTP_ASSOC_CHANGE, SCTP_PEER_ADDR_CHANGE,
		SCTP_REMOTE_ERROR, SCTP_SHUTDOWN_EVENT, SCTP_PARTIAL_DELIVERY_EVENT,
		SCTP_ADAPTATION_INDICATION, SCTP_SEND_FAILED_EVENT, SCTP_STREAM_RESET_EVENT,
		SCTP_STREAM_CHANGE_EVENT};

void Connection::sock_setup(/*session_t peer_id,*/ struct socket *sock)
{

	usrsctp_set_non_blocking(sock, 1);
	fmConfigureSctpInterleaving(sock);

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
	if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_RECVRCVINFO, &one, sizeof(one)) < 0)
		throw ConnectionException("Cannot enable SCTP receive stream information");
	if (domain != AF_CONN) {
		if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR, &one,
					sizeof(one)) < 0) {
			perror("usrsctp_setsockopt SCTP_I_WANT_MAPPED_V4_ADDR");
		}
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

			if (bind_address.isAny())
				addr.sin6_addr = in6addr_any;
			else if (bind_address.isLocalhost())
				addr.sin6_addr = in6addr_loopback;
			else
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
		if (usrsctp_bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in6)) <
				0) {
			perror("usrsctp_bind1");
		}
	} else if (domain == AF_CONN) {

		struct sockaddr_conn sconn
		{
		};

		sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
		sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif

		sconn.sconn_port = htons(bind_address.getPort());
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
	if (m_peers.count(PEER_ID_SERVER)) {
		putEvent(ConnectionEvent::connectFailed());
		return;
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
	m_peers_address.insert_or_assign(PEER_ID_SERVER, address);

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
	const int connect_result =
			usrsctp_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (connect_result < 0 && errno != EINPROGRESS) {
		perror("usrsctp_connect fail");
		putEvent(ConnectionEvent::connectFailed());
		disconnect();
		return;
	}

	cs << "connect() ok sock=" << sock << std::endl;

	sock_connect = true;
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
	// struct socket *s;
	// struct sockaddr_in sin;

	// if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (conn_fd < 0) {
		perror("socket");
		putEvent(ConnectionEvent::connectFailed());
		//DUMP("no sock", conn_fd);
		return;
	}
	//DUMP(conn_fd);
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

	//DUMP(sina);
	if (connect(conn_fd, (struct sockaddr *)&sina, sizeof(sina)) < 0) {
		perror("connect");
		putEvent(ConnectionEvent::connectFailed());
		//DUMP(conn_fd);
		close(conn_fd);
		conn_fd = -1;
		return;
	}
	// std::string t {"TESTFIRST"};
	// DUMP(write(fd, t.c_str(), t.size()));

	usrsctp_sysctl_set_sctp_ecn_enable(0);
	usrsctp_register_address((void *)&conn_fd);
	int rc = 0;
	//DUMP(conn_fd);
	//DUMP("creating handle_packets thrd");
	/*
	if ((rc = pthread_create(&tid, NULL, &handle_packets, (void *)&fd)) != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(rc));
		exit(EXIT_FAILURE);
	}
*/
	//std::thread t1(handle_packets, (void *)&fd);
	handle_packets_thread = std::thread{handle_packets, (void *)&conn_fd};

	//DUMP("created handle_packets thrd", rc);

	if ((sock = usrsctp_socket(
				 AF_CONN, SOCK_STREAM, IPPROTO_SCTP, NULL, NULL, 0, &conn_fd)) == NULL) {
		perror("usrsctp_socket");
		putEvent(ConnectionEvent::connectFailed());
		//DUMP((long)sock);
		disconnect();
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
	sock_setup(/*PEER_ID_SERVER,*/ sock);

	m_peers.insert_or_assign(PEER_ID_SERVER, sock);
	m_peers_address.insert_or_assign(PEER_ID_SERVER, address);

	struct sockaddr_conn sconn = {};
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(0);
	sconn.sconn_addr = NULL;
	if (usrsctp_bind(sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0) {
		perror("usrsctp_bind");
		putEvent(ConnectionEvent::bindFailed());
		disconnect();
		return;
	}
	usrsctp_set_non_blocking(sock, 1);

	sconn = {};
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(address.getPort());
	sconn.sconn_addr = &conn_fd;
	if (usrsctp_connect(sock, (struct sockaddr *)&sconn, sizeof(sconn)) < 0 &&
			errno != EINPROGRESS && errno != EWOULDBLOCK) {
		perror("usrsctp_connect");
		putEvent(ConnectionEvent::connectFailed());
		disconnect();
		return;
	}

	cs << "connect() ok sock=" << sock << std::endl;

	sock_connect = true;
}

void Connection::disconnect()
{
	struct socket *primary_sock = sock;
	sock = nullptr;
	sock_connect = false;
	sock_listen = false;
	{
		const auto lock = m_peers.lock_unique_rec();

		for (auto i = m_peers.begin(); i != m_peers.end(); ++i) {
			if (i->second && i->second != primary_sock)
				usrsctp_close(i->second);
		}
		m_peers.clear();
	}
	if (primary_sock)
		usrsctp_close(primary_sock);
	m_peers_address.clear();
	recv_buf.clear();
	pending_sends.clear();
	m_established_peers.clear();
	m_association_ready.store(false, std::memory_order_release);

	if (conn_fd >= 0) {
		usrsctp_deregister_address(&conn_fd);
		shutdown(conn_fd, SHUT_RDWR);
		close(conn_fd);
		conn_fd = -1;
	}
	if (handle_packets_thread.joinable())
		handle_packets_thread.join();
}

} // namespace

#endif
