/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/sctp/internal.h"

#if USE_SCTP

namespace con_sctp
{

void debug_printf(const char *format, ...)
{
	printf("SCTP_DEBUG: ");
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

static int conn_output(void *addr, void *buf, size_t length, uint8_t tos, uint8_t set_df)
{
	DUMP((long)addr, (long)buf, length, tos, set_df);

	char *dump_buf;
	int *fdp;

	fdp = (int *)addr;
	if (!fdp || *fdp < 0)
		return EPIPE;

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

Connection::Connection(u32 max_packet_size, float timeout, bool ipv6,
		PeerHandler *peerhandler, bool start_worker) :
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

	if (start_worker)
		start();
}

bool Connection::sctp_inited = false;
std::mutex Connection::sctp_init_mutex;
unsigned int Connection::sctp_refcount = 0;

Connection::~Connection()
{

	join();
	disconnect();

	finish_sctp();
}

void Connection::finish_sctp()
{
	const auto lock = std::unique_lock(sctp_init_mutex);
	if (!sctp_ref_registered)
		return;

	sctp_ref_registered = false;
	if (sctp_refcount > 0)
		--sctp_refcount;

	if (sctp_refcount > 0 || !sctp_inited) {
		sctp_inited_by_me = false;
		return;
	}

	for (int i = 0; i < 100; ++i) {
		if (!usrsctp_finish())
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		// cs << "Connection::~Connection() wait " << i << std::endl;
	}

	sctp_inited = false;
	sctp_inited_by_me = false;
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
		send_(c->peer_id, c->channelnum, c->data, c->reliable);
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

void Connection::onAssociationChange(
		session_t peer_id, const struct sctp_assoc_change *sac)
{
	(void)peer_id;
	(void)sac;
}

void Connection::sctp_setup(u16 port)
{
	const auto lock = std::unique_lock(sctp_init_mutex);
	if (!sctp_ref_registered) {
		++sctp_refcount;
		sctp_ref_registered = true;
	}

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


} // namespace

#endif
