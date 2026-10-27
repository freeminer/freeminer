/*
Copyright (C) 2023 proller <proler@gmail.com>
*/

#include "network/sctp/internal.h"

#if USE_SCTP

namespace con_sctp
{

void Connection::sendToAll(const ConnectionCommandPtr &command)
{
	const auto lock = m_peers.lock_unique_rec();
	for (const auto &i : m_peers)
		send_(i.first, command);
}

void Connection::send_(session_t peer_id, const ConnectionCommandPtr &command)
{
	if (command->channelnum >= CHANNEL_COUNT || !getPeer(peer_id) ||
			!command->data.getSize())
		return;
	// Keep the queued command's owning buffer through short writes and retries.
	pending_sends[peer_id].push_back({command->channelnum, command, command->reliable});
}

bool Connection::flushPendingSends()
{
	bool more_sends = false;
	// Snapshot IDs because a fatal send error removes the peer and its queue.
	std::vector<session_t> peers;
	for (const auto &[id, queue] : pending_sends)
		if (!queue.empty())
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
		bool exhausted_budget = true;
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
			const size_t remaining = message.command->data.getSize() - message.offset;
			const size_t length = std::min(remaining, size_t(0xffff - 1000));
			if (length == remaining)
				spa.sendv_sndinfo.snd_flags |= SCTP_EOR;
			const auto sent = usrsctp_sendv(peer, *message.command->data + message.offset,
					length, nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
			if (sent < 0) {
				const int error = errno;
				exhausted_budget = false;
				if (isRetryableError(error))
					break;
				warningstream << "SCTP send failed for peer " << id << " errno=" << error
							  << " (" << std::strerror(error)
							  << ") stream=" << spa.sendv_sndinfo.snd_sid
							  << " reliable=" << message.reliable
							  << " bytes=" << message.command->data.getSize()
							  << " submitted=" << message.offset
							  << " queued_messages=" << queue.size() << std::endl;
				logPaths(id, peer, "fatal send error");
				deletePeer(id, false);
				break;
			}
			if (!sent) {
				exhausted_budget = false;
				break;
			}
			message.offset += sent;
			if (message.offset == message.command->data.getSize())
				queue.pop_front();
		}
		// A fairness limit is not backpressure: keep draining in the next
		// worker turn instead of imposing an artificial millisecond delay.
		if (exhausted_budget && pending_sends.count(id) && !pending_sends.at(id).empty())
			more_sends = true;
	}
	return more_sends;
}

} // namespace con_sctp

#endif
