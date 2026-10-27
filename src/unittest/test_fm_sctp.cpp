// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"
#include "network/sctp/connection.h"
#include "network/sctp/fm_sctp_receive.h"
#include "external/usrsctp/usrsctplib/usrsctp.h"
#include <cstring>
#include <future>

class TestFmSctp : public TestBase
{
public:
	TestFmSctp() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmSctp"; }
	void runTests(IGameDef *) override
	{
		TEST(testCoalescedWakeup);
		TEST(testWakeDuringWait);
		TEST(testCallbackRetirement);
		TEST(testCompleteMessage);
		TEST(testPartialMessages);
	}

	void testCoalescedWakeup()
	{
		con_sctp::SctpWakeup wakeup;
		for (unsigned i = 0; i < 100; ++i)
			wakeup.notify();
		UASSERT(wakeup.wait(0));
		UASSERT(!wakeup.wait(0));
		wakeup.notify();
		UASSERT(wakeup.wait(0));
	}

	void testWakeDuringWait()
	{
		con_sctp::SctpWakeup wakeup;
		std::promise<void> started;
		auto ready = started.get_future();
		auto waiter = std::async(std::launch::async, [&] {
			started.set_value();
			return wakeup.wait(1000);
		});
		ready.wait();
		wakeup.notify();
		UASSERT(waiter.get());
	}

	void testCallbackRetirement()
	{
		// Register the stack through Connection so its process-global reference
		// count remains consistent with other transport tests.
		class Stack : public con_sctp::Connection
		{
		public:
			Stack() : Connection(1350, 5.0f, true, nullptr, false) { sctp_setup(0); }
		} stack;
		const auto close = [](struct socket *peer) {
			con_sctp::SctpSocketWakeup::detach(peer);
			usrsctp_close(peer);
		};
		std::unique_ptr<struct socket, decltype(close)> peer(
				usrsctp_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP, nullptr, nullptr, 0,
						nullptr),
				close);
		UASSERT(peer);
		auto wakeup = std::make_shared<con_sctp::SctpWakeup>();
		std::weak_ptr<con_sctp::SctpWakeup> weak = wakeup;
		UASSERT(con_sctp::SctpSocketWakeup::attach(peer.get(), wakeup) == 0);
		con_sctp::SctpSocketWakeup::onReady(peer.get(), nullptr, 0);
		UASSERT(wakeup->wait(0));
		auto callbacks = std::async(std::launch::async, [&] {
			for (unsigned i = 0; i < 1000; ++i)
				con_sctp::SctpSocketWakeup::onReady(peer.get(), nullptr, 0);
		});
		con_sctp::SctpSocketWakeup::detach(peer.get());
		wakeup.reset();
		callbacks.get();
		UASSERT(weak.expired());
		// An already scheduled callback cannot revive or access retired state.
		con_sctp::SctpSocketWakeup::onReady(peer.get(), nullptr, 0);
	}

	void testCompleteMessage()
	{
		std::string partial;
		u8 data[] = {0x7f, 0xfe, 1, 2, 255};
		auto message = con_sctp::assembleSctpMessage(partial, data, sizeof(data), true);
		UASSERT(message && partial.empty());
		UASSERT(message->getSize() == sizeof(data));
		UASSERT(std::memcmp(**message, data, sizeof(data)) == 0);
		data[0] = 0;
		UASSERT((*message)[0] == 0x7f); // receive scratch storage can be reused
	}

	void testPartialMessages()
	{
		std::string partial, other_stream;
		std::string data(100000, '\0');
		for (size_t i = 0; i < data.size(); ++i)
			data[i] = static_cast<char>(i * 31);
		const auto *bytes = reinterpret_cast<const u8 *>(data.data());
		UASSERT(!con_sctp::assembleSctpMessage(partial, bytes, 60000, false));
		auto other = con_sctp::assembleSctpMessage(other_stream, bytes, 5, true);
		UASSERT(other && other->getSize() == 5 && other_stream.empty());
		UASSERT(!con_sctp::assembleSctpMessage(partial, bytes + 60000, 30000, false));
		auto message = con_sctp::assembleSctpMessage(partial, bytes + 90000, 10000, true);
		UASSERT(message && partial.empty());
		UASSERT(message->getSize() == data.size());
		UASSERT(std::memcmp(**message, bytes, data.size()) == 0);
		auto next = con_sctp::assembleSctpMessage(partial, bytes, 8, true);
		UASSERT(next && next->getSize() == 8 && partial.empty());
	}
};
static TestFmSctp g_test_instance;
