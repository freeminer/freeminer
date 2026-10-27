// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "networkprotocol.h" // session_t
#include "socket.h" // Address
// fm: the hostname-aware default method forwards an Address by value.
#include "address.h"
#include "fm_receive_signal.h"
#include <memory>
#include <utility>
// ===

#include <string_view>

//namespace con_ws { class Connection; }

class NetworkPacket;
//class PeerHandler;

namespace con
{
class PeerHandler;


enum rtt_stat_type : int {
	MIN_RTT,
	MAX_RTT,
	AVG_RTT,
	MIN_JITTER,
	MAX_JITTER,
	AVG_JITTER
};

enum rate_stat_type : int {
	CUR_DL_RATE,
	AVG_DL_RATE,
	CUR_INC_RATE,
	AVG_INC_RATE,
	CUR_LOSS_RATE,
	AVG_LOSS_RATE,
};

class IPeer {
public:
	// Unique id of the peer
	const session_t id;

	virtual const Address &getAddress() const = 0;

protected:
	IPeer(session_t id) : id(id) {}
	~IPeer() {}
};

class IConnection
{
public:

// fm:
	virtual size_t events_size() { return 0; }
// ==

	virtual ~IConnection() = default;

	virtual void Serve(Address bind_addr) = 0;
	virtual void Connect(Address address) = 0;
	// fm: transports with multiple paths can also use the server hostname.
	virtual void ConnectToHost(Address address, const std::string &)
	{
		Connect(address);
	}
	// ===
	virtual bool Connected() = 0;
	virtual void Disconnect() = 0;
	virtual void DisconnectPeer(session_t peer_id) = 0;

	virtual bool ReceiveTimeoutMs(NetworkPacket *pkt, u32 timeout_ms) = 0;
	bool TryReceive(NetworkPacket *pkt) {
		return ReceiveTimeoutMs(pkt, 0);
	}

	virtual void Send(session_t peer_id, u8 channelnum, NetworkPacket *pkt, bool reliable) = 0;

	//virtual session_t GetPeerID() const = 0;
	virtual Address GetPeerAddress(session_t peer_id) = 0;
	virtual float getPeerStat(session_t peer_id, rtt_stat_type type) = 0;
	virtual float getLocalStat(rate_stat_type type) = 0;
	// fm: Install once, before Serve/Connect publishes work to transport threads.
	void setReceiveSignal(std::shared_ptr<ReceiveSignal> signal)
	{
		m_receive_signal_owner = std::move(signal);
		m_receive_signal.store(m_receive_signal_owner.get(), std::memory_order_release);
	}

protected:
	void notifyReceive()
	{
		if (auto *signal = m_receive_signal.load(std::memory_order_acquire))
			signal->notify();
	}

private:
	// Outlives derived transport threads and keeps late notifications safe.
	std::shared_ptr<ReceiveSignal> m_receive_signal_owner;
	std::atomic<ReceiveSignal *> m_receive_signal{nullptr};
	// ===
};

// MTP = Minetest Protocol
IConnection *createMTP(float timeout, bool ipv6, PeerHandler *handler, bool simple_singleplayer_mode);

} // namespace
