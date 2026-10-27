// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "threading/semaphore.h"
#include <atomic>

namespace con
{
// One signal can wake a receiver for several independently owned event queues.
class ReceiveSignal
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
		// Clear before scanning queues, so events arriving during the scan
		// leave a signal for the next wait.
		m_pending.store(false);
		return true;
	}

private:
	std::atomic_bool m_pending{false};
	Semaphore m_ready;
};
} // namespace con
