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
#include "porting.h"
#include "profiler.h"
#include "serialization.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <string_view>
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

// A route can disappear during handoff while the association is still valid.
// Let SCTP's association timers decide when the peer has actually been lost.
inline bool fmSctpRetryableError(int error)
{
	return error == EWOULDBLOCK || error == EAGAIN || error == EINTR ||
		   error == ENOBUFS || error == ENOMEM || error == ENETDOWN ||
		   error == ENETUNREACH || error == EHOSTUNREACH || error == EADDRNOTAVAIL
#ifdef EHOSTDOWN
		   || error == EHOSTDOWN
#endif
			;
}

inline void fmConfigureSctpDefaults()
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

inline void fmConfigureSctpInterleaving(struct socket *sock)
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
