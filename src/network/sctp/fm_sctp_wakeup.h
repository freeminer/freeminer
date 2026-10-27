// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "threading/semaphore.h"
#include <atomic>
#include <memory>

struct socket;

namespace con_sctp
{
class SctpWakeup
{
public:
	void notify()
	{
		if (!m_pending.exchange(true))
			m_ready.post();
	}

	bool wait(unsigned timeout_ms)
	{
		if (!m_ready.wait(timeout_ms))
			return false;
		// Clear before the worker checks its queues, preserving notifications
		// that arrive between the last queue check and the next wait.
		m_pending.store(false);
		return true;
	}

private:
	std::atomic_bool m_pending{false};
	Semaphore m_ready;
};

class SctpSocketWakeup
{
public:
	static int attach(struct socket *sock, const std::shared_ptr<SctpWakeup> &wakeup);
	static void detach(struct socket *sock);
	// Never dereferences sock or a Connection: late callbacks are harmless.
	static void onReady(struct socket *sock, void *, int);
};
} // namespace con_sctp
