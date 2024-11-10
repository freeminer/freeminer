// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "networkprotocol.h"
namespace con
{

class IPeer;

class PeerHandler
{
public:
	PeerHandler() = default;
	virtual ~PeerHandler() = default;

	// Note: all functions are called from within a Receive() call on the same thread.

	/*
		This is called after the Peer has been inserted into the
		Connection's peer container.
	*/
	virtual void peerAdded(session_t peer_id) = 0;

	/*
		This is called before the Peer has been removed from the
		Connection's peer container.
	*/
	virtual void deletingPeer(session_t peer_id, bool timeout) = 0;
};

}
