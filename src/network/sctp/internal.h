/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#pragma once

#include "config.h"

#if USE_SCTP

#include "external/usrsctp/usrsctplib/usrsctp.h"
#include "log.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/sctp/connection.h"
#include "network/address.h"
#include "porting.h"
#include "profiler.h"
#include "serialization.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <string_view>
#include <sstream>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

namespace con_sctp
{

#if SCTP_DEBUG
static auto &cs = errorstream; // remove after debug
#else
static auto &cs = verbosestream; // remove after debug
#endif

inline Address getPeerAddress(const sockaddr *address, size_t length)
{
	if (!address || length < sizeof(address->sa_family))
		return {};
	if (address->sa_family == AF_INET6 && length >= sizeof(sockaddr_in6)) {
		sockaddr_in6 ipv6;
		std::memcpy(&ipv6, address, sizeof(ipv6));
		return Address(ipv6); // Preserve the zone and convert the port to host order.
	}
	if (address->sa_family == AF_INET && length >= sizeof(sockaddr_in)) {
		sockaddr_in ipv4;
		std::memcpy(&ipv4, address, sizeof(ipv4));
		return Address(ipv4);
	}
	return {}; // AF_CONN is not an IP endpoint.
}

inline size_t getAddressSize(const sockaddr *address)
{
	switch (address->sa_family) {
	case AF_INET:
		return sizeof(sockaddr_in);
	case AF_INET6:
		return sizeof(sockaddr_in6);
	case AF_CONN:
		return sizeof(sockaddr_conn);
	default:
		return 0;
	}
}

inline std::string getEndpoint(const sockaddr *address)
{
	if (address->sa_family == AF_CONN)
		return "AF_CONN";
	const auto endpoint = getPeerAddress(address, getAddressSize(address));
	std::ostringstream out;
	endpoint.print(out);
	return out.str();
}

// Only called for association/path transitions, never for individual packets.
inline void logPaths(session_t peer_id, struct socket *sock, const char *reason)
{
	sctp_status status = {};
	socklen_t length = sizeof(status);
	if (usrsctp_getsockopt(sock, IPPROTO_SCTP, SCTP_STATUS, &status, &length) == 0)
		infostream << "SCTP peer " << peer_id << " paths reason=" << reason
				   << " assoc=" << status.sstat_assoc_id
				   << " state=" << status.sstat_state
				   << " unacked=" << status.sstat_unackdata << " primary="
				   << getEndpoint(reinterpret_cast<const sockaddr *>(
							  &status.sstat_primary.spinfo_address))
				   << std::endl;
	for (bool remote : {false, true}) {
		sockaddr *addresses = nullptr;
		const int count = remote ? usrsctp_getpaddrs(sock, 0, &addresses)
								 : usrsctp_getladdrs(sock, 0, &addresses);
		const int error = count < 0 ? errno : 0;
		infostream << "SCTP peer " << peer_id
				   << (remote ? " remote-known" : " local-bound") << " count=" << count
				   << " errno=" << error << std::endl;
		const sockaddr *address = addresses;
		for (int i = 0; address && i < count; ++i) {
			const size_t size = getAddressSize(address);
			if (!size)
				break;
			std::ostringstream line;
			line << "SCTP peer " << peer_id
				 << (remote ? " remote-known=" : " local-bound=") << getEndpoint(address);
			if (remote && address->sa_family != AF_CONN) {
				sctp_udpencaps encaps = {};
				std::memcpy(&encaps.sue_address, address, size);
				length = sizeof(encaps);
				if (usrsctp_getsockopt(sock, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT,
							&encaps, &length) == 0)
					line << " udp_port=" << ntohs(encaps.sue_port);
				else
					line << " udp_port_unavailable_errno=" << errno;
				sctp_paddrinfo path = {};
				std::memcpy(&path.spinfo_address, address, size);
				length = sizeof(path);
				if (usrsctp_getsockopt(sock, IPPROTO_SCTP, SCTP_GET_PEER_ADDR_INFO, &path,
							&length) == 0)
					line << " state=" << path.spinfo_state
						 << " srtt_ms=" << path.spinfo_srtt
						 << " rto_ms=" << path.spinfo_rto << " mtu=" << path.spinfo_mtu;
			}
			infostream << line.str() << std::endl;
			address = reinterpret_cast<const sockaddr *>(
					reinterpret_cast<const char *>(address) + size);
		}
		if (addresses) {
			if (remote)
				usrsctp_freepaddrs(addresses);
			else
				usrsctp_freeladdrs(addresses);
		}
	}
}

// A route can disappear during handoff while the association is still valid.
// Let SCTP's association timers decide when the peer has actually been lost.
inline bool isRetryableError(int error)
{
	return error == EWOULDBLOCK || error == EAGAIN || error == EINTR ||
		   error == ENOBUFS || error == ENOMEM || error == ENETDOWN ||
		   error == ENETUNREACH || error == EHOSTUNREACH || error == EADDRNOTAVAIL
#ifdef EHOSTDOWN
		   || error == EHOSTDOWN
#endif
			;
}

inline void configureDefaults()
{
	const auto configure = [](const char *name, int result) {
		if (result != 0)
			warningstream << "SCTP: cannot configure " << name << std::endl;
	};

#ifndef __EMSCRIPTEN__
	// Dynamic address reconfiguration requires AUTH on both endpoints.
	configure("AUTH", usrsctp_sysctl_set_sctp_auth_enable(1));
	configure("ASCONF", usrsctp_sysctl_set_sctp_asconf_enable(1));
	configure("automatic ASCONF", usrsctp_sysctl_set_sctp_auto_asconf(1));
	configure("mobility", usrsctp_sysctl_set_sctp_mobility_base(1));
	configure("fast handoff", usrsctp_sysctl_set_sctp_mobility_fasthandoff(1));
	configure("NAT friendly INIT", usrsctp_sysctl_set_sctp_inits_include_nat_friendly(1));
	// After NAT rebinding, data can arrive before the authenticated ASCONF that
	// identifies its new source. Do not kill the sender with a reflected OOTB
	// ABORT while that update is in flight. Unknown packets are still dropped;
	// AUTH validation and ABORT processing for known associations are unchanged.
	configure("out-of-the-blue packet policy", usrsctp_sysctl_set_sctp_blackhole(2));
#endif

	// CMT sends concurrently over paths; normal failover and ASCONF remain
	// enabled independently. Buffer splitting is a CMT throughput policy.
	configure("CMT", usrsctp_sysctl_set_sctp_cmt_on_off(0));
	configure("CMT delayed acknowledgments", usrsctp_sysctl_set_sctp_cmt_use_dac(0));
	configure("CMT buffer splitting", usrsctp_sysctl_set_sctp_buffer_splitting(0));

	// Heartbeats are milliseconds; the shutdown guard is seconds.
	configure("heartbeat interval",
			usrsctp_sysctl_set_sctp_heartbeat_interval_default(10000));
	configure("shutdown guard", usrsctp_sysctl_set_sctp_shutdown_guard_time_default(20));

	// Keep adaptive RTT-based recovery, but avoid a one-second minimum
	// retransmission delay on low-latency paths. SACK delay stays below RTO.Min.
	configure("minimum RTO", usrsctp_sysctl_set_sctp_rto_min_default(100));
	configure(
			"delayed SACK timer", usrsctp_sysctl_set_sctp_delayed_sack_time_default(20));
	configure("SACK frequency", usrsctp_sysctl_set_sctp_sack_freq_default(2));
}

inline void configurePeerTimeout(struct socket *sock, bool enabled)
{
	if (enabled) {
		// Several paths/control timers can consume the default ten retries in
		// seconds during handoff. Let checkPeerTimeouts() enforce the configured
		// receive deadline instead. Path failure detection and INIT retries keep
		// their normal limits, so alternate paths can still take over promptly.
		sctp_assocparams association = {};
		association.sasoc_assoc_id = SCTP_FUTURE_ASSOC;
		association.sasoc_asocmaxrxt = UINT16_MAX;
		if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_ASSOCINFO, &association,
					sizeof(association)) < 0)
			throw ConnectionException("Cannot configure SCTP association retry limit");
	}
}

inline void configureInterleaving(struct socket *sock)
{
	// Level 2 permits partial delivery on separate streams of one association.
	const uint32_t level = 2;
	if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_FRAGMENT_INTERLEAVE, &level,
				sizeof(level)) < 0) {
		warningstream << "SCTP: cannot enable fragment interleaving" << std::endl;
		return;
	}

	// The bundled usrsctp implements this option but omits it from its public
	// header. Keep its ABI value here rather than including private stack headers.
#ifdef SCTP_INTERLEAVING_SUPPORTED
	constexpr int interleaving_option = SCTP_INTERLEAVING_SUPPORTED;
#else
	constexpr int interleaving_option = 0x00001206;
#endif
	const sctp_assoc_value enable = {SCTP_FUTURE_ASSOC, 1};
	if (usrsctp_setsockopt(
				sock, IPPROTO_SCTP, interleaving_option, &enable, sizeof(enable)) < 0)
		warningstream << "SCTP: cannot enable I-DATA negotiation" << std::endl;
	// I-DATA is used only when the remote peer also advertises support.
}

} // namespace con_sctp

#endif
