/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/sctp/internal.h"

#if USE_SCTP

namespace con_sctp
{

void Connection::sendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	const auto lock = m_peers.lock_unique_rec();
	for (const auto &i : m_peers)
		send_(i.first, channelnum, data, reliable);
}

void Connection::send_(
		session_t peer_id, u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	if (channelnum >= CHANNEL_COUNT || !getPeer(peer_id) || !data.getSize())
		return;
	pending_sends[peer_id].push_back({channelnum, data, reliable});
}

void Connection::flushPendingSends()
{
	// Snapshot IDs because a fatal send error removes the peer and its queue.
	std::vector<session_t> peers;
	for (const auto &[id, queue] : pending_sends)
		peers.push_back(id);
	for (auto id : peers) {
		auto *peer = getPeer(id);
		if (!peer) {
			// Derived transports can remove their sockets directly.
			pending_sends.erase(id);
			continue;
		}
		if (!m_established_peers.count(id))
			continue; // Wait for COMM_UP and stream-count validation.
		auto &queue = pending_sends.at(id);
		for (unsigned budget = 0; budget < 16 && !queue.empty(); ++budget) {
			auto &message = queue.front();
			sctp_sendv_spa spa = {};
			spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
			spa.sendv_sndinfo.snd_sid = message.channel + 1 + (!message.reliable) * 4;
			if (!message.reliable) {
				// No retransmissions, but allow the initial transmission to wait
				// for congestion control instead of expiring after one millisecond.
				spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
				spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
				spa.sendv_prinfo.pr_value = 0;
				spa.sendv_sndinfo.snd_flags = SCTP_UNORDERED;
			}
			const size_t remaining = message.data.getSize() - message.offset;
			const size_t length = std::min(remaining, size_t(0xffff - 1000));
			if (length == remaining)
				spa.sendv_sndinfo.snd_flags |= SCTP_EOR;
			const auto sent = usrsctp_sendv(peer, *message.data + message.offset, length,
					nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
			if (sent < 0) {
				if (fmSctpRetryableError(errno))
					break;
				warningstream << "SCTP send failed for peer " << id << " errno=" << errno
							  << std::endl;
				deletePeer(id, false);
				break;
			}
			if (!sent)
				break;
			message.offset += sent;
			if (message.offset == message.data.getSize())
				queue.pop_front();
		}
	}
}

} // namespace con_sctp

#endif
