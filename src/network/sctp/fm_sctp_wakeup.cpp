// SPDX-License-Identifier: GPL-3.0-or-later
#include "config.h"

#if USE_SCTP
#include "fm_sctp_wakeup.h"
#include "external/usrsctp/usrsctplib/usrsctp.h"
#include <mutex>
#include <unordered_map>

namespace con_sctp
{
namespace
{
struct Registry
{
	std::mutex mutex;
	std::unordered_map<struct socket *, std::shared_ptr<SctpWakeup>> sockets;
};

Registry &registry()
{
	// Timer threads can outlive the last Connection when stack shutdown is
	// deferred. Keep the registry valid until process exit; socket entries
	// themselves are removed on close.
	static auto *instance = new Registry;
	return *instance;
}
} // namespace

int SctpSocketWakeup::attach(
		struct socket *sock, const std::shared_ptr<SctpWakeup> &wakeup)
{
	{
		auto &r = registry();
		std::lock_guard lock(r.mutex);
		r.sockets[sock] = wakeup;
	}
	const int result = usrsctp_set_upcall(sock, onReady, nullptr);
	if (result < 0)
		detach(sock);
	return result;
}

void SctpSocketWakeup::detach(struct socket *sock)
{
	auto &r = registry();
	std::lock_guard lock(r.mutex);
	r.sockets.erase(sock);
	// Leave the static callback installed until the socket is destroyed.
	// usrsctp can already be invoking it outside the socket lock.
}

void SctpSocketWakeup::onReady(struct socket *sock, void *, int)
{
	std::shared_ptr<SctpWakeup> wakeup;
	{
		auto &r = registry();
		std::lock_guard lock(r.mutex);
		auto it = r.sockets.find(sock);
		if (it != r.sockets.end())
			wakeup = it->second;
	}
	if (wakeup)
		wakeup->notify();
}
} // namespace con_sctp
#endif
