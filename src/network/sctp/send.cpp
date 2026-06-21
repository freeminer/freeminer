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

} // namespace

#endif
